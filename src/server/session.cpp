#include "session.h"
#include "grid_task_execution.h"
#include "simple_exception.hpp"
#include "memory.h"
#include <sstream>

extern std::string os;

session::session(boost::asio::io_service& io_service, lockable_vector<grid_task_execution_ptr> &task_executions) : 
	socket_(io_service), task_executions_(task_executions), file_tr_(), msg_size_(0), 
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
		//std::cout << "<request> " << request << " </request>" << std::endl;

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