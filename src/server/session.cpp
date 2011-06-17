#include <stack>
#include <sstream>
#include <set>

#include <msgpack.hpp>
#include <boost/thread/locks.hpp>

#include "simple_exception.hpp"
#include "session.h"
#include "server.h"
#include "grid_node.h"
#include "grid_task_execution.h"
#include "acl.h"
#include "users_manager.h"
#include "transaction.h"
#include "memory.h"

using Kimo::ACL;

extern std::string os;

session::session(boost::asio::io_service& io_service, lockable_vector<grid_task_execution_ptr> &task_executions,
				 server* parent_server)
: 	socket_(io_service), task_executions_(task_executions), file_tr_(), 
	// TODO : убрать заглушку, узнавать имя пользователя при подключении
	username_("testuser"), parent_server_(parent_server), transaction_in_progress(false)
{
}

session::~session()
{
}

boost::asio::ip::tcp::socket& session::socket()
{
	return socket_;
}


void session::start()
{
	async_read_header();
}

void session::async_read_header()
{
	// начинаем прием сообщений
	msg_size_ = 0;
	boost::asio::async_read(socket_, boost::asio::buffer((char*)&msg_size_, sizeof(msg_size_)), boost::bind(&session::handle_read_header, 
		shared_from_this(), boost::asio::placeholders::error));
}

void session::async_read_body()
{
	// начинаем прием сообщений
	memset(data_, 0, max_length);
	boost::asio::async_read(socket_, boost::asio::buffer(data_, msg_size_), boost::bind(&session::handle_read_body, 
		shared_from_this(), boost::asio::placeholders::error));
}

void session::handle_read_header(const boost::system::error_code& error)
{
	if( !error )
		async_read_body();
	else
	{
		// возникает, когда кто-то закрыл соединение, так что это типа нормально
		if( !(error == boost::asio::error::eof) )
		{
			// а все остальное странно
			std::stringstream sout;
			sout << "Disconnect " << std::endl << "Error " << error.message() << std::endl;
			std::cerr << sout.str();
		}
		// эх, а раньше тут был delete this;
		shared_from_this().reset();
	}
}

void session::handle_read_body(const boost::system::error_code& error)
{
	if (!error)
	{
		std::string request(data_, msg_size_);
		std::cout << "<request> " << request << " </request>" << std::endl;

		// запросы на получение / отправку файлов
		std::string local_name, remote_name;
		if( file_tr_.recieve_file(request, socket_) )
		{
#if defined(_DEBUG) || defined(DEBUG)
			std::cout << "file accepted\n";
#endif
		}
		else if( file_tr_.is_upload_request(request, local_name, remote_name) )
		{
			//std::cout << "upload_request : " << request << std::endl; 
			file_tr_.send_file(local_name, remote_name, socket_);
		}
		else if( apply_task_command(request) )
			;
        else if ( login_request(request) )
            ;
		else if ( token_request(request) )
			;
        else if ( transaction_begin(request) )
            ;
        else if ( transaction_transfer(request) )
            ;
        else if ( transaction_end(request) )
            ;
		else if ( user_manage_request(request) )
			;
		else if ( privilege_manage_request(request) )
			;
		// пробуем что-нибудь десериализовать
		else
		{
			msgpack::unpacked msg;
			msgpack::unpack(&msg, request.data(), request.size());

			// пробуем десериализовать объект типа grid_task
			try{
				grid_task task = msg.get().convert();
				apply_task(task);
			}
			catch(std::exception &){
				// здесь могут быть попытки десериализовать сообщения другого рода
				std::cerr << "Unknown request : " << request << std::endl;
			}
		}

		async_read_header();
	}
	else
		delete this;
}

void session::apply_task(const grid_task &task)
{
	std::string reply;

	// проверяем нет ли уже такого задания
	task_executions_.lock();
	for(std::vector<grid_task_execution_ptr>::const_iterator i = task_executions_.begin(); i < task_executions_.end(); ++i)
		if( (*i)->username() == username_ && (*i)->task().name() == task.name() )
		{
			reply = std::string("<task \"") + task.name() + std::string("\" status : already_exists>");
			break;
		}
	// если нет, то принимаем
	if( reply.empty() )
	{
		reply = std::string("<task \"") + task.name() + std::string("\" status : accepted>");
		grid_task_execution_ptr gte = grid_task_execution_ptr( new grid_task_execution(task, username_) );
		task_executions_.push_back(gte);
	}
	task_executions_.unlock();

	uint32_t msg_size = reply.size();
	boost::asio::write(socket_, boost::asio::buffer((char*)&msg_size, sizeof(msg_size)));
	boost::asio::write(socket_, boost::asio::buffer(reply.data(), reply.size()));
}

bool session::apply_task_command(const std::string &request)
{
	boost::regex re_command("(?xs)(^<task \\s+ \"(.+)\" \\s+ ([_[:alpha:][:digit:]]+)>$)");
	boost::smatch match_res;
	uint32_t msg_size;

	if( boost::regex_match(request, match_res, re_command) )
	{
		const std::string name = match_res[2], command = match_res[3];

		// запуск задания на выполнение
		if( command == std::string("run") )
		{
			bool found = false;
			task_executions_.lock();
			for(lockable_vector<grid_task_execution_ptr>::iterator i = task_executions_.begin(); i < task_executions_.end(); ++i)
				if( (*i)->task().name() == name && (*i)->username() == username_ )
				{
					(*i)->async_start();
					found = true;
					break;
				}
			task_executions_.unlock();

			// если не нашли, то пишем, что нет такого
			if( !found )
			{
				std::string reply = std::string("<task \"") + name + std::string("\" status : no_such_task>");
				msg_size = reply.size();
				boost::asio::write(socket_, boost::asio::buffer((char*)&msg_size, sizeof(msg_size)));
				boost::asio::write(socket_, boost::asio::buffer(reply.data(), reply.size()));
			}
		}

		// проверка статуса задания
		else if( command == std::string("status") )
		{
			short progress = -1;
			task_executions_.lock();
			for(lockable_vector<grid_task_execution_ptr>::iterator i = task_executions_.begin(); i < task_executions_.end(); ++i)
				if( (*i)->task().name() == name && (*i)->username() == username_ )
				{
					progress = (*i)->progress();
					break;
				}
			task_executions_.unlock();

			std::string reply;
			if( progress >= 0 )
				reply = std::string("<task \"") + name + std::string("\" status : ") +
					boost::lexical_cast<std::string>(progress) + std::string(">");
			else
				reply = std::string("<task \"") + name + std::string("\" status : no_such_task>");

			msg_size = reply.size();
			boost::asio::write(socket_, boost::asio::buffer((char*)&msg_size, sizeof(msg_size)));
			boost::asio::write(socket_, boost::asio::buffer(reply.data(), reply.size()));
		}

		// запрос на получение выходных файлов задания
		else if( command == std::string("get_result") )
		{
			// ищем данное задание
			grid_task gt;
			task_executions_.lock();
			for(lockable_vector<grid_task_execution_ptr>::iterator i = task_executions_.begin(); i < task_executions_.end(); ++i)
				if( (*i)->task().name() == name && (*i)->username() == username_ )
				{
					gt = (*i)->task();
					break;
				}
			task_executions_.unlock();

			// и пробуем отправить результаты
			if( !gt.empty() )
			{
				try{
					for(grid_task::pair_name_vector::const_iterator i = gt.output_files().begin(); i < gt.output_files().end(); ++i)
						if( this->file_tr_.send_file(i->second, i->first, socket_) == false )
							std::cerr << "Cannot return result of task " << name << " to user " <<
								username_ << " (" << i->second << ")" << std::endl;
						else
							std::cout << "file " << i->second << " sent" << std::endl;
				}
				catch( std::exception & ex ){
					std::cerr << ex.what() << std::endl;
				}
			}
		}

		// удаляем задание
		else if( command == std::string("remove") )
		{
			task_executions_.lock();
			for(lockable_vector<grid_task_execution_ptr>::iterator i = task_executions_.begin(); i < task_executions_.end(); ++i)
			if( (*i)->task().name() == name && (*i)->username() == username_ )
			{
				task_executions_.erase(i);
				break;
			}
			task_executions_.unlock();
		}

		// что-то странное, такого не должно быть
		else
		{
			std::cerr << "Unknown task command " << command << std::endl;
		}

		return true;
	}
	else
		return false;
}

bool session::login_request(const std::string &request)
{
	const boost::regex re_status("(?xs)(^<user \\s+ login \\s+ \"(.+)\" \\s+ \"([\\d\\w]+)\">$)");

	boost::smatch match_res;
	if( boost::regex_match(request, match_res, re_status) )
	{
		std::string login = match_res[2], password = match_res[3];
		int user_id = -1;
		boost::uint32_t length;

		UsersManager& users_manager_ = get_parent_server()->get_parent_node()->get_users_manager();
		if (!users_manager_.isValid(login, password, true) || (user_id = users_manager_.getId(login)) < 0)
		{
			std::string reply = std::string("<user login \"") + login + std::string("\" status \"user not found\">");
			length = reply.size();
			boost::asio::write(socket_, boost::asio::buffer(&length, sizeof(length)));
			boost::asio::write(socket_, boost::asio::buffer(reply.data(), reply.size()));
		}
		else if (users_manager_.isDenied(user_id, Kimo::ACL::PRIV_LOGIN))
		{
			std::string reply = std::string("<user login \"") + login + std::string("\" status \"user not allowed to login\">");
			length = reply.size();
			boost::asio::write(socket_, boost::asio::buffer(&length, sizeof(length)));
			boost::asio::write(socket_, boost::asio::buffer(reply.data(), reply.size()));
		}
		else
		{
			m_user_id = user_id;
			username_ = login;
			long token;
			// сгенерировать токен
			token = users_manager_.newToken(user_id);
			// синхронизировать токены на нодах
			sync_data(std::string("users"), users_manager_.getLastModified());

			std::string reply = std::string("<user login \"") + login + std::string("\" status \"ok\">");
			length = reply.size();
#if defined(_DEBUG) || defined(DEBUG)
			std::cout << "sending: " << reply << std::endl;
			size_t sended = 
#endif
			boost::asio::write(socket_, boost::asio::buffer(&length, sizeof(length)));
#if defined(_DEBUG) || defined(DEBUG)
			std::cout << "sended: " << sended << " bytes of size" << std::endl;
			sended = 
#endif
			boost::asio::write(socket_, boost::asio::buffer(reply.data(), reply.size()));
#if defined(_DEBUG) || defined(DEBUG)
			std::cout << "sended: " << sended << " bytes of reply" << std::endl;
#endif

			// выдать токен клиенту
			std::string reply1;
			reply1 = std::string("<user token \"") + boost::lexical_cast<std::string>(token) + std::string("\">");
			length = reply1.size();
			boost::asio::write(socket_, boost::asio::buffer(&length, sizeof(length)));
			boost::asio::write(socket_, boost::asio::buffer(reply1.data(), length));

            uint32_t msg_size;
            // отправляем инфу о всех имеющиеся заданиях данного юзера
            task_executions_.lock();
            for(lockable_vector<grid_task_execution_ptr>::const_iterator i = task_executions_.begin(); i < task_executions_.end(); ++i)
                if( (*i)->username() == username_ )
                {
                    std::string task_msg = std::string("<task \"") + (*i)->task().name() + std::string("\" status : ");
                    if( !(*i)->active() && !(*i)->finished() )
                        task_msg.append("accepted>");
                    else
                    {
                        short progress = (*i)->progress();
                        task_msg.append(boost::lexical_cast<std::string>(progress));
                        task_msg.append(">");
                    }
                    msg_size = task_msg.size();
                    boost::asio::write(socket_, boost::asio::buffer((char*)&msg_size, sizeof(msg_size)));
                    boost::asio::write(socket_, boost::asio::buffer(task_msg.data(), task_msg.size()));
                }
            task_executions_.unlock();

            // отправляем инфу о себе как об узле
            // ось
            std::string node_param = std::string("<node_param \"os\" : ") + os + std::string(">");
            msg_size = node_param.size();
            boost::asio::write(socket_, boost::asio::buffer((char*)&msg_size, sizeof(msg_size)));
            boost::asio::write(socket_, boost::asio::buffer(node_param.data(), node_param.size()));
		}

		return true;
	}

	return false;
}

bool session::transaction_begin(const std::string &request)
{
	const boost::regex re("(?xs)(^<transaction \\s+ \"([\\d\\w]+)\" \\s+ begin \\s+ \"(.*)\">$)");

	boost::smatch match_res;
	if( boost::regex_match(request, match_res, re) )
	{
		boost::uint32_t length;
		transaction_in_progress = true;
		std::string name = match_res[2], str_timestamp = match_res[3];
		time_t timestamp = boost::lexical_cast<time_t>(str_timestamp);

		std::string msg = std::string("<transaction \"") + name + std::string("\" status \"ok\">");

		length = msg.size();
		boost::asio::write(socket_, boost::asio::buffer(&length, sizeof(length)));
		boost::asio::write(socket_, boost::asio::buffer(msg.data(), msg.size()));

		return true;
	}

	return false;
}

bool session::transaction_transfer(const std::string &request)
{
	const boost::regex re("(?xs)(^<transaction \\s+ \"([\\d\\w]+)\" \\s+ data \\s+ \"(.*)\">$)");

	boost::smatch match_res;
	if( boost::regex_match(request, match_res, re) )
	{
		boost::uint32_t length;
		std::string name = match_res[2], data = match_res[3];
		sbuffer_.clear();
		sbuffer_.write(data.c_str(), data.size());

		std::string msg = std::string("<transaction \"") + name + std::string("\" status \"ok\">");

		length = msg.size();
		boost::asio::write(socket_, boost::asio::buffer(&length, sizeof(length)));
		boost::asio::write(socket_, boost::asio::buffer(msg.data(), msg.size()));

		return true;
	}

	return false;
}

bool session::transaction_end(const std::string &request)
{
	const boost::regex re("(?xs)(^<transaction \\s+ \"([\\d\\w]+)\" \\s+ end>$)");

	boost::smatch match_res;
	if( boost::regex_match(request, match_res, re) )
	{
		boost::uint32_t length;
		std::string name = match_res[2];
		std::string msg;

		if (get_parent_server()->get_parent_node()->get_users_manager().deserialize(sbuffer_) < 0)
			msg = std::string("<transaction \"") + name + std::string("\" status \"bad\">");
		else
			msg = std::string("<transaction \"") + name + std::string("\" status \"ok\">");

		length = msg.size();
		boost::asio::write(socket_, boost::asio::buffer(&length, sizeof(length)));
		boost::asio::write(socket_, boost::asio::buffer(msg.data(), msg.size()));

		transaction_in_progress = false;
		return true;
	}

	return false;
}

server* session::get_parent_server()
{
	return parent_server_;
}

void session::sync_data(const std::string& transaction_name, time_t timestamp)
{
	using std::set;
	using std::stack;

	grid_node::addresses_t& addresses = get_parent_server()->get_parent_node()->get_addresses();
	grid_node::ports_t& ports = get_parent_server()->get_parent_node()->get_ports();

	typedef std::vector<Kimo::Transaction> transactions_t;
	//transactions_t transactions;
	//transactions.resize(addresses.size());

	size_t size = addresses.size();
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
		port_stack = ports[i];
		while(!port_stack.empty())
		{
			port = port_stack.top();
			port_stack.pop();
			if (!transactions[i].connect(addresses[i], port))
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
	UsersManager& users_manager = get_parent_server()->get_parent_node()->get_users_manager();
	msgpack::sbuffer sbuffer;
	users_manager.serialize(sbuffer);

	// Передаем данные
	for (size_t i = 0; i < size; ++i)
	{
		if (bad_addresses.count(i) == 0)
		{
			if (transactions[i].transfer((char*)sbuffer.data(), sbuffer.size()))
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

bool session::user_manage_request(const std::string &request)
{
	const boost::regex re("(?xs)(^<user \\s+ (\\w+) \\s+ \"(.+)\" \\s+ \"(.*)\">$)");

	boost::smatch match_res;
	if( boost::regex_match(request, match_res, re) )
	{
		std::string op = match_res[2], name = match_res[3], password = match_res[4];
		std::string reply = std::string("<user ") + op + std::string(" \"") + name + std::string("\" status \"");
		UsersManager& um = get_parent_server()->get_parent_node()->get_users_manager();
		if (um.isAllowed(m_user_id, Kimo::ACL::PRIV_USERWR))
		{
			if (op == "add")
			{
				int new_user_id = um.addUser(name, password, true);
				if (new_user_id == -1)
					reply += std::string("already exist\">");
				else if (new_user_id == -2)
					reply += std::string("login is too long\">"); //Чозабред о_О
				else
				{
					reply += std::string("ok\">");
					sync_data(std::string("users"), um.getLastModified());
				}
			}
			else if (op == "remove")
			{
				int result = um.removeUser(um.getId(name));
				if (result < 0)
					reply += std::string("user not exist\">");
				else
				{
					reply += std::string("ok\">");
					sync_data(std::string("users"), um.getLastModified());
				}
			}
		}
		else
			reply += std::string("access denied\">");

		boost::uint32_t length = reply.size();
		boost::asio::write(socket_, boost::asio::buffer(&length, sizeof(length)));
		boost::asio::write(socket_, boost::asio::buffer(reply.data(), reply.size()));

		return true;
	}

	return false;
}

bool session::privilege_manage_request(const std::string &request)
{
	const boost::regex re("(?xs)(^<privilege \\s+ (\\w+) \\s+ \"(.+)\" \\s+ to \\s+ \"(.+)\">$)");

	boost::smatch match_res;
	if( boost::regex_match(request, match_res, re) )
	{
		std::string op = match_res[2], privilege = match_res[3], name = match_res[4];
		std::string reply = std::string("<privilege ") + op + std::string(" \"") + name + std::string("\" status \"");
		UsersManager& um = get_parent_server()->get_parent_node()->get_users_manager();
		if (um.isAllowed(m_user_id, Kimo::ACL::PRIV_USERWR))
		{
			int user_id = um.getId(name);
			Kimo::ACL::ACL_t privilege_num = boost::lexical_cast<Kimo::ACL::ACL_t>(privilege);
			Kimo::ACL::PRIVILEGE true_privilege = Kimo::ACL::makePrivilege(privilege_num);
			if (user_id < 0)
				reply += std::string("user not found\">");
			else if (!Kimo::ACL::isValidPrivilege(privilege_num))
				reply += std::string("invalid privilage\">");
			else if (op == "allow")
			{
				um.allow(user_id, true_privilege);
				reply += std::string("ok\">");
			}
			else if (op == "deny")
			{
				um.deny(user_id, true_privilege);
				reply += std::string("ok\">");
			}
			else
				reply += std::string("invalid privilege operation\">");

			sync_data("users", um.getLastModified());
		}
		else
			reply += std::string("access denied\">");

		boost::uint32_t length = reply.size();
		boost::asio::write(socket_, boost::asio::buffer(&length, sizeof(length)));
		boost::asio::write(socket_, boost::asio::buffer(reply.data(), reply.size()));

		return true;
	}

	return false;
}

bool session::token_request(const std::string &request)
{
	const boost::regex re("(?xs)(^<token \\s+ \"([-\\d]+)\">$)");

	boost::smatch match_res;
	if( boost::regex_match(request, match_res, re) )
	{
		long token = boost::lexical_cast<long>(match_res[2]);
		m_user_token = token;

		return true;
	}

	return false;
}
