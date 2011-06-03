#include "grid_node.h"

grid_node::grid_node(const short port, const std::vector<std::string> &addresses, const std::vector< std::stack<int> > &ports) : io_service_(), 
	task_executions_(), addresses_(addresses), ports_(ports)
{
	server_ = server_ptr(new server(io_service_, port, task_executions_));
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
