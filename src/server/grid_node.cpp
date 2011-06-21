#include "grid_node.h"

grid_node::grid_node(const short port, const addresses_t &addresses, const ports_t &ports,
					 const std::string& users_path, const std::string& passwd)
: io_service_(), task_executions_(), addresses_(addresses), ports_(ports), users_manager_(users_path, passwd)
{
	server_ = server_ptr(new server(io_service_, port, task_executions_, this));
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

UsersManager& grid_node::get_users_manager()
{
	return users_manager_;
}

grid_node::addresses_t& grid_node::get_addresses()
{
	return addresses_;
}

grid_node::ports_t& grid_node::get_ports()
{
	return ports_;
}
