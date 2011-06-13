#include "grid_node.h"
#include "server.h"
#include <boost/thread.hpp>

server::server(boost::asio::io_service &io_service, const short port, lockable_vector<grid_task_execution_ptr> &task_executions,
			   grid_node* parent_node)
:	io_service_(io_service), acceptor_(io_service, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)),
	task_executions_(task_executions), parent_node_(parent_node)
{
}

server::~server()
{
}

void server::run()
{
	//session* new_session = new session(io_service_, task_executions_, this);
	session_ptr new_session = session_ptr(new session(io_service_, task_executions_, this));
	acceptor_.async_accept(new_session->socket(), boost::bind(&server::handle_accept, this, new_session,
			boost::asio::placeholders::error));
}

void server::handle_accept(session_ptr new_session, const boost::system::error_code& error)
{
	if (!error)
	{
		boost::thread t(boost::bind(&session::start, new_session));
		t.detach();
		new_session->start();
		new_session = session_ptr(new session(io_service_, task_executions_, this));
	}
	else
		new_session->shared_from_this().reset();

	acceptor_.async_accept(new_session->socket(), boost::bind(&server::handle_accept, this, new_session,
			boost::asio::placeholders::error));
}

grid_node* server::get_parent_node()
{
	return parent_node_;
}
