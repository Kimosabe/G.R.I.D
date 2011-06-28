#include "grid_node.h"
#include "grid_task.h"
#include "simple_exception.hpp"
#include <memory.h>
#include <iostream>
#include <fstream>
#include <boost/lexical_cast.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/read.hpp>
#include <cryptopp\ripemd.h>
#include <cryptopp\hex.h>
#include <msgpack.hpp>
#include "user_list.h"

#if defined(WIN32) || defined(WIN64)
#include <windows.h>
#endif

grid_node::grid_node(boost::asio::io_service &io_serv, const std::string &address, const std::stack<int> &ports,
					 const int number, task_table_t &task_table, tasks_t &tasks) : 
						io_serv_(io_serv), socket_(io_serv), active(false), address_(address), file_tr_(),
						number_(number), task_table_(task_table), tasks_(tasks), os_(), msg_size_(0), m_token(-1)
{
	if( ports.size() == 0 ) return;
	std::stack<int> ports_(ports);
	port_ = ports_.top();
	ports_.pop();
	boost::asio::ip::tcp::resolver resolver_(io_serv_);
	boost::asio::ip::tcp::resolver::query query_(boost::asio::ip::tcp::v4(), address_.data(), 
		boost::lexical_cast<std::string>(port_).data());

	boost::system::error_code err;
	boost::asio::ip::tcp::resolver::iterator iter = resolver_.resolve(query_, err);

	socket_.async_connect(*iter, boost::bind(&grid_node::handle_connect, this,
			boost::asio::placeholders::error, ++iter, ports_));
}

void grid_node::handle_connect(const boost::system::error_code &err,
							   boost::asio::ip::tcp::resolver::iterator endpoint_iterator,
							   std::stack<int> &ports)
{
	// подконнектились
	if (!err)
	{
		active = true;
		//std::cout << "connected\n";
		//this->start();
	}

	// следующий endpoint
	else if( endpoint_iterator != boost::asio::ip::tcp::resolver::iterator() )
	{
		boost::asio::ip::tcp::endpoint endpoint = *endpoint_iterator;
		socket_.async_connect(endpoint,
			boost::bind(&grid_node::handle_connect, this,
			boost::asio::placeholders::error, ++endpoint_iterator, ports));
	}

	// пробуем следущий порт
	else if( !ports.empty() )
	{
		port_ = ports.top();
		ports.pop();
		boost::asio::ip::tcp::resolver resolver_(io_serv_);
		boost::asio::ip::tcp::resolver::query query_(boost::asio::ip::tcp::v4(), address_.data(), 
			boost::lexical_cast<std::string>(port_).data());

		boost::system::error_code err;
		boost::asio::ip::tcp::resolver::iterator iter = resolver_.resolve(query_, err);
		socket_.async_connect(*iter, boost::bind(&grid_node::handle_connect, this,
			boost::asio::placeholders::error, ++iter, ports));
	}

#if defined(_DEBUG) || defined(DEBUG)
	else
		std::cerr << "Error: (handle_connect)" << err.message() << std::endl;
#endif

}

grid_node::~grid_node()
{
}

void grid_node::start()
{
	async_read_header();
}

void grid_node::async_read_header()
{
	msg_size_ = 0;
	boost::asio::async_read(socket_, boost::asio::buffer((char*)&msg_size_, sizeof(msg_size_)), boost::bind(&grid_node::handle_read_header,
		this, boost::asio::placeholders::error));
}

void grid_node::async_read_body()
{
	memset(data_, 0, max_length);
	boost::asio::async_read(socket_, boost::asio::buffer(data_, msg_size_), boost::bind(&grid_node::handle_read_body,
		this, boost::asio::placeholders::error));
}

void grid_node::handle_read_header(const boost::system::error_code &error)
{
	if( !error )
		async_read_body();
	else
	{
		// если соединение оборвалось само по себе, а не по вызову stop
		if( active == true )
		{
			std::stringstream sout;
			sout << "Disconnected from node " << number_ << std::endl;
			std::cout << sout.str();
			// ошибка eof возникает при закрытии соединения, так что это нормально
			if( error != boost::asio::error::eof )
			{
				std::stringstream sout;
				sout << "Error " << error.message() << std::endl;
				std::cerr << sout.str();
			}
		}
		active = false;
	}
}

void grid_node::handle_read_body(const boost::system::error_code& error)
{
	if( !error )
	{
		std::string request(data_, msg_size_);
		//std::cout << "<request> " << request << " </request>" << std::endl;

		if( file_tr_.recieve_file(request, socket_) )
			std::cout << "file accepted" << std::endl;
		else if( parse_task_status_request(request) )
			;
		else if( parse_node_param_request(request) )
			;
		else if ( parse_users_managment_request(request) )
			;
		else if ( parse_token_expired_reply(request) )
			;
		else if ( parse_show_all_processes_reply(request) )
			;
		else if ( parse_kill_reply(request) )
			;
		else if ( parse_acl_reply(request) )
			;
		else if ( parse_show_users_reply(request) )
			;
		else
			std::cout << request << std::endl;

		async_read_header();
	}
	else
		active = false;
}

bool grid_node::send_file(const std::string &local_name, const std::string &remote_name)
{
	if( !active ) return false;
	return this->file_tr_.send_file(local_name, remote_name, this->socket_);
}

bool grid_node::request_file(const std::string &local_name, const std::string &remote_name)
{
	if( !active ) return false;
	return this->file_tr_.request_file(local_name, remote_name, this->socket_); 
}

void grid_node::apply_task(const grid_task &task)
{
	try{
		msgpack::sbuffer sbuf;
		msgpack::pack(&sbuf, task);
		std::string message(sbuf.data(), sbuf.size());
		uint32_t msg_size = message.size();
		boost::asio::write(socket_, boost::asio::buffer((char*)&msg_size, sizeof(msg_size)));
		boost::asio::write(socket_, boost::asio::buffer(message.data(), message.size()));
	}
	catch(std::exception &ex){
		throw ex;
	}
}

bool grid_node::parse_task_status_request(const std::string &request)
{
	const boost::regex re_status("(?xs)(^<task \\s+ \"(.+)\" \\s+ status \\s+ : \\s+ ([_%[:alpha:][:digit:]]+)>$)");
	boost::smatch match_res;
	uint32_t msg_size;

	if( boost::regex_match(request, match_res, re_status) )
	{
		const std::string name = match_res[2], status = match_res[3];

		// задание принято, надо отправить все входные файлы и запустить
		if( status == std::string("accepted") )
		{
			grid_task gt;

			tasks_.lock();
			for(tasks_t::const_iterator i = tasks_.begin(); i < tasks_.end(); ++i)
				if( i->name() == name )
				{
					gt = *i;
					break;
				}
			tasks_.unlock();

			if( !gt.empty() )
			{
				try{
					for(grid_task::pair_name_vector::const_iterator i = gt.input_files().begin(); i < gt.input_files().end(); ++i)
						if( this->file_tr_.send_file(i->first, i->second, socket_) == false )
							throw simple_exception(std::string("Sending ") + i->first + std::string(" failed"));

					const std::string runmsg = std::string("<task \"") + gt.name() + std::string("\" run>");
					msg_size = runmsg.size();
					boost::asio::write(socket_, boost::asio::buffer((char*)&msg_size, sizeof(msg_size)));
					boost::asio::write(socket_, boost::asio::buffer(runmsg.data(), runmsg.size()));

					task_table_.lock();
					if( task_table_.count(name) > 0 )
						task_table_[name].change_status(task_status_record::EXECUTION, "Execution");
					else
						task_table_[name] = task_status_record(number_, task_status_record::EXECUTION, "Execution");
					task_table_.unlock();
				}
				catch( std::exception &ex ){
					std::cerr << ex.what() << std::endl;
					task_table_.lock();
					if( task_table_.count(name) > 0 )
						task_table_[name].change_status(task_status_record::FAILED, ex.what());
					else
						task_table_[name] = task_status_record(number_, task_status_record::FAILED, ex.what());
					task_table_.unlock();
				}
			}
		}

		// задание не принято узлом потому что уже существует задание с таким именем
		else if( status == std::string("already_exists") )
		{
			std::cout << "task " << name << " already exists" << std::endl;
			task_table_.lock();
			if( task_table_.count(name) > 0 )
				task_table_[name].change_status(task_status_record::NOTACCEPTED, "Already exists");
			else
				task_table_[name] = task_status_record(number_, task_status_record::NOTACCEPTED, "Already exists");
			task_table_.unlock();
		}
		// и такое бывает
		else if( status == std::string("no_such_task") )
		{
			std::cout << "task " << name << " not found by node" << std::endl;
			if( task_table_.count(name) > 0 )
				task_table_[name].change_status(task_status_record::NOTACCEPTED, "Not found by node");
			else
				task_table_[name] = task_status_record(number_, task_status_record::NOTACCEPTED, "Not found by node");
			task_table_.unlock();
		}
		// выполнение провалено
		else if( status == std::string("failed") )
		{
			task_table_.lock();
			if( task_table_.count(name) > 0 )
				task_table_[name].change_status(task_status_record::FAILED, "Failed");
			else
				task_table_[name] = task_status_record(number_, task_status_record::FAILED, "Failed");
			task_table_.unlock();
			std::cout << "task " << name << " execution failed " << std::endl;
		}
		// задание было прервано
		else if (status == "interrupted")
		{
			task_table_.lock();
			if( task_table_.count(name) > 0 )
				task_table_[name].change_status(task_status_record::INTERRUPTED, "Interrupted");
			else
				task_table_[name] = task_status_record(number_, task_status_record::INTERRUPTED, "Interrupted");
			task_table_.unlock();
			std::cout << "task " << name << " execution interrupted " << std::endl;
		}
		// выполнение запрещено
		else if (status == "access_denied")
		{
			task_table_.lock();
			if( task_table_.count(name) > 0 )
				task_table_[name].change_status(task_status_record::ACCESS_DENIED, "Access denied");
			else
				task_table_[name] = task_status_record(number_, task_status_record::ACCESS_DENIED, "Access denied");
			task_table_.unlock();
			std::cout << "task " << name << " access denied " << std::endl;
		}
		// пользователю нужно перезайти
		else if (status == "token_expired")
		{
			task_table_.lock();
			if( task_table_.count(name) > 0 )
				task_table_.erase(task_table_.find(name));
			task_table_.unlock();
			std::cout << "task " << name << " need to relogin " << std::endl;

			typedef CryptoPP::RIPEMD256 HASHER;
			// LOGIN
			HASHER hasher;
			CryptoPP::HexEncoder encoder;
			std::string username;
			std::string password;
			char buffer[HASHER::DIGESTSIZE/* + 1*/];
			bool incorrect_login;
			do
			{
				incorrect_login = false;
				std::cout << "login: ";
				std::getline(std::cin, username);
				if (username.empty())
				{
					std::cout << "login is empty" << std::endl;
					incorrect_login = true;
					continue;
				}
				std::cout << "password: ";
				std::getline(std::cin, password);

				// хэшируем пароль
				hasher.CalculateDigest((byte*)buffer, (const byte*)password.c_str(), password.size());
				password.clear();
				encoder.Attach( new CryptoPP::StringSink( password ) );
				encoder.Put( (byte*)buffer, HASHER::DIGESTSIZE );
				encoder.MessageEnd();
			} while (incorrect_login || !login(username, password));
			// LOGIN
		}
		// задание выполняется, (прогресс в процентах)
		else
		{
			try{
				short progress = boost::lexical_cast<short>(status);
				std::cout << "task " << name << " progress : " << progress << '%' << std::endl;
				if( progress == 100 )
				{
					task_table_.lock();
					if( task_table_.count(name) > 0 )
						task_table_[name].change_status(task_status_record::DONE, "Done");
					else
						task_table_[name] = task_status_record(number_, task_status_record::DONE, "Done");
					task_table_.unlock();
				}
				else if( progress >= 0 )
				{
					std::string smessage = std::string("Execution : ") + status + std::string("%");
					task_table_.lock();
					if( task_table_.count(name) > 0 )
						task_table_[name].change_status(task_status_record::EXECUTION, smessage);
					else
						task_table_[name] = task_status_record(number_, task_status_record::EXECUTION, smessage);
					task_table_.unlock();
				}
			}
			catch( std::exception & ex ){
				std::cerr << "Unexpected : " << ex.what() << " with task status " << status
					<< std::endl;
			}
		}

		return true;
	}
	else
		return false;
}

bool grid_node::parse_node_param_request(const std::string &request)
{
	const boost::regex re_nparam("(?xs)(^<node_param \\s+ \"(.+)\" \\s+ : \\s+ ([_%[:alpha:][:digit:]]+)>$)");
	boost::smatch match_res;

	if( boost::regex_match(request, match_res, re_nparam) )
	{
		const std::string pname = match_res[2], pvalue = match_res[3];
		if( pname == std::string("os") )
			os_ = pvalue;
		else
			std::cerr << "unknown node param " << pname << " = " << pvalue << std::endl;

		return true;
	}
	else
		return false;
}

void grid_node::remove_task(const std::string &name)
{
	std::string request = std::string("<task \"") + name + std::string("\" remove>");
	uint32_t msg_size = request.size();
	boost::asio::write(socket_, boost::asio::buffer((char*)&msg_size, sizeof(msg_size)));
	boost::asio::write(socket_, boost::asio::buffer(request.data(), request.size()));
}

void grid_node::get_result(const std::string &name)
{
	std::string request = std::string("<task \"") + name + std::string("\" get_result>");
	uint32_t msg_size = request.size();
	boost::asio::write(socket_, boost::asio::buffer((char*)&msg_size, sizeof(msg_size)));
	boost::asio::write(socket_, boost::asio::buffer(request.data(), request.size()));
}

void grid_node::refresh_status(const std::string &name)
{
	std::string request = std::string("<task \"") + name + std::string("\" status>");
	uint32_t msg_size = request.size();
	boost::asio::write(socket_, boost::asio::buffer((char*)&msg_size, sizeof(msg_size)));
	boost::asio::write(socket_, boost::asio::buffer(request.data(), request.size()));
}


bool grid_node::login(std::string& login, std::string& password)
{
	std::string request = std::string("<user login \"") + login + std::string("\" \"") + password +
		std::string("\">");
	boost::uint32_t length;
	length = request.size();
	boost::asio::write(socket_, boost::asio::buffer(&length, sizeof(length)));
	boost::asio::write(socket_, boost::asio::buffer(request.data(), request.size()));

	//streambuf_.consume(streambuf_.size());
	size_t bytes_transferred = boost::asio::read(socket_, boost::asio::buffer(&length, sizeof(length)));

	boost::scoped_array<char> buffer(new char[length+1]);
	bytes_transferred = boost::asio::read(socket_, boost::asio::buffer(buffer.get(), length));
	buffer[length] = '\0';

	if( bytes_transferred > 0 )
	{
		//std::istream ss(&streambuf_);
		std::string request; request.append(buffer.get(), length);
		//std::cout << "<request> " << request << " </request>" << std::endl;

		//if( !ss.eof() )
		{
			const boost::regex re_status("(?xs)(^<user \\s+ login \\s+ \"(.+)\" \\s+ status \\s+ \"(.+)\">$)");
			//std::getline(ss, request, '\v');

			boost::smatch match_res;
			if( boost::regex_match(request, match_res, re_status) )
			{
				std::string received_login = match_res[2], status = match_res[3];

				if (received_login != login)
					std::cerr << "can't login: wrong login received" << std::endl;
				else if (status != "ok")
					std::cerr << "can't login: " << status << std::endl;
				else
				{
					boost::asio::read(socket_, boost::asio::buffer(&length, sizeof(length)));
					buffer.reset(new char[length+1]);
					buffer[length] = '\0';
					boost::asio::read(socket_, boost::asio::buffer(buffer.get(), length));
					request = buffer.get();
					//std::cout << "<request> " << request << " </request>" << std::endl;
					if (!grid_node::parse_token_request(request))
					{
						std::cerr << "wrong token" << std::endl;
						return false;
					}
					return true;
				}
			}
		}
	}
	
	return false;
}

void grid_node::stop()
{
	active = false;
	socket_.close();
}

void grid_node::add_user(const std::string& name, const std::string& password)
{
	std::string request = std::string("<user add \"") + name + std::string("\" \"") + password + std::string("\">");
	boost::uint32_t msg_size = request.size();

	boost::asio::write(socket_, boost::asio::buffer(&msg_size, sizeof(msg_size)));
	boost::asio::write(socket_, boost::asio::buffer(request.data(), request.size()));
}

void grid_node::remove_user(const std::string& name)
{
	std::string request = std::string("<user remove \"") + name + std::string("\" \"\">");
	boost::uint32_t msg_size = request.size();

	boost::asio::write(socket_, boost::asio::buffer(&msg_size, sizeof(msg_size)));
	boost::asio::write(socket_, boost::asio::buffer(request.data(), request.size()));
}

bool grid_node::parse_users_managment_request(const std::string& request)
{
	const boost::regex re_status("(?xs)(^<(\\w+) \\s+ (\\w+) \\s+ \"(.+)\" \\s+ status \\s+ \"([\\d\\s\\w]+)\">$)");
	boost::smatch match_res;

	if( boost::regex_match(request, match_res, re_status) )
	{
		const std::string type = match_res[2], op = match_res[3], name = match_res[4], status = match_res[5];

		if (status != "ok")
		{
			std::cerr << type << " operation \"" << op << "\" on user \"" << name << "\" failed: " << status << std::endl;
		}
		else
		{
			std::cout << type << " " << op << " complited" << std::endl;
		}

		return true;
	}

	return false;
}

void grid_node::allow_privilege(const std::string& name, const Kimo::ACL::ACL_t privilege)
{
	std::string str_privilege = boost::lexical_cast<std::string>(privilege);
	std::string request = std::string("<privilege allow \"") + str_privilege
		+ std::string("\" to \"") + name + std::string("\">");
	boost::uint32_t msg_size = request.size();

	boost::asio::write(socket_, boost::asio::buffer(&msg_size, sizeof(msg_size)));
	boost::asio::write(socket_, boost::asio::buffer(request.data(), request.size()));
}

void grid_node::deny_privilege(const std::string& name, const Kimo::ACL::ACL_t privilege)
{
	std::string str_privilege = boost::lexical_cast<std::string>(privilege);
	std::string request = std::string("<privilege deny \"") + str_privilege
		+ std::string("\" to \"") + name + std::string("\">");
	boost::uint32_t msg_size = request.size();

	boost::asio::write(socket_, boost::asio::buffer(&msg_size, sizeof(msg_size)));
	boost::asio::write(socket_, boost::asio::buffer(request.data(), request.size()));
}

bool grid_node::parse_token_request(const std::string &request)
{
	const boost::regex re_status("(?xs)(^<user \\s+ token \\s+ \"([-\\d]+)\">$)");
	boost::smatch match_res;

	if( boost::regex_match(request, match_res, re_status) )
	{
		std::string token_str = match_res[2];
		long token = boost::lexical_cast<long>(token_str);

		if (token < 0)
		{
			std::cerr << "wrong token received" << std::endl;
		}

		m_token = token;

		return true;
	}

	return false;
}

bool grid_node::parse_token_expired_reply(const std::string &reply)
{
	const boost::regex re_status("(?xs)(^<token \\s+ expired>$)");
	boost::smatch match_res;

	if( boost::regex_match(reply, match_res, re_status) )
	{
		typedef CryptoPP::RIPEMD256 HASHER;
		// LOGIN
		HASHER hasher;
		CryptoPP::HexEncoder encoder;
		std::string username;
		std::string password;
		char buffer[HASHER::DIGESTSIZE/* + 1*/];
		bool incorrect_login;
		do
		{
			incorrect_login = false;
			std::cout << "login: ";
			std::getline(std::cin, username);
			if (username.empty())
			{
				std::cout << "login is empty" << std::endl;
				incorrect_login = true;
				continue;
			}
			std::cout << "password: ";
			std::getline(std::cin, password);

			// хэшируем пароль
			hasher.CalculateDigest((byte*)buffer, (const byte*)password.c_str(), password.size());
			password.clear();
			encoder.Attach( new CryptoPP::StringSink( password ) );
			encoder.Put( (byte*)buffer, HASHER::DIGESTSIZE );
			encoder.MessageEnd();
		} while (incorrect_login || !login(username, password));
		// LOGIN

		return true;
	}

	return false;
}

long grid_node::getToken()
{
	return m_token;
}

void grid_node::setToken(long token)
{
	m_token = token;

	std::string message = std::string("<token \"") + boost::lexical_cast<std::string>(m_token) + std::string("\">");
	uint32_t msg_size = message.size();
	boost::asio::write(socket_, boost::asio::buffer(&msg_size, sizeof(msg_size)));
	boost::asio::write(socket_, boost::asio::buffer(message.data(), message.size()));
}

void grid_node::all_tasks_request(size_t node_id)
{
	std::string request;
	request = std::string("<tasks show_all ") + boost::lexical_cast<std::string>(node_id) + std::string(">");
	boost::uint32_t size = request.size();
	
	boost::asio::write(socket_, boost::asio::buffer(&size, sizeof(size)));
	boost::asio::write(socket_, boost::asio::buffer(request.data(), size));
}

bool grid_node::parse_show_all_processes_reply(const std::string &reply)
{
	const boost::regex re_status("(?xs)(^<tasks \\s+ show_all \\s+ status \\s+ \"(.+)\" \\s+ data \\s+ \"(.*)\">$)");
	boost::smatch match_res;

	if( boost::regex_match(reply, match_res, re_status) )
	{
		std::string status = match_res[2], data = match_res[3];
		if (status == "ok")
		{
			if (data.empty())
				std::cout << "Warning: process data is empty." << std::endl;
			else
			{
				msgpack::unpacked unpacked;
				msgpack::unpack(&unpacked, data.data(), data.size());
				m_tasks_mutex.lock();
				m_tasks.clear();
				unpacked.get().convert(&m_tasks);
				m_tasks_mutex.unlock();

				// o_O
				std::cout << address_ << ":" << port_ << ": tasks received" << std::endl;
			}
		}
		else
		{
			std::cout << address_ << ":" << port_ << ": can't get all processes data: " << status << std::endl;
		}

		return true;
	}

	return false;
}

void grid_node::get_tasks(Kimo::TaskList &tasks)
{
	m_tasks_mutex.lock();
	tasks.insert(tasks.end(), m_tasks.begin(), m_tasks.end());
	m_tasks_mutex.unlock();
}

void grid_node::kill(const std::string& task_name)
{
	std::string request;
	request = std::string("<kill \"") + task_name + "\">";
	boost::uint32_t size = request.size();
	
	boost::asio::write(socket_, boost::asio::buffer(&size, sizeof(size)));
	boost::asio::write(socket_, boost::asio::buffer(request.data(), size));
}

bool grid_node::parse_kill_reply(const std::string &reply)
{
	const boost::regex re_status("(?xs)(^<kill \\s+ \"([\\w\\d_]+)\" \\s+ status \\s+ \"(.+)\">$)");
	boost::smatch match_res;

	if( boost::regex_match(reply, match_res, re_status) )
	{
		std::string name = match_res[2], status = match_res[3];
		if (status == "ok")
		{
			std::cout << "task \"" << name << "\" killed" << std::endl;
		}
		else
		{
			std::cout << "can't kill task \"" << name << "\": " << status << std::endl;
		}
		return true;
	}
	return false;
}

// вспомогательная функция для вывода доступных привилегий на экран
static void acl_print(Kimo::ACL::ACL_t acl, const char* spriv, Kimo::ACL::PRIVILEGE priv)
{
	std::cout << spriv << " is ";
	if (acl & priv)
	{
#if defined(WIN32) || defined(WIN64)
		SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 2);
#endif
		std::cout << "allowed" << std::endl;
#if defined(WIN32) || defined(WIN64)
		SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 7);
#endif
	}
	else
	{
#if defined(WIN32) || defined(WIN64)
		SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 4);
#endif
		std::cout << "denied" << std::endl;
#if defined(WIN32) || defined(WIN64)
		SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 7);
#endif
	}
}

bool grid_node::parse_acl_reply(const std::string &reply)
{
	const boost::regex re_status("(?xs)(^<acl \\s+ \"(.*)\" \\s+ status \\s+ \"(.+)\">$)");
	boost::smatch match_res;

	if( boost::regex_match(reply, match_res, re_status) )
	{
		std::string str_acl = match_res[2], status = match_res[3];
		if (status == "ok")
		{
			Kimo::ACL::ACL_t acl = boost::lexical_cast<Kimo::ACL::ACL_t>(str_acl);

			acl_print(acl, "  login", Kimo::ACL::PRIV_LOGIN);
			acl_print(acl, "  process execution", Kimo::ACL::PRIV_PROCEXEC);
			acl_print(acl, "  reading info about all processes", Kimo::ACL::PRIV_PROCRD);
			acl_print(acl, "  killing any process", Kimo::ACL::PRIV_PROCTERM);
			acl_print(acl, "  reading info about all users", Kimo::ACL::PRIV_USERRD);
			acl_print(acl, "  editing any user profile", Kimo::ACL::PRIV_USERWR);
		}
		else
		{
			std::cout << "can't get acl: " << status << std::endl;
		}

		return true;
	}
	return false;
}

void grid_node::get_acl(const std::string& username)
{
	if (username.empty())
	{
		std::cout << "username is empty" << std::endl;
		return;
	}
	std::string request = "<get acl ";
	request += username; request += ">";
	boost::uint32_t size = request.size();
	
	boost::asio::write(socket_, boost::asio::buffer(&size, sizeof(size)));
	boost::asio::write(socket_, boost::asio::buffer(request.data(), size));
}

void grid_node::change_password(const std::string &username, const std::string &passwd)
{
	std::string request = "<user change_password \"";
	request += username; request += "\" \"";
	request += passwd; request += "\">";
	boost::uint32_t size = request.size();
	
	boost::asio::write(socket_, boost::asio::buffer(&size, sizeof(size)));
	boost::asio::write(socket_, boost::asio::buffer(request.data(), size));
}

void grid_node::show_users()
{
	std::string request = "<get user_list>";
	boost::uint32_t size = request.size();
	
	boost::asio::write(socket_, boost::asio::buffer(&size, sizeof(size)));
	boost::asio::write(socket_, boost::asio::buffer(request.data(), size));
}

bool grid_node::parse_show_users_reply(const std::string &reply)
{
	const boost::regex re_status("(?xs)(^<users \\s+ status \\s+ \"(.+)\" \\s+ list \\s+ \"(.*)\">$)");
	boost::smatch match_res;

	if( boost::regex_match(reply, match_res, re_status) )
	{
		std::string status = match_res[2], data = match_res[3];
		if (status == "ok")
		{
			msgpack::unpacked unpacked;
			msgpack::unpack(&unpacked, data.data(), data.size());
			Kimo::UserInfoList user_list;
			unpacked.get().convert(&user_list);

			Kimo::UserInfoList::iterator itr = user_list.begin();
			std::cout << "#\tusername" << std::endl;
			for (; itr != user_list.end(); ++itr)
			{
				std::cout << itr->id << "\t" << itr->name << std::endl;
			}
		}
		else
		{
			std::cout << "can't get user list: " << status << std::endl;
		}

		return true;
	}
	return false;
}
