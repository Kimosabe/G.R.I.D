#ifndef GRID_TASK_H_
#define GRID_TASK_H_

#include <string>
#include <vector>
#include <utility>
#include <msgpack.hpp>

#if defined(_DEBUG) || defined(DEBUG)
#pragma comment(lib, "msgpack-mtddll.lib")
#else
#pragma comment (lib, "msgpack-mt.lib")
#endif //DEBUG

#define GRID_ANY_NODE -1

/**
 * ������� ��� ���� �������
 * ���� ��������� ������� ������, ������ � �������� ������
 * ������� ��� ������� ����� ���������� �� ����������� ����
 * ����� ��������������� ����������� ��� �������
 * � �������� ����� ������������ �������
 * ������� ��� ����� ��������� ������
 */

class grid_task{
public:
	MSGPACK_DEFINE(commands_, input_files_, output_files_, node_, work_dir_, name_, os_);

	typedef std::vector< std::pair<std::string, std::string> > pair_name_vector; 

	grid_task();
	grid_task(const grid_task &gt);
	virtual ~grid_task();

	grid_task& operator = (const grid_task &gt);
	bool empty() const;

	const int node() const							{ return node_; };
	const std::vector<std::string>& commands() const{ return commands_; };
	const pair_name_vector& input_files() const		{ return input_files_; };
	const pair_name_vector& output_files() const	{ return output_files_; };
	const std::string& work_directory() const		{ return work_dir_; };
	const std::string& name() const					{ return name_; };
	const std::string& os() const					{ return os_; };


	void add_command(const std::string &command);
	void add_input_file(const std::string &local_name);
	void add_output_file(const std::string &remote_name);
	void add_input_file(const std::string &local_name, const std::string &remote_name);
	void add_output_file(const std::string &remote_name, const std::string &local_name);
	void remove_input_file(const std::string &local_name);
	void remove_output_file(const std::string &remote_name);
	void rename(const std::string &new_name);

	void set_node(const int &node);
	void set_work_directory(const std::string &work_dir);
	void set_os(const std::string &os);

	void clear();

protected:
	std::vector<std::string> commands_;
	pair_name_vector input_files_;
	pair_name_vector output_files_;
	int node_;
	// work_dir_ ���� �� ������������. �������� ����� ��������� �����-������
	std::string work_dir_, name_, os_;
};

#endif //GRID_TASK_H_
