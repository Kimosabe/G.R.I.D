#include "grid_node.h"
#include <iostream>
#include <fstream>
#include <boost/lexical_cast.hpp>

using namespace boost;
using namespace boost::asio;

grid_node::grid_node(io_service &io_serv_, const std::string &address_, 
					 const std::stack<int> &ports_) : io_serv(io_serv_), socket_(io_serv_),
					 active(false), address(address_), file_transf(), streambuf_()
{
	if( ports_.size() == 0 ) return;
	std::stack<int> ports(ports_);
	int port = ports.top();
	ports.pop();
	ip::tcp::resolver resolver_(io_serv);
	ip::tcp::resolver::query query_(ip::tcp::v4(), address.data(), 
		lexical_cast<std::string>(port).data());

	system::error_code err;
	ip::tcp::resolver::iterator iter = resolver_.resolve(query_, err);

	socket_.async_connect(*iter, bind(&grid_node::handle_connect, this,
			placeholders::error, ++iter, ports));
}

void grid_node::handle_connect(const system::error_code &err,
							   ip::tcp::resolver::iterator endpoint_iterator,
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
	else if( endpoint_iterator != ip::tcp::resolver::iterator() )
	{
		ip::tcp::endpoint endpoint = *endpoint_iterator;
		socket_.async_connect(endpoint,
			bind(&grid_node::handle_connect, this,
			placeholders::error, ++endpoint_iterator, ports));
	}

	//пробуем следущий порт
	else if( !ports.empty() )
	{
		int port = ports.top();
		ports.pop();
		ip::tcp::resolver resolver_(io_serv);
		ip::tcp::resolver::query query_(ip::tcp::v4(), address.data(), 
			lexical_cast<std::string>(port).data());

		system::error_code err;
		ip::tcp::resolver::iterator iter = resolver_.resolve(query_, err);
		socket_.async_connect(*iter, bind(&grid_node::handle_connect, this,
			placeholders::error, ++iter, ports));
	}

#if defined(_DEBUG) || defined(DEBUG)
	else
		std::cerr << "Error: (handle_connect)" << err.message() << "\n";
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

bool grid_node::send_command(const std::string &command)
{
	if( !active ) return false;

	const std::string message = std::string("<command = ") + command + std::string( ">\v");
	try{
		boost::asio::write(socket_, boost::asio::buffer(message.c_str(), message.size()));
		return true;
	}
	catch( const std::exception &ex ){
		std::cerr << ex.what() << std::endl;
	}

	return false;
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
			}
		}

		this->start();
	}
	else
		active = false;
}

