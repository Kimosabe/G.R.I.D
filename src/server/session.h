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
#include <boost/asio/streambuf.hpp>
#include "users_manager.h"
#include <msgpack.hpp>

class server;

class session : public boost::enable_shared_from_this<session>, private boost::noncopyable{
public:
	typedef lockable_vector<grid_task_execution_ptr> task_executions_t;
	session(boost::asio::io_service& io_service, lockable_vector<grid_task_execution_ptr> &task_executions,
		server* parent_server);

	virtual ~session();

	boost::asio::ip::tcp::socket& socket();

	void start();

	server* get_parent_server();

private:
	boost::asio::ip::tcp::socket socket_;	
	const static size_t max_length = 65536; //64 kb
	char data_[max_length];
	uint32_t msg_size_;

	std::string username_;

	file_transferer file_tr_;
	
	task_executions_t &task_executions_;

	void async_read_header();
	void async_read_body();
    
    void sync_data(const std::string& transaction_name, time_t timestamp);

	bool transaction_begin(const std::string &request);
	bool transaction_transfer(const std::string &request);
	bool transaction_end(const std::string &request);

    bool login_request(const std::string &request);
	bool user_manage_request(const std::string &request);
	bool privilege_manage_request(const std::string &request);
	bool token_request(const std::string &request);
	bool show_all_processes_request(const std::string &request);
	bool kill_request(const std::string &request);
	bool get_users_request(const std::string &request);
	bool timestamp_request(const std::string &request);

	bool token_expired();

	void handle_read_header(const boost::system::error_code& error);
	void handle_read_body(const boost::system::error_code& error);

	void apply_task(const grid_task &task);
	bool apply_task_command(const std::string &request);
    
    server* parent_server_;
	msgpack::sbuffer sbuffer_;
	bool transaction_in_progress;
	int m_user_id;
	long m_user_token;
	time_t m_user_token_timestamp;
};

typedef boost::shared_ptr<session> session_ptr;

#endif //_SESSION_H
