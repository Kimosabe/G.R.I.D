#include "grid_node.h"
#include "server.h"

grid_node::grid_node(const short port) : io_service_(), task_executions_()
{
	server_ = server_ptr(new server(io_service_, port));
}

grid_node::~grid_node()
{
	stop();
}

void grid_node::stop()
{
	io_service_.stop();
}

void grid_node::run()
{	
	server_->run();
	io_service_.run();	
}
