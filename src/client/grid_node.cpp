#include "grid_node.h"
#include "grid_task.h"
#include "simple_exception.hpp"
#include <memory.h>
#include <iostream>
#include <fstream>
#include <boost/lexical_cast.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/read.hpp>

grid_node::grid_node(boost::asio::io_service &io_serv, const std::string &address, const std::stack<int> &ports,
					 const int number, task_table_t &task_table, tasks_t &tasks) : 
						io_serv_(io_serv), socket_(io_serv), active(false), address_(address), file_tr_(),
						number_(number), task_table_(task_table), tasks_(tasks), os_(), msg_size_(0)
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
	std::string request = std::string("<user \"") + login + std::string("\" \"") + password +
		std::string("\" login>");
	boost::uint32_t length;
	length = request.size();
	boost::asio::write(socket_, boost::asio::buffer(&length, sizeof(length)));
	boost::asio::write(socket_, boost::asio::buffer(request.data(), request.size()));

	//streambuf_.consume(streambuf_.size());
	size_t bytes_transferred = boost::asio::read(socket_, boost::asio::buffer(&length, sizeof(length)));
#if defined(_DEBUG) || defined(DEBUG)
	std::cout << "received: " << bytes_transferred << " bytes for buffer" << std::endl;
#endif

	boost::scoped_array<char> buffer(new char[length]);
	bytes_transferred = boost::asio::read(socket_, boost::asio::buffer(buffer.get(), length));
#if defined(_DEBUG) || defined(DEBUG)
	std::cout << "received: " << bytes_transferred << " bytes of login response" << std::endl;
#endif

	if( bytes_transferred > 0 )
	{
		//std::istream ss(&streambuf_);
		std::string request; request.append(buffer.get(), length);

		//if( !ss.eof() )
		{
			const boost::regex re_status("(?xs)(^<user \\s+ \"(.+)\" \\s+ \"(.+)\" \\s+ token \\s+ (-?\\d+)>$)");
			//std::getline(ss, request, '\v');

			boost::smatch match_res;
			if( boost::regex_match(request, match_res, re_status) )
			{
				std::string received_login = match_res[2], status = match_res[3];
				long token = boost::lexical_cast<long>(match_res[4]);

				if (received_login != login)
					std::cerr << "can't login: wrong login received" << std::endl;
				else if (status != "accepted")
					std::cerr << "can't login: " << status << std::endl;
				else if (token < 0)
					std::cerr << "can't login: wrong token" << std::endl;
				else
				{
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


