#include "grid_task.h"

grid_task::grid_task() : commands_(), input_files_(), output_files_(), name_(),
	node_(GRID_ANY_NODE), work_dir_(), os_()
{
}

grid_task::grid_task(const grid_task &gt) : commands_(gt.commands_), name_(gt.name_),
	input_files_(gt.input_files_), output_files_(gt.output_files_), node_(gt.node_),
	work_dir_(gt.work_dir_), os_(gt.os_)
{
}

grid_task::~grid_task()
{
}

grid_task& grid_task::operator = (const grid_task &gt)
{
	this->commands_		= gt.commands_;
	this->input_files_	= gt.input_files_;
	this->output_files_	= gt.output_files_;
	this->node_			= gt.node_;
	this->work_dir_		= gt.work_dir_;
	this->name_			= gt.name_;
	this->os_			= gt.os_;
	return *this;
}

bool grid_task::empty() const
{
	return name_.empty() || (commands_.empty() && input_files_.empty() && output_files_.empty());
}

void grid_task::add_command(const std::string &command)
{
	if( !command.empty() )
		commands_.push_back(command);
}

void grid_task::add_input_file(const std::string &local_name)
{
	this->add_input_file(local_name, local_name);
}

void grid_task::add_output_file(const std::string &remote_name)
{
	this->add_output_file(remote_name, remote_name);
}

void grid_task::add_input_file(const std::string &local_name, const std::string &remote_name)
{
	for( pair_name_vector::iterator i=input_files_.begin(); i<input_files_.end(); ++i )
		if( i->first == local_name )
		{
			i->second = remote_name;
			return;
		}

	input_files_.push_back( std::pair<std::string, std::string>(local_name, remote_name) );
}

void grid_task::add_output_file(const std::string &remote_name, const std::string &local_name)
{
	for( pair_name_vector::iterator i=output_files_.begin(); i<output_files_.end(); ++i )
		if( i->first == remote_name )
		{
			i->second = local_name;
			return;
		}

	output_files_.push_back( std::pair<std::string, std::string>(remote_name, local_name) );
}

void grid_task::remove_input_file(const std::string &local_name)
{
	for( pair_name_vector::iterator i=input_files_.begin(); i<input_files_.end(); ++i )
		if( i->first == local_name )
		{
			input_files_.erase(i, i+1);
			return;
		}
}

void grid_task::remove_output_file(const std::string &remote_name)
{
	for( pair_name_vector::iterator i=output_files_.begin(); i<output_files_.end(); ++i )
		if( i->first == remote_name )
		{
			output_files_.erase(i, i+1);
			return;
		}
}

void grid_task::set_work_directory(const std::string &work_dir)
{
	this->work_dir_ = work_dir;
}

void grid_task::set_node(const int &node)
{
	this->node_ = node >= 0 ? node : GRID_ANY_NODE;
}

void grid_task::set_os(const std::string &os)
{
	os_ = os;
}

void grid_task::clear()
{
	commands_.clear();
	input_files_.clear();
	output_files_.clear();
	work_dir_.clear();
	name_.clear();
	os_.clear();
}

void grid_task::rename(const std::string &new_name)
{
	this->name_ = new_name;
}
