#include "session.h"
#include <sstream>

session::session(boost::asio::io_service& io_service) : socket_(io_service), file_tr(), 
	child_prcosses(), streambuf_()
{
}

session::~session()
{
	//TODO: kill them all
	child_prcosses.clear();
}

boost::asio::ip::tcp::socket& session::socket()
{
	return socket_;
}

void session::start()
{
	streambuf_.consume(streambuf_.size());
	boost::asio::async_read_until(socket_, streambuf_, '\n',
		boost::bind(&session::handle_read, this, boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred));
}

void session::handle_read(const boost::system::error_code& error, size_t bytes_transferred)
{
	if (!error)
	{
		if( bytes_transferred > 0 )
		{
			//const std::string request(data_, bytes_transferred);
			std::stringstream ss;
			ss << &streambuf_;
			//ss.getline(this->data_, max_length);
			const std::string request(ss.str(), 0, ss.str().size() - 1);
			std::string local_name, remote_name;

			std::cout << "<request> " << request << " </request>" << std::endl;

			//TODO: here must be normal message parser
			/*
			if( file_tr.recieve_file(request, socket_) )
				std::cout << "file accepted\n";
			else if( file_tr.is_upload_request(request, local_name, remote_name) )
			{
				std::cout << "upload_request : " << request << std::endl; 
				file_tr.send_file(local_name, remote_name, socket_);
			}
			else if( accept_command(request) )
				std::cout << "command : " << request << std::endl;
			else
				std::cout << "Unknown : " << request << std::endl;
			*/
			
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

bool session::accept_command(const std::string &request)
{
	const static boost::regex re_command("(?x)(^<command \\s+ = \\s+ (.+)>$)");
	boost::smatch match_res;

	if( !boost::regex_match(request, match_res, re_command, boost::regex_constants::match_not_dot_newline) )
		return false;

	std::cout << "command = " << match_res[2] << std::endl;

	const pid_t pid = execute(match_res[2]);

	if( is_invalid_pid(pid) ){
		//а вот хз пока что делать
		std::cerr << "Error : execution \"" << match_res[2] << "\" failed\n";
	}
	else{
		this->child_prcosses.push_back(pid);
	}

	return true;
}