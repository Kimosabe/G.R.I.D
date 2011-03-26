#ifndef GRID_TASK_H_
#define GRID_TASK_H_

#include <string>
#include <vector>
#include <utility>

#define GRID_ANY_NODE -1

class grid_task{
private:
	typedef std::vector< std::pair<std::string, std::string> > pair_name_vector; 

	std::vector<std::string> commands_;
	pair_name_vector input_files_;
	pair_name_vector output_files_;
	int node_;
	std::string work_dir_, name_;
public:
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

	void clear();
};

#endif //GRID_TASK_H_
