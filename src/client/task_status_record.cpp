#include "task_status_record.h"

task_status_record::task_status_record() : node_(-1), status_(DONE), status_message_()
{
}

task_status_record::task_status_record(const task_status_record &tr) : node_(tr.node_), status_(tr.status_),
		status_message_(tr.status_message_)
{
}

task_status_record::task_status_record(const int node, const task_status status, const std::string &status_message) : 
	node_(node), status_(status), status_message_(status_message)
{
}

task_status_record::~task_status_record()
{
}

void task_status_record::change_status(const task_status status, const std::string &status_message)
{
	status_ = status;
	status_message_ = status_message;
}

task_status_record& task_status_record::operator= (const task_status_record &tr)
{
	this->node_ = tr.node_;
	this->status_ = tr.status_;
	this->status_message_ = tr.status_message_;
	return *this;
}

