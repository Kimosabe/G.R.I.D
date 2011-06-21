#ifndef _GRID_NODE_H
#define _GRID_NODE_H

#if defined(_WIN32) || defined(_WIN64) || defined(WIN32) || defined(WIN64)

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif //_WIN32_WINNT

#endif //Windows

#include <stack>
#include <string>
#include <boost/shared_ptr.hpp>
#include <boost/asio.hpp>
#include <boost/noncopyable.hpp>
#include "lockable_vector.hpp"
#include "grid_task_execution.h"
#include "server.h"
#include "users_manager.h"

class grid_node : private boost::noncopyable {
public:
	typedef std::vector<std::string> addresses_t;
	typedef std::vector< std::stack<int> > ports_t;
	typedef lockable_vector<grid_task_execution_ptr> task_executions_t;

	grid_node(const short port, const addresses_t &addresses, const ports_t &ports,
		const std::string& users_path, const std::string& passwd);
	virtual ~grid_node();

	void run();
	void stop();
	UsersManager& get_users_manager();
	addresses_t& get_addresses();
	ports_t& get_ports();
private:
	server_ptr server_;
	task_executions_t task_executions_;
	boost::asio::io_service io_service_;
	std::string os_;
	// адреса других узлов сети
	addresses_t addresses_;
	ports_t ports_;
	// мэнеджер пользователей
	UsersManager users_manager_;
};

typedef boost::shared_ptr<grid_node> grid_node_ptr;

#endif //_GRID_NODE_H
