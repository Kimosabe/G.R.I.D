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
#include "memory.h"
#include "task_list.h"
#include "kimo_crypt.h"

using Kimo::ACL;

extern std::string os;

session::session(boost::asio::io_service& io_service, lockable_vector<grid_task_execution_ptr> &task_executions,
				 server* parent_server)
: 	socket_(io_service), task_executions_(task_executions), file_tr_(), 
	parent_server_(parent_server), transaction_in_progress(false)
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
		//std::cout << "<request> " << request << " </request>" << std::endl;

		// запросы на получение / отправку файлов
		std::string local_name, remote_name;
		if ( login_request(request) )
            ;
		else if ( token_request(request) )
			;
		//else if (this->token_expired())
		//	;
		else if( file_tr_.recieve_file(request, socket_) )
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
		else if ( show_all_processes_request(request) )
			;
		else if ( kill_request(request) )
			;
		else if (timestamp_request(request) )
			;
		else if (get_request(request) )
			;
		// пробуем что-нибудь десериализовать
		else
		{
			msgpack::unpacked msg;
			msgpack::unpack(&msg, request.data(), request.size());

			// пробуем десериализовать объект типа grid_task
			try{
				grid_task task = msg.get().convert();

				UsersManager& um = get_parent_server()->get_parent_node()->get_users_manager();
				if (um.getTokenTimestamp(m_user_id) < time(NULL))
				{
					std::string reply = std::string("<task \"") + task.name() + std::string("\" status : token_expired>");
					uint32_t msg_size = reply.size();
					boost::asio::write(socket_, boost::asio::buffer((char*)&msg_size, sizeof(msg_size)));
					boost::asio::write(socket_, boost::asio::buffer(reply.data(), reply.size()));
				}
				else if (um.isAllowed(m_user_id, Kimo::ACL::PRIV_PROCEXEC))
					apply_task(task);
				else
				{
					std::string reply = std::string("<task \"") + task.name() + std::string("\" status : access_denied>");
					uint32_t msg_size = reply.size();
					boost::asio::write(socket_, boost::asio::buffer((char*)&msg_size, sizeof(msg_size)));
					boost::asio::write(socket_, boost::asio::buffer(reply.data(), reply.size()));
				}
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

		if (this->token_expired())
			return true;

		UsersManager& um = get_parent_server()->get_parent_node()->get_users_manager();
		// запуск задания на выполнение
		if( command == std::string("run") )
		{
			if ( um.isAllowed(m_user_id, Kimo::ACL::PRIV_PROCEXEC) )
			{
				bool found = false;
				task_executions_.lock();
				for(task_executions_t::iterator i = task_executions_.begin(); i < task_executions_.end(); ++i)
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
			else
			{
				std::string reply = std::string("<task \"") + name + std::string("\" status : access_denied>");
				msg_size = reply.size();
				boost::asio::write(socket_, boost::asio::buffer((char*)&msg_size, sizeof(msg_size)));
				boost::asio::write(socket_, boost::asio::buffer(reply.data(), reply.size()));
			}
		}

		// проверка статуса задания
		else if( command == std::string("status") )
		{
			short progress = -1;
			bool interrupted = false;
			task_executions_.lock();
			for(lockable_vector<grid_task_execution_ptr>::iterator i = task_executions_.begin(); i < task_executions_.end(); ++i)
				if( (*i)->task().name() == name && (*i)->username() == username_ )
				{
					progress = (*i)->progress();
					interrupted = (*i)->interrupted();
					break;
				}
			task_executions_.unlock();

			std::string reply;
			if (interrupted)
				reply = std::string("<task \"") + name + std::string("\" status : interrupted>");
			else if( progress >= 0 )
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
			boost::asio::write(socket_, boost::asio::buffer(&length, sizeof(length)));
			boost::asio::write(socket_, boost::asio::buffer(reply.data(), reply.size()));
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
					else if ((*i)->interrupted())
						task_msg.append("interrupted>");
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

		UsersManager& um = get_parent_server()->get_parent_node()->get_users_manager();
		if (name == "users")
		{
			std::string data;
			decrypt(data, sbuffer_.data(), sbuffer_.size(), um.get_passwd());
			sbuffer_.clear();
			sbuffer_.write(data.data(), data.size());
			if (um.deserialize(sbuffer_) < 0)
				msg = std::string("<transaction \"") + name + std::string("\" status \"bad\">");
			else
			{
				msg = std::string("<transaction \"") + name + std::string("\" status \"ok\">");
			}

			m_user_token = um.getToken(m_user_id);
		}
		else
			msg = "o_O"; // O_o

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
	get_parent_server()->get_parent_node()->sync_data(transaction_name, timestamp);
}

bool session::user_manage_request(const std::string &request)
{
	const boost::regex re("(?xs)(^<user \\s+ (\\w+) \\s+ \"(.+)\" \\s+ \"(.*)\">$)");

	boost::smatch match_res;
	if( boost::regex_match(request, match_res, re) )
	{
		if (this->token_expired())
			return true;

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
			else if (op == "change_password") //XXX копипаста
			{
				int id = um.getId(name);
				if (id < 0)
					reply += "user not found\">";
				else if (um.change_passwd(id, password, true) < 0)
					reply += "pasword change failed\">";
				else
				{
					reply += "ok\">";
					sync_data(std::string("users"), um.getLastModified());
				}
			}
			else
				return false;
		}
		else if (op == "change_password" && name == username_) //XXX копипаста
		{
			int id = um.getId(name);
			if (id < 0)
				reply += "user not found\">";
			else if (um.change_passwd(id, password, true) < 0)
				reply += "password change failed\">";
			else
				reply += "ok\">";
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
		if (this->token_expired())
			return true;

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
		m_user_id = get_parent_server()->get_parent_node()->get_users_manager().getId(token);
		username_ = get_parent_server()->get_parent_node()->get_users_manager().getLogin(m_user_id);

		return true;
	}

	return false;
}

bool session::token_expired()
{
	UsersManager& um = get_parent_server()->get_parent_node()->get_users_manager();
	if (um.getTokenTimestamp(m_user_id) < time(NULL))
	{
		std::string reply = std::string("<token expired>");
		boost::uint32_t msg_size = reply.size();
		boost::asio::write(socket_, boost::asio::buffer(&msg_size, sizeof(msg_size)));
		boost::asio::write(socket_, boost::asio::buffer(reply.data(), reply.size()));
		return true;
	}
	return false;
}

bool session::show_all_processes_request(const std::string &request)
{
	const boost::regex re("(?xs)(^<tasks \\s+ show_all \\s+ ([\\d]+)>$)");

	boost::smatch match_res;
	if( boost::regex_match(request, match_res, re) )
	{
		UsersManager& um = get_parent_server()->get_parent_node()->get_users_manager();
		std::string reply = std::string("<tasks show_all status \"");

		if (um.isAllowed(m_user_id, Kimo::ACL::PRIV_PROCRD))
		{
			size_t node_id = boost::lexical_cast<size_t>(match_res[2]);
			Kimo::Task task;
			Kimo::TaskList tasks;
			std::stringstream ss;
			lockable_vector<grid_task_execution_ptr>::iterator itr;
			itr = task_executions_.begin();
			for (; itr != task_executions_.end(); ++itr)
			{
				task.node_id = node_id;
				task.name = (*itr)->task().name();
				task.owner = (*itr)->username();
				if ((*itr)->active())
				{
					task.status = std::string("progress: ") + boost::lexical_cast<std::string>((*itr)->progress());
				}
				else if ((*itr)->finished())
					task.status = std::string("finished");
				else if ((*itr)->interrupted())
					task.status = std::string("interrupted");
				else
					task.status = std::string("starting");
				task.start_date = boost::posix_time::to_simple_string((*itr)->start_time());

				tasks.push_back(task);
			}

			msgpack::sbuffer sbuffer;
			msgpack::pack(sbuffer, tasks);

			reply += "ok\" data \"";
			reply.append(sbuffer.data(), sbuffer.size());
			reply += std::string("\">");
		}
		else
		{
			reply += "access denied\" data \"\">";
		}

		boost::uint32_t size = reply.size();
		boost::asio::write(socket_, boost::asio::buffer(&size, sizeof(size)));
		boost::asio::write(socket_, boost::asio::buffer(reply.data(), size));

		return true;
	}

	return false;
}

bool session::kill_request(const std::string &request)
{
	const boost::regex re("(?xs)(^<kill \\s+ \"([\\d\\w_]+)\">$)");

	boost::smatch match_res;
	if( boost::regex_match(request, match_res, re) )
	{
		std::string name = match_res[2];
		UsersManager& um = get_parent_server()->get_parent_node()->get_users_manager();
		std::string reply = std::string("<kill \"") + name + "\" status \"";
		lockable_vector<grid_task_execution_ptr>::iterator itr = task_executions_.begin();
		bool found = false;
		for (; itr != task_executions_.end(); ++itr)
		{
			if ((*itr)->task().name() == name)
			{
				found = true;
				if ((*itr)->username() == username_ || um.isAllowed(m_user_id, Kimo::ACL::PRIV_PROCTERM))
				{
					if ((*itr)->finished())
					{
						reply += "task finished\">";
					}
					else
					{
						// попытаться убить процесс
						(*itr)->interrupt();
						reply += "ok\">";
					}
				}
				else
				{
					reply += "access denied\">";
				}
				break;
			}
		}

		if (!found)
		{
			reply += "task not found\">";
		}

		boost::uint32_t size = reply.size();
		boost::asio::write(socket_, boost::asio::buffer(&size, sizeof(size)));
		boost::asio::write(socket_, boost::asio::buffer(reply.data(), size));

		return true;
	}
	return false;
}

bool session::timestamp_request(const std::string &request)
{
	const boost::regex re("(?xs)(^<timestamp \\s+ ([\\w\\d\\-_]+)>$)");

	boost::smatch match_res;
	if( boost::regex_match(request, match_res, re) )
	{
		UsersManager& um = get_parent_server()->get_parent_node()->get_users_manager();
		std::string what = match_res[2];
		time_t tmstmp;
		if (what == "users")
		{
			tmstmp = um.getLastModified();
		}
		else
			return false;

		boost::uint32_t size = sizeof(tmstmp);
		boost::asio::write(socket_, boost::asio::buffer(&size, sizeof(size)));
		boost::asio::write(socket_, boost::asio::buffer(&tmstmp, size));
		return true;
	}
	return false;
}

bool session::get_request(const std::string &request)
{
	const boost::regex re("(?xs)(^<get \\s+ ([\\d\\w\\-_]+) \\s* ([\\w\\d]*)>$)");

	boost::smatch match_res;
	if( boost::regex_match(request, match_res, re) )
	{
		UsersManager& um = get_parent_server()->get_parent_node()->get_users_manager();
		std::string what = match_res[2], buffer;
		if (what == "users")
		{
			msgpack::sbuffer sbuffer;
			um.serialize(sbuffer);
			encrypt(buffer, sbuffer.data(), sbuffer.size(), um.get_passwd());
		}
		else if (what == "acl")
		{
			std::string username = match_res[3];
			int id = um.getId(username);
			if (um.isAllowed(m_user_id, Kimo::ACL::PRIV_USERRD) || username == username_)
			{
				if (id < 0)
				{
					buffer = "<acl \"\" status \"user not found\">";
				}
				else
				{
					Kimo::ACL::ACL_t acl = um.getACL(id);
					buffer = "<acl \"";
					buffer += boost::lexical_cast<std::string>(acl);
					buffer += "\" status \"ok\">";
				}
			}
			else
			{
				buffer = "<acl \"\" status \"access denied\">";
			}
		}
		else if (what == "user_list")
		{
			if (um.isAllowed(m_user_id, Kimo::ACL::PRIV_USERRD))
			{
				Kimo::UserInfoList list;
				um.getUserList(list);
				msgpack::sbuffer sbuffer;
				msgpack::pack(sbuffer, list);
				buffer = "<users status \"ok\" list \"";
				buffer.append(sbuffer.data(), sbuffer.size());
				buffer += "\">";
			}
			else
			{
				buffer = "users status \"access denied\" list "">";
			}
		}
		else
			return false;

		boost::uint32_t size = buffer.size();
		boost::asio::write(socket_, boost::asio::buffer(&size, sizeof(size)));
		boost::asio::write(socket_, boost::asio::buffer(buffer.data(), size));
		return true;
	}
	return false;
}
