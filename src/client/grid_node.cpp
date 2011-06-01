#include "grid_node.h"
#include <iostream>
#include <fstream>
#include <boost/lexical_cast.hpp>
#include "grid_task.h"

grid_node::grid_node(boost::asio::io_service &io_serv_, const std::string &address_, const std::stack<int> &ports_,
					 task_table_t &task_table, tasks_t &tasks) : 
						io_serv(io_serv_), socket_(io_serv_), active(false), address(address_),
							file_transf(), streambuf_(), task_table_(task_table), tasks_(tasks)
{
	if( ports_.size() == 0 ) return;
	std::stack<int> ports(ports_);
	int port = ports.top();
	ports.pop();
	boost::asio::ip::tcp::resolver resolver_(io_serv);
	boost::asio::ip::tcp::resolver::query query_(boost::asio::ip::tcp::v4(), address.data(), 
		boost::lexical_cast<std::string>(port).data());

	boost::system::error_code err;
	boost::asio::ip::tcp::resolver::iterator iter = resolver_.resolve(query_, err);

	socket_.async_connect(*iter, boost::bind(&grid_node::handle_connect, this,
			boost::asio::placeholders::error, ++iter, ports));
}

void grid_node::handle_connect(const boost::system::error_code &err,
							   boost::asio::ip::tcp::resolver::iterator endpoint_iterator,
							   std::stack<int> &ports)
{
	//подконнектились
	if (!err)
	{
		active = true;
		std::cout << "connected\n";
		this->start();
	}

	//следующий endpoint
	else if( endpoint_iterator != boost::asio::ip::tcp::resolver::iterator() )
	{
		boost::asio::ip::tcp::endpoint endpoint = *endpoint_iterator;
		socket_.async_connect(endpoint,
			boost::bind(&grid_node::handle_connect, this,
			boost::asio::placeholders::error, ++endpoint_iterator, ports));
	}

	//пробуем следущий порт
	else if( !ports.empty() )
	{
		int port = ports.top();
		ports.pop();
		boost::asio::ip::tcp::resolver resolver_(io_serv);
		boost::asio::ip::tcp::resolver::query query_(boost::asio::ip::tcp::v4(), address.data(), 
			boost::lexical_cast<std::string>(port).data());

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
	streambuf_.consume(streambuf_.size());
	boost::asio::async_read_until(socket_, streambuf_, '\v', boost::bind(&grid_node::handle_read,
		this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
}

bool grid_node::send_file(const std::string &local_name, const std::string &remote_name)
{
	if( !active ) return false;
	return this->file_transf.send_file(local_name, remote_name, this->socket_);
}

bool grid_node::request_file(const std::string &local_name, const std::string &remote_name)
{
	if( !active ) return false;
	return this->file_transf.request_file(local_name, remote_name, this->socket_); 
}

void grid_node::handle_read(const boost::system::error_code& error, size_t bytes_transferred)
{
	if( !error )
	{
		if( bytes_transferred > 0 )
		{
			std::istream ss(&streambuf_);
			std::string request;

			while( !ss.eof() )
			{
				std::getline(ss, request, '\v');
				if( file_transf.recieve_file(request, socket_) )
					std::cout << "file accepted" << std::endl;
				else if( parse_task_status_request(request) )
					;
				else
					std::cout << request << std::endl;
			}
		}

		this->start();
	}
	else
		active = false;
}

void grid_node::apply_task(const grid_task &task)
{
	try{
		msgpack::sbuffer sbuf;
		msgpack::pack(&sbuf, task);
		std::string message(sbuf.data(), sbuf.size());
		message.append("\v");
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
						this->file_transf.send_file(i->first, i->second, socket_);

					const std::string runmsg = std::string("<task \"") + gt.name() + std::string("\" run>\v");
					boost::asio::write(socket_, boost::asio::buffer(runmsg.data(), runmsg.size()));

					task_table_.lock();
					if( task_table_.count(gt.name()) > 0 )
						task_table_[gt.name()].change_status(task_status_record::EXECUTION, "Execution");
					task_table_.unlock();
				}
				catch( std::exception &ex ){
					std::cerr << ex.what() << std::endl;
					task_table_.lock();
					if( task_table_.count(gt.name()) > 0 )
						task_table_[gt.name()].change_status(task_status_record::FAILED, ex.what());
					task_table_.unlock();
				}
			}
		}
		// задание не принято узлом потому что уже существует задание с таким именем
		else if( status == std::string("already_exists") )
		{
			std::cout << "task " << name << " already exists" << std::endl;
		}
		// выполнение провалено
		else if( status == std::string("failed") )
		{
			task_table_.lock();
			if( task_table_.count(name) > 0 )
				task_table_[name].change_status(task_status_record::FAILED, "Failed");
			task_table_.unlock();
			std::cout << "task " << name << " execution failed " << std::endl;
		}
		// задание выполняется, (прогресс в процентах)
		else
		{
			short progress = boost::lexical_cast<short>(status);
			std::cout << "task " << name << " progress : " << progress << '%' << std::endl;
			if( progress == 100 )
			{
				if( task_table_.count(name) > 0 )
					task_table_[name].change_status(task_status_record::DONE, "Done");
				task_table_.unlock();
			}
		}

		return true;
	}
	else
		return false;
}

