#include "grid_node.h"

grid_node::grid_node(const short port, const std::vector<std::string> &addresses, const std::vector< std::stack<int> > &ports) : io_service_(), 
	task_executions_(), addresses_(addresses), ports_(ports), thread_pool_()
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
	for(std::vector<thread_ptr>::iterator i = thread_pool_.begin(); i < thread_pool_.end(); ++i)
		(*i)->join();
}

void grid_node::wait()
{
	for(std::vector<thread_ptr>::iterator i = thread_pool_.begin(); i < thread_pool_.end(); ++i)
		(*i)->join();
}

void grid_node::run()
{	
	server_->run();
	for(size_t i = 0; i < addresses_.size()+1; ++i)
	{
		thread_ptr t = thread_ptr(new boost::thread(boost::bind(&boost::asio::io_service::run, &io_service_)));
		thread_pool_.push_back(t);
	}
	//io_service_.run();	
}
