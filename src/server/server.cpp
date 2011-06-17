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
	session_ptr new_session = session_ptr(new session(io_service_, task_executions_, this));
	acceptor_.async_accept(new_session->socket(), boost::bind(&server::handle_accept, this, new_session,
			boost::asio::placeholders::error));
}

void server::handle_accept(session_ptr new_session, const boost::system::error_code& error)
{
	if (!error)
	{
		/*std::string address = new_session->socket().remote_endpoint().address().to_string();
		unsigned short port = new_session->socket().remote_endpoint().port();

		grid_node::addresses_t& addresses = get_parent_node()->get_addresses();
		grid_node::ports_t& ports = get_parent_node()->get_ports();
		size_t i;

		for (i = 0; i < addresses.size(); ++i)
		{
			if (addresses[i] == address)
			{
				std::stack<int> port_stack = ports[i];
				std::find(port_stack.begin(), port_stack.end(), port);
			}
		}*/

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
