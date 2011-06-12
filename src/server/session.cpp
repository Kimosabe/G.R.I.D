#include "session.h"
#include "grid_task_execution.h"
#include "simple_exception.hpp"
#include <sstream>
#include "acl.h"

using Kimo::ACL;

extern std::string os;

session::session(boost::asio::io_service& io_service, lockable_vector<grid_task_execution_ptr> &task_executions,
				 UsersManager& users_manager)
: 	socket_(io_service), task_executions_(task_executions), file_tr(), streambuf_(), 
	// TODO : убрать заглушку, узнавать имя пользователя при подключении
	username_("testuser"), users_manager_(users_manager)
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
	async_read();
}

void session::async_read()
{
	// начинаем прием сообщений
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
						//std::cout << "upload_request : " << request << std::endl; 
						file_tr.send_file(local_name, remote_name, socket_);
						continue;
					}
					else if( apply_task_command(request) )
						continue;
					else if ( login_request(request) )
					{
						continue;
					}
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
						catch(std::exception &){
						}

						// здесь могут быть попытки десериализовать сообщения другого рода
					}

					std::cerr << "Unknown request : " << request << std::endl;
				}
			}
		}
		this->async_read();
	}
	else
		delete this;
}

void session::handle_write(const boost::system::error_code& error)
{
	if (!error)
		this->async_read();
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
			reply = std::string("<task \"") + task.name() + std::string("\" status : already_exists>\v");
			break;
		}
	// если нет, то принимаем
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
				std::string reply = std::string("<task \"") + name + std::string("\" status : no_such_task>\v");
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
					boost::lexical_cast<std::string>(progress) + std::string(">\v");
			else
				reply = std::string("<task \"") + name + std::string("\" status : no_such_task>\v");

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
						if( this->file_tr.send_file(i->second, i->first, socket_) == false )
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
	const boost::regex re_status("(?xs)(^<user \\s+ \"(.+)\" \\s+ \"([\\d\\w]+)\" \\s+ login>$)");

	boost::smatch match_res;
	if( boost::regex_match(request, match_res, re_status) )
	{
		std::string login = match_res[2], password = match_res[3];
		int user_id = -1;

		if (!users_manager_.isValid(login, password, true) || (user_id = users_manager_.getId(login)) < 0)
		{
			std::string reply = std::string("<user \"") + login + std::string("\" \"user not found\" token -1>\v");
			boost::asio::write(socket_, boost::asio::buffer(reply.data(), reply.size()));
		}
		else if (users_manager_.isDenied(user_id, Kimo::ACL::PRIV_LOGIN))
		{
			std::string reply = std::string("<user \"") + login + std::string("\" \"user not allowed to login\" token -1>\v");
			boost::asio::write(socket_, boost::asio::buffer(reply.data(), reply.size()));
		}
		else
		{
			long token;
			// сгенерировать токен
			token = users_manager_.newToken(user_id);
			// синхронизировать токены на нодах
			;
			// выдать токен клиенту
			std::string reply = std::string("<user \"") + login + std::string("\" \"accepted\" token ")
				+ boost::lexical_cast<std::string>(token) + std::string(">\v");
			boost::asio::write(socket_, boost::asio::buffer(reply.data(), reply.size()));

			// отправляем инфу о всех имеющиеся заданиях данного юзера
			task_executions_.lock();
			for(lockable_vector<grid_task_execution_ptr>::const_iterator i = task_executions_.begin(); i < task_executions_.end(); ++i)
				if( (*i)->username() == username_ )
				{
					std::string task_msg = std::string("<task \"") + (*i)->task().name() + std::string("\" status : ");
					if( !(*i)->active() && !(*i)->finished() )
						task_msg.append("accepted>\v");
					else
					{
						short progress = (*i)->progress();
						task_msg.append(boost::lexical_cast<std::string>(progress));
						task_msg.append(">\v");
					}
					boost::asio::write(socket_, boost::asio::buffer(task_msg.data(), task_msg.size()));
				}
			task_executions_.unlock();

			// отправляем инфу о себе как об узле
			// ось
			std::string node_param = std::string("<node_param \"os\" : ") + os + std::string(">\v");
			boost::asio::write(socket_, boost::asio::buffer(node_param.data(), node_param.size()));
		}

		return true;
	}

	return false;
}
