#ifndef _TASK_STATUS_RECORD_H
#define _TASK_STATUS_RECORD_H

#include "grid_task.h"

class task_status_record {
public:
	enum task_status{SENDING, EXECUTION, DONE, FAILED, NOTACCEPTED, ACCESS_DENIED, INTERRUPTED};

	task_status_record();
	task_status_record(const task_status_record &tr);
	task_status_record(const int node, const task_status status, const std::string &status_message);
	virtual ~task_status_record();

	const int node() const						{ return node_; };
	const task_status status() const			{ return status_; };
	const std::string& status_message() const	{ return status_message_; };

	void change_status(const task_status status, const std::string &status_message);
	task_status_record& operator= (const task_status_record &tr);

private:
	int node_;
	task_status status_;
	std::string status_message_;
};

#endif //_TASK_STATUS_RECORD_H
