#ifndef _SERVER_H
#define _SERVER_H

#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/noncopyable.hpp>

#include "session.h"
#include "users_manager.h"

class grid_node;

class server : private boost::noncopyable {
public:
	server(boost::asio::io_service& io_service, const short port, lockable_vector<grid_task_execution_ptr> &task_executions,
		grid_node* parent_node);
	virtual ~server();

	grid_node* get_parent_node();

	void run();
private:
	boost::asio::io_service& io_service_;
	boost::asio::ip::tcp::acceptor acceptor_;

	lockable_vector<grid_task_execution_ptr> &task_executions_;

	void handle_accept(session* new_session, const boost::system::error_code& error);
	grid_node* parent_node_;
};

typedef boost::shared_ptr<server> server_ptr;

#endif //_SERVER_H
