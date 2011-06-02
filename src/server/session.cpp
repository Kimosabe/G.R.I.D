#include "session.h"
#include "grid_task_execution.h"
#include <sstream>

session::session(boost::asio::io_service& io_service, lockable_vector<grid_task_execution_ptr> &task_executions) : 
	socket_(io_service), task_executions_(task_executions), file_tr(), streambuf_(), 
	// TODO : убрать заглушку, узнавать имя пользователя при подключении
	username_("testuser")
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
	streambuf_.consume(streambuf_.size());
	boost::asio::async_read_until(socket_, streambuf_, '\v', boost::bind(&session::handle_read, 
		this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
}

void session::handle_read(const boost::system::error_code& error, size_t bytes_transferred)
{
	if (!error)
	{
		if( bytes_transferred > 0 )
		{
			std::istream ss(&streambuf_);

			while( !ss.eof() )
			{
				std::string request;
				std::getline(ss, request, '\v');
	
				if( !request.empty() )
				{
					//std::cout << "<request> " << request << " </request>" << std::endl;

					// запросы на получение / отправку файлов

					std::string local_name, remote_name;
					if( file_tr.recieve_file(request, socket_) )
					{
						std::cout << "file accepted\n";
						continue;
					}
					else if( file_tr.is_upload_request(request, local_name, remote_name) )
					{
						std::cout << "upload_request : " << request << std::endl; 
						file_tr.send_file(local_name, remote_name, socket_);
						continue;
					}
					else if( apply_task_command(request) )
						continue;
					// пробуем что-нибудь десериализовать
					else
					{
						msgpack::unpacked msg;
						msgpack::unpack(&msg, request.data(), request.size());

						// пробуем десериализовать объект типа grid_task
						try{
							grid_task task = msg.get().convert();
							apply_task(task);
							continue;
						}
						catch(const std::exception &){
						}

						// здесь могут быть попытки десериализовать сообщения другого рода
					}

					std::cout << "Unknown : " << request << std::endl;
				}
			}
		}
		this->start();
	}
	else
		delete this;
}

void session::handle_write(const boost::system::error_code& error)
{
	if (!error)
		this->start();
	else
		delete this;
}

void session::apply_task(const grid_task &task)
{
	std::cout << "applying " << task.name() << std::endl;
	std::cout.flush();

	std::string reply;

	task_executions_.lock();
	for(std::vector<grid_task_execution_ptr>::const_iterator i = task_executions_.begin(); i < task_executions_.end(); ++i)
		if( (*i)->username() == username_ && (*i)->task().name() == task.name() )
		{
			reply = std::string("<task \"") + task.name() + std::string("\" status : already_exists>\v");
			break;
		}
	if( reply.empty() )
	{
		reply = std::string("<task \"") + task.name() + std::string("\" status : accepted>\v");
		grid_task_execution_ptr gte = grid_task_execution_ptr( new grid_task_execution(task, username_) );
		task_executions_.push_back(gte);
	}
	task_executions_.unlock();

	boost::asio::write(socket_, boost::asio::buffer(reply.data(), reply.size()));
}

bool session::apply_task_command(const std::string &request)
{
	boost::regex re_command("(?xs)(^<task \\s+ \"(.+)\" \\s+ ([_[:alpha:][:digit:]]+)>$)");
	boost::smatch match_res;

	if( boost::regex_match(request, match_res, re_command) )
	{
		const std::string name = match_res[2], command = match_res[3];

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

			if( !found )
			{
				std::string reply = std::string("<task \"") + name + std::string("\" status : no_such_task>\v");
				boost::asio::write(socket_, boost::asio::buffer(reply.data(), reply.size()));
			}
		}
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
					boost::lexical_cast<std::string>(progress) + std::string(">\v");
			else
				reply = std::string("<task \"") + name + std::string("\" status : no_such_task>\v");

			boost::asio::write(socket_, boost::asio::buffer(reply.data(), reply.size()));
		}
		else
		{
			std::cerr << "Unknown task command " << command << std::endl;
		}

		return true;
	}
	else
		return false;
}