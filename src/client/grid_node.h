#ifndef GRID_NODE_H_
#define GRID_NODE_H_

#if defined(_WIN32) || defined(_WIN64) || defined(WIN32) || defined(WIN64)

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif //_WIN32_WINNT

#endif //Windows

#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/noncopyable.hpp>
#include <vector>
#include <string>
#include <stack>
#include "file_transferer.h"
#include "lockable_map.hpp"
#include "lockable_vector.hpp"
#include "task_status_record.h"

class grid_task;

/**
  * соединение с одним из узлов грид системы
  */

class grid_node : private boost::noncopyable {
public:
	typedef lockable_map<std::string, task_status_record> task_table_t;
	typedef lockable_vector<grid_task> tasks_t;

	grid_node(boost::asio::io_service& io_serv, const std::string &address, 
		const std::stack<int> &ports, const int number, task_table_t &task_table, tasks_t &tasks);
	virtual ~grid_node();

	bool is_active() const			{ return active; };
	const std::string& os() const	{ return os_; };

	bool send_file(const std::string &local_name, const std::string &remote_name);
	bool request_file(const std::string &local_name, const std::string &remote_name);

	void remove_task(const std::string &name);
	void get_result(const std::string &name);
	void refresh_status(const std::string &name);
	bool login(std::string& login, std::string& password);
	void add_user(const std::string& name, const std::string& password);

	void apply_task(const grid_task &task);

    void stop();
	void start();

private:
	boost::asio::io_service &io_serv_;
	boost::asio::ip::tcp::socket socket_;
	std::string address_, os_;
	int port_;

	file_transferer file_tr_;

	const static size_t max_length = 65536; //64 kb
	char data_[max_length];
	uint32_t msg_size_;

	int number_;	// порядковый номер данного узла для клиента
	bool active;

	// ссылка на таблицу с данными о выполнении заданий
	lockable_map<std::string, task_status_record> &task_table_;
	// ссылка на вектор принятых заданий
	lockable_vector<grid_task> &tasks_;

	const static size_t maxsize = 4096;
	char buf[maxsize];

	void handle_connect(const boost::system::error_code& err,
		boost::asio::ip::tcp::resolver::iterator endpoint_iterator,
		std::stack<int> &ports);

	void async_read_header();
	void async_read_body();
	void handle_read_header(const boost::system::error_code& error);
	void handle_read_body(const boost::system::error_code& error);

	bool parse_task_status_request(const std::string &request);
	bool parse_node_param_request(const std::string &request);
	bool parse_user_request(const std::string &request);
};

typedef boost::shared_ptr<grid_node> node_ptr;

#endif //GRID_NODE_H_
