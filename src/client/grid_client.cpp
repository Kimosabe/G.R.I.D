#include "grid_client.h"
#include "grid_task.h"
#include "simple_exception.hpp"
#include <iostream>
#include <utility>

grid_client::grid_client() : io_serv_(), nodes_(), thread_pool_(), task_table_(), tasks_()
{
}

void grid_client::stop()
{
	io_serv_.stop();
	using namespace boost;
	for(std::vector<thread_ptr>::iterator i = thread_pool_.begin(); i < thread_pool_.end(); ++i)
		(*i)->join();
	thread_pool_.clear();
}

bool grid_client::run(const std::vector<std::string> &addresses,
					  const std::vector< std::stack<int> > &ports)
{
	using namespace std;
	size_t nodes_num = addresses.size() < ports.size() ? addresses.size() : ports.size();
	nodes_.clear();

	vector<string>::const_iterator addr_iter = addresses.begin();
	vector< stack<int> >::const_iterator port_iter = ports.begin();

	// пробуем подключиться к каждому узлу из списка адресов используя для каждого список
	// возможных портов
	for(size_t i = 0; i < nodes_num; ++addr_iter,++port_iter,++i)
	{
		try{
			node_ptr newnode = node_ptr( new grid_node(io_serv_, *addr_iter, *port_iter) );
			nodes_.push_back( newnode );
		}
		catch(std::exception &ex){
			std::cerr << ex.what() << std::endl;
		}
	}

	thread_pool_.clear();
	thread_pool_.resize( nodes_.size() );
	vector<thread_ptr>::iterator thread_iter = thread_pool_.begin();

	// создаем столько потоков, сколько имеется узлов
	for(; thread_iter < thread_pool_.end(); ++thread_iter)
		*thread_iter = thread_ptr( new boost::thread( boost::bind(&boost::asio::io_service::run,
									&io_serv_) ) );

#if defined(_DEBUG) || defined(DEBUG)
	std::cout << nodes_.size() << " nodes total" << std::endl;
	cout.flush();
#endif

	return true;
}

grid_client::~grid_client()
{
	this->stop();
	nodes_.clear();
}

void grid_client::apply_task(const grid_task &gt)
{
	try{
		if(gt.empty()) return;
		if( task_table_.count(gt.name()) > 0 )
			throw simple_exception(std::string("Task ") + gt.name() + std::string(" already exists"));

		// TODO : распределение заданий по узлам в соответствии с загрузкой
		// в общем, job manager

		const int target_node = gt.node() == GRID_ANY_NODE ? 0 : gt.node();

		try{
			nodes_[target_node]->apply_task(gt);
			task_table_[gt.name()] = std::pair<size_t, task_status>(target_node, EXECUTION);
		}
		catch(std::exception &ex){
			task_table_[gt.name()] = std::pair<size_t, task_status>(target_node, FAILED);
			throw ex;
		}
		tasks_.push_back(gt);
	}
	catch(std::exception &ex){
		throw ex;
	}

}

void grid_client::remove_task(const std::string &name)
{
	task_table_.erase(name);
	for(std::vector<grid_task>::iterator i = tasks_.begin(); i < tasks_.end(); ++i)
		if( i->name() == name )
		{
			tasks_.erase(i);
			return;
		}
}

void grid_client::debug_method()
{
	using namespace std;
	for(vector<node_ptr>::iterator i = nodes_.begin(); i < nodes_.end(); ++i)
		if( (*i)->is_active() )
		{
			(*i)->send_file(std::string("..\\..\\..\\test\\gg.txt"), std::string("gg1.txt"));
			(*i)->request_file(std::string("gg1.txt"), std::string("gg1.txt"));
			(*i)->send_command(std::string("..\\test\\gg.exe"));
			(*i)->request_file(std::string("gg2.txt"), std::string("gg1.txt"));
		}
}
