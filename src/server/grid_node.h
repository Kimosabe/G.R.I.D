#ifndef _GRID_NODE_H
#define _GRID_NODE_H

#if defined(_WIN32) || defined(_WIN64) || defined(WIN32) || defined(WIN64)

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif //_WIN32_WINNT

#endif //Windows

#include "lockable_vector.hpp"
#include "server.h"
#include <stack>
#include <boost/shared_ptr.hpp>
#include <boost/asio.hpp>
#include <boost/noncopyable.hpp>

class grid_node : private boost::noncopyable {
public:
	typedef boost::shared_ptr<boost::thread> thread_ptr;

	grid_node(const short port, const std::vector<std::string> &addresses, const std::vector< std::stack<int> > &ports);
	virtual ~grid_node();

	void run();
	void wait();
	void stop();

	typedef lockable_vector<grid_task_execution_ptr> task_executions_t;
private:
	server_ptr server_;
	task_executions_t task_executions_;
	boost::asio::io_service io_service_;
	std::string os_;

	// адреса других узлов сети
	std::vector<std::string> addresses_;
	std::vector< std::stack<int> > ports_;

	std::vector<thread_ptr> thread_pool_;
};

typedef boost::shared_ptr<grid_node> grid_node_ptr;

#endif //_GRID_NODE_H
