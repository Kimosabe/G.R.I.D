#ifndef _SESSION_H
#define _SESSION_H

#include "file_transferer.h"
#include "exec.h"
#include <boost/asio/streambuf.hpp>

class session{
private:
	boost::asio::ip::tcp::socket socket_;
	//64 kb
	const static size_t max_length = 65536;
	char data_[max_length];

	boost::asio::streambuf streambuf_;

	file_transferer file_tr;
	std::vector<pid_t> child_prcosses;
	//TODO: task queue / task manager

	//TODO: replace with task accepter
	bool accept_command(const std::string &request);
public:
	session(boost::asio::io_service& io_service);
	virtual ~session();

	boost::asio::ip::tcp::socket& socket();

	void start();

	void handle_read(const boost::system::error_code& error, size_t bytes_transferred);
	void handle_write(const boost::system::error_code& error);
};

#endif //_SESSION_H
