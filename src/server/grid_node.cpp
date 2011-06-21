#include <deque>
#include <stack>
#include <time.h>
#include <msgpack.hpp>
#include "grid_node.h"
#include "kimo_crypt.h"
#include "transaction.h"

grid_node::grid_node(const short port, const addresses_t &addresses, const ports_t &ports,
					 const std::string& users_path, const std::string& passwd)
: io_service_(), task_executions_(), addresses_(addresses), ports_(ports), users_manager_(users_path, passwd)
{
	server_ = server_ptr(new server(io_service_, port, task_executions_, this));
}

grid_node::~grid_node()
{
	stop();
}

void grid_node::stop()
{
	io_service_.stop();
}

void grid_node::run()
{
	std::cout << "Timestamp: " << users_manager_.getLastModified() << std::endl;
	check_timestamps();
	server_->run();
	io_service_.run();	
}

UsersManager& grid_node::get_users_manager()
{
	return users_manager_;
}

grid_node::addresses_t& grid_node::get_addresses()
{
	return addresses_;
}

grid_node::ports_t& grid_node::get_ports()
{
	return ports_;
}

void grid_node::check_timestamps()
{
	msgpack::sbuffer sbuffer;
	std::string request = "<timestamp users>", data;
	boost::uint32_t length = request.size();
	time_t timestamp;
	boost::asio::io_service io_service;
	boost::asio::ip::tcp::socket socket(io_service);
	std::stack<int> port_stack;
	size_t size = addresses_.size();
	int port;
	bool connected, gotcha = false;

	std::cout << "verifying data";

	for (size_t i = 0; i < size; ++i)
	{
		std::cout << ".";
		connected = false;
		port_stack = ports_[i];
		while(!port_stack.empty() && !connected)
		{
			port = port_stack.top();
			port_stack.pop();
			boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::address::from_string(addresses_[i]), port);
			boost::system::error_code err;

			socket.connect(endpoint, err);

			if (!err)
				connected = true;
		}

		if (connected)
		{
			boost::asio::write(socket, boost::asio::buffer(&length, sizeof(length)));
			boost::asio::write(socket, boost::asio::buffer(request.data(), length));

			boost::asio::read(socket, boost::asio::buffer(&length, sizeof(length)));
			//boost::scoped_array<char> buffer(new char[size]);
			//boost::asio::read(socket, boost::asio::buffer(buffer.get(), size));
			boost::asio::read(socket, boost::asio::buffer(&timestamp, sizeof(time_t)));

			if (timestamp > users_manager_.getLastModified())
			{
				std::cout << "me old";
				request = "<get users>";
				length = request.size();
				boost::asio::write(socket, boost::asio::buffer(&length, sizeof(length)));
				boost::asio::write(socket, boost::asio::buffer(request.data(), length));

				boost::asio::read(socket, boost::asio::buffer(&length, sizeof(length)));
				boost::scoped_array<char> buffer(new char[length]);
				boost::asio::read(socket, boost::asio::buffer(buffer.get(), length));

				data.clear();
				decrypt(data, buffer.get(), length, users_manager_.get_passwd());
				sbuffer.write(data.data(), data.size());
				if (users_manager_.deserialize(sbuffer) < 0)
					std::cout << "can't deserialize users data from " << addresses_[i] << ":" << port << std::endl;
			}
			else if (timestamp < users_manager_.getLastModified())
			{
				gotcha = true;
			}

			socket.close();
		}
	}

	if (gotcha)
	{
		sync_data("users", users_manager_.getLastModified());
	}

	std::cout << "Done" << std::endl;
}

void grid_node::sync_data(const std::string& transaction_name, time_t timestamp)
{
	using std::set;
	using std::stack;

	typedef std::vector<Kimo::Transaction> transactions_t;
	//transactions_t transactions;
	//transactions.resize(addresses.size());

	size_t size = addresses_.size();
	Kimo::Transaction* transactions = new Kimo::Transaction[size];
	
	std::set<size_t> bad_addresses;
	std::stack<int> port_stack;

	bool connected;
	short port;
	//transactions_t::iterator itr = transactions.begin();
	// Подключение ко всем нодам
	for (size_t i = 0; i < size; ++i)
	{
		connected = false;
		transactions[i].set_name(transaction_name.c_str());
		port_stack = ports_[i];
		while(!port_stack.empty())
		{
			port = port_stack.top();
			port_stack.pop();
			if (!transactions[i].connect(addresses_[i], port))
			{
				connected = true;
				break;
			}
		}

		if (!connected)
			bad_addresses.insert(i);
	}

	// Начало транзакции со всеми нодами, к которым смогли подключиться
	for (size_t i = 0; i < size; ++i)
	{
		if (bad_addresses.count(i) == 0)
		{
			if (transactions[i].begin(timestamp))
				bad_addresses.insert(i);
		}
	}

	// Формируем данные для транзакции
	msgpack::sbuffer sbuffer;
	users_manager_.serialize(sbuffer);
	std::string data;
	encrypt(data, sbuffer.data(), sbuffer.size(), users_manager_.get_passwd());

	// Передаем данные
	for (size_t i = 0; i < size; ++i)
	{
		if (bad_addresses.count(i) == 0)
		{
			if (transactions[i].transfer(data.data(), data.size()))
				bad_addresses.insert(i);
		}
	}

	// Завершаем транзакцию
	for (size_t i = 0; i < size; ++i)
	{
		if (bad_addresses.count(i) == 0)
		{
			if (transactions[i].end())
				bad_addresses.insert(i);
		}
	}

	size_t complited_transactions = size - bad_addresses.size();
	std::cerr << "complited: " << complited_transactions << " / " << size << std::endl;

	delete [] transactions;
}
