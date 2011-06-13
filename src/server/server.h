#ifndef _SERVER_H
#define _SERVER_H

#include "session.h"
#include <vector>
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/noncopyable.hpp>

class server : private boost::noncopyable {
public:
	server(boost::asio::io_service& io_service, const short port, lockable_vector<grid_task_execution_ptr> &task_executions);
	virtual ~server();

	void run();
private:
	boost::asio::io_service& io_service_;
	boost::asio::ip::tcp::acceptor acceptor_;

	lockable_vector<grid_task_execution_ptr> &task_executions_;

	//void handle_accept(session* new_session, const boost::system::error_code& error);
	void handle_accept(session_ptr new_session, const boost::system::error_code& error);
};

typedef boost::shared_ptr<server> server_ptr;

#endif //_SERVER_H
