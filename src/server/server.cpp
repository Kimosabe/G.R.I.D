#include "server.h"

server::server(boost::asio::io_service &io_service, const short port, lockable_vector<grid_task_execution_ptr> &task_executions,
			   UsersManager& users_manager)
:	io_service_(io_service), acceptor_(io_service, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)),
	task_executions_(task_executions), users_manager_(users_manager)
{
}

server::~server()
{
}

void server::run()
{
	session* new_session = new session(io_service_, task_executions_, users_manager_);
	acceptor_.async_accept(new_session->socket(), boost::bind(&server::handle_accept, this, new_session,
			boost::asio::placeholders::error));
}

void server::handle_accept(session* new_session, const boost::system::error_code& error)
{
	if (!error)
	{
		new_session->start();
		new_session = new session(io_service_, task_executions_, users_manager_);
	}

	acceptor_.async_accept(new_session->socket(), boost::bind(&server::handle_accept, this, new_session,
			boost::asio::placeholders::error));
}
