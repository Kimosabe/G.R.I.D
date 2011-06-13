#ifndef _SESSION_H
#define _SESSION_H

#include "file_transferer.h"
#include "exec.h"
#include "grid_task.h"
#include "grid_task_execution.h"
#include "lockable_vector.hpp"
#include "int_types.h"
#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/noncopyable.hpp>


class session : public boost::enable_shared_from_this<session>, private boost::noncopyable {
public:
	session(boost::asio::io_service& io_service, lockable_vector<grid_task_execution_ptr> &task_executions);
	virtual ~session();

	boost::asio::ip::tcp::socket& socket();

	void start();

private:
	boost::asio::ip::tcp::socket socket_;	
	const static size_t max_length = 65536; //64 kb
	char data_[max_length];
	uint32_t msg_size_;

	std::string username_;

	file_transferer file_tr_;
	
	lockable_vector<grid_task_execution_ptr> &task_executions_;

	void async_read_header();
	void async_read_body();

	void handle_read_header(const boost::system::error_code& error);
	void handle_read_body(const boost::system::error_code& error);

	void apply_task(const grid_task &task);
	bool apply_task_command(const std::string &request);
};

typedef boost::shared_ptr<session> session_ptr;

#endif //_SESSION_H
