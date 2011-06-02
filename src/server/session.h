#ifndef _SESSION_H
#define _SESSION_H

#include "file_transferer.h"
#include "exec.h"
#include "grid_task.h"
#include "grid_task_execution.h"
#include "lockable_vector.hpp"
#include <boost/asio/streambuf.hpp>

class session{
private:
	boost::asio::ip::tcp::socket socket_;	
	const static size_t max_length = 65536; //64 kb
	char data_[max_length];
	boost::asio::streambuf streambuf_;
	std::string username_;

	file_transferer file_tr;
	
	lockable_vector<grid_task_execution_ptr> &task_executions_;

	void async_read();

	void apply_task(const grid_task &task);
	bool apply_task_command(const std::string &request);
public:
	session(boost::asio::io_service& io_service, lockable_vector<grid_task_execution_ptr> &task_executions);
	virtual ~session();

	boost::asio::ip::tcp::socket& socket();

	void start();

	void handle_read(const boost::system::error_code& error, size_t bytes_transferred);
	void handle_write(const boost::system::error_code& error);
};

#endif //_SESSION_H
