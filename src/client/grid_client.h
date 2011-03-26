#ifndef GRID_CLIENT_H_
#define GRID_CLIENT_H_

#include "grid_node.h"
#include <boost/thread.hpp>
#include <queue>

class grid_task;

typedef boost::shared_ptr<boost::thread> thread_ptr;

class grid_client{
private:
	std::vector<node_ptr> nodes;
	boost::asio::io_service io_serv;
	std::vector<thread_ptr> thread_pool;

	enum task_status{QUEUED, EXECUTION, DONE, FAILED};
	enum task_type{COMMAND, FILESEND, FILERECIEVE};
public:
	grid_client();
	virtual ~grid_client();

	bool run(const std::vector<std::string> &addresses, const std::vector< std::stack<int> > &ports);
	void stop();

	bool apply_task(const grid_task &gt);

	void debug_method();
};

#endif //GRID_CLIENT_H_
