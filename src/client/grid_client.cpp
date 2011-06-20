#include <iostream>
#include <utility>
#include <boost/asio/io_service.hpp>
#include "grid_client.h"
#include "grid_task.h"
#include "simple_exception.hpp"
#include "task_list.h"

grid_client::grid_client()
: io_serv_(), nodes_(), thread_pool_(), task_table_(), tasks_(), work_(io_serv_)
{
}

void grid_client::stop()
{
	for(std::vector<node_ptr>::iterator i = nodes_.begin(); i < nodes_.end(); ++i)
		(*i)->stop();
	io_serv_.stop();
	using namespace boost;
	for(std::vector<thread_ptr>::iterator i = thread_pool_.begin(); i < thread_pool_.end(); ++i)
		//(*i)->interrupt();
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
	for(int i = 0; i < nodes_num; ++addr_iter,++port_iter,++i)
	{
		try{
			node_ptr newnode = node_ptr( new grid_node(io_serv_, *addr_iter, *port_iter, i, task_table_, tasks_) );
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
		//*thread_iter = thread_ptr( new boost::thread( boost::bind(&boost::asio::io_service::run,
		//							&io_serv_) ) );
		*thread_iter = thread_ptr( new boost::thread( boost::bind(&grid_client::io_service_run, this) ) );

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
			task_table_.lock();
			task_table_[gt.name()] = task_status_record(target_node, task_status_record::SENDING, std::string("Sending"));
			task_table_.unlock();
		}
		catch(std::exception &ex){
			task_table_.lock();
			task_table_[gt.name()] = task_status_record(target_node, task_status_record::FAILED, ex.what());
			task_table_.unlock();
			throw ex;
		}

		tasks_.lock();
		tasks_.push_back(gt);
		tasks_.unlock();
	}
	catch(std::exception &ex){
		std::cerr << ex.what() << std::endl;
	}

}

void grid_client::remove_task(const std::string &name)
{
	task_table_.lock();
	// если задание принято каким-либо узлом, отправляем ему запрос на удаление
	if( task_table_.count(name) > 0 )
		if( task_table_[name].status() != task_status_record::NOTACCEPTED )
			nodes_[task_table_[name].node()]->remove_task(name);
	task_table_.erase(name);
	task_table_.unlock();

	tasks_.lock();
	for(std::vector<grid_task>::iterator i = tasks_.begin(); i < tasks_.end(); ++i)
		if( i->name() == name )
		{
			tasks_.erase(i);
			break;
		}
	tasks_.unlock();
}

const std::string grid_client::task_status_message(const std::string &taskname) const
{
	std::string res;

	task_table_.lock();

	std::map<std::string, task_status_record>::const_iterator it = task_table_.find(taskname);
	if( it != task_table_.end() )
		res = it->second.status_message();

	task_table_.unlock();

	return res;
}

void grid_client::get_result(const std::string &name)
{
	task_table_.lock();
	// если задание принято каким-либо узлом, просим вернуть результат выполнения
	if( task_table_.count(name) > 0 )
	{
		if( task_table_[name].status() != task_status_record::NOTACCEPTED )
			nodes_[task_table_[name].node()]->get_result(name);
	}
	else
		std::cout << "Task " << name << " not found" << std::endl;
	task_table_.unlock();
}

void grid_client::refresh_status(const std::string &name)
{
	task_table_.lock();
	// если задание принято каким-либо узлом, просим вернуть результат выполнения
	if( task_table_.count(name) > 0 )
	{
		if( task_table_[name].status() != task_status_record::NOTACCEPTED )
			nodes_[task_table_[name].node()]->refresh_status(name);
	}
	else
		std::cout << "Task " << name << " not found" << std::endl;
	task_table_.unlock();
}

void grid_client::tasks(grid_client::pair_string_vector &res) const
{
	task_table_.lock();
	for(lockable_map<std::string, task_status_record>::const_iterator i = task_table_.begin(); i != task_table_.end(); ++i)
		res.push_back(std::pair<std::string, std::string>(i->first, i->second.status_message()));
	task_table_.unlock();
}

bool grid_client::login(std::string& username, std::string& password)
{
	bool active_found = false;
	if (nodes_.size())
	{
		Nodes::iterator itr = nodes_.begin();
		for (; itr != nodes_.end(); ++itr)
		{
			if ((*itr)->is_active())
			{
				active_found = true;
				if ((*itr)->login(username, password))
				{
					user_token = (*itr)->getToken();
					// если залогинились, запускаем взаимодецствие со всеми нодами.
					std::vector<node_ptr>::iterator itr;
					for (itr = nodes_.begin(); itr != nodes_.end(); ++itr)
					{
						(*itr)->setToken(user_token);
						(*itr)->start();
					}
					return true;
				}
			}
		}
	}

	if (!active_found)
		std::cerr << "No nodes are available" << std::endl;

	return false;
}

void grid_client::add_user(const std::string& name, const std::string& password)
{
	if (nodes_.size())
	{
		Nodes::iterator itr = nodes_.begin();
		for (; itr != nodes_.end(); ++itr)
		{
			if ((*itr)->is_active())
			{
				(*itr)->add_user(name, password);
				return;
			}
		}
	}

	std::cerr << "no nodes are active" << std::endl;
}

void grid_client::remove_user(const std::string& name)
{
	if (nodes_.size())
	{
		Nodes::iterator itr = nodes_.begin();
		for (; itr != nodes_.end(); ++itr)
		{
			if ((*itr)->is_active())
			{
				(*itr)->remove_user(name);
				return;
			}
		}
	}

	std::cerr << "no nodes are active" << std::endl;
}

void grid_client::allow_privilege(const std::string& name, const Kimo::ACL::ACL_t privilege)
{
	if (nodes_.size())
	{
		Nodes::iterator itr = nodes_.begin();
		for (; itr != nodes_.end(); ++itr)
		{
			if ((*itr)->is_active())
			{
				(*itr)->allow_privilege(name, privilege);
				return;
			}
		}
	}

	std::cerr << "no nodes are active" << std::endl;
}

void grid_client::deny_privilege(const std::string& name, const Kimo::ACL::ACL_t privilege)
{
	if (nodes_.size())
	{
		Nodes::iterator itr = nodes_.begin();
		for (; itr != nodes_.end(); ++itr)
		{
			if ((*itr)->is_active())
			{
				(*itr)->deny_privilege(name, privilege);
				return;
			}
		}
	}

	std::cerr << "no nodes are active" << std::endl;
}

void grid_client::io_service_run()
{
	try
	{
		io_serv_.run();
	}
	catch(std::exception& ex)
	{
		std::cout << "O_o: " << ex.what() << std::endl;
	}
}

void grid_client::request_all_processes()
{
	Nodes::iterator itr = nodes_.begin();
	for (size_t i = 0; itr != nodes_.end(); ++itr, ++i)
	{
		if ((*itr)->is_active())
			(*itr)->all_tasks_request(i);
	}
}

void grid_client::show_all_processes()
{
	Kimo::TaskList::iterator jtr;

	Nodes::iterator itr = nodes_.begin();
	for (size_t i = 0; itr != nodes_.end(); ++itr, ++i)
	{
		(*itr)->get_tasks(m_tasks);
	}

	jtr = m_tasks.begin();
	std::cout << "#\tnode id\tname\towner\tstatus\t\tstart at" << std::endl;
	for (size_t i = 0; jtr != m_tasks.end(); ++jtr, ++i)
		std::cout << i << "\t" << jtr->node_id << "\t" << jtr->name << "\t"
			<< jtr->owner << "\t" << jtr->status << "\t" << jtr->start_date << std::endl;
}

void grid_client::kill(size_t process_num)
{
	if (!m_tasks.size())
	{
		std::cout << "no tasks known, make request first." << std::endl;
		return;
	}

	if (process_num >= m_tasks.size())
	{
		std::cout << "no such task." << std::endl;
		return;
	}

	if (m_tasks[process_num].node_id >= nodes_.size())
	{
		std::cout << "ERROR: wrong node_id in task description." << std::endl;
		return;
	}

	nodes_[m_tasks[process_num].node_id]->kill(m_tasks[process_num].name);
}

void grid_client::kill(const std::string &task_name)
{
	lockable_map<std::string, task_status_record>::iterator i;
	if ((i = task_table_.find(task_name)) != task_table_.end())
	{
		int node_id = i->second.node();
		if (node_id < nodes_.size())
		{
			nodes_[node_id]->kill(task_name);
		}
	}
}
