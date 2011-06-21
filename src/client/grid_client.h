#ifndef GRID_CLIENT_H_
#define GRID_CLIENT_H_

#include "grid_node.h"
#include "lockable_map.hpp"
#include "lockable_vector.hpp"
#include "task_status_record.h"
#include "acl.h"
#include <boost/thread.hpp>

typedef boost::shared_ptr<boost::thread> thread_ptr;

/**
 * ������ ����-�������
 */

class grid_client{
public:
	typedef std::vector< std::pair<std::string, std::string> > pair_string_vector;
	grid_client();
	virtual ~grid_client();

	bool run(const std::vector<std::string> &addresses, const std::vector< std::stack<int> > &ports);
	void stop();

	void apply_task(const grid_task &gt);
	void remove_task(const std::string &name);
	void get_result(const std::string &name);
	void refresh_status(const std::string &name);
	bool login(std::string& username, std::string& password);
	void add_user(const std::string& name, const std::string& password);
	void remove_user(const std::string& name);
	void allow_privilege(const std::string& name, const Kimo::ACL::ACL_t privilege);
	void deny_privilege(const std::string& name, const Kimo::ACL::ACL_t privilege);
	void request_all_processes();
	void show_all_processes();
	void kill(size_t process_num);
	void kill(const std::string& task_name);
	void get_acl();

	const std::string task_status_message(const std::string &taskname) const;
	// ������ ��� <��� �������, ������> 
	void tasks(pair_string_vector &res) const;

	void io_service_run();
private:
	typedef std::vector<node_ptr> Nodes;
	boost::asio::io_service io_serv_;

	Nodes nodes_;	
	std::vector<thread_ptr> thread_pool_;

	// ������������ ����� �������, ������ ����, �� ������� ��� ��������� � ��� �������
	lockable_map<std::string, task_status_record> task_table_;
	// ������ ���� ���������� ������� 
	lockable_vector<grid_task> tasks_;

	// ��� ������������
	std::string username;
	boost::asio::io_service::work work_;
	long user_token;
	// ������ �� ���� �����
	Kimo::TaskList m_tasks;
};

#endif //GRID_CLIENT_H_
