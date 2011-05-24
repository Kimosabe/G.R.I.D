#ifndef GRID_CLIENT_H_
#define GRID_CLIENT_H_

#include "grid_node.h"
#include <boost/thread.hpp>
#include <queue>
#include <map>

class grid_task;

typedef boost::shared_ptr<boost::thread> thread_ptr;

/**
 * клиент грид-системы
 */

class grid_client{
public:
	grid_client();
	virtual ~grid_client();

	bool run(const std::vector<std::string> &addresses, const std::vector< std::stack<int> > &ports);
	void stop();

	void apply_task(const grid_task &gt);
	void remove_task(const std::string &name);

	void debug_method();

	enum task_status{EXECUTION, DONE, FAILED};

private:
	boost::asio::io_service io_serv_;

	std::vector<node_ptr> nodes_;	
	std::vector<thread_ptr> thread_pool_;

	// соответствие имени задания, номера узла, на который оно назначено и его статуса
	std::map< std::string, std::pair<size_t, task_status> > task_table_;
	// вектор всех актуальных заданий 
	std::vector<grid_task> tasks_;
};

#endif //GRID_CLIENT_H_
