#ifndef _GRID_TASK_EXECUTION_H
#define _GRID_TASK_EXECUTION_H

#include "grid_task.h"
#include "exec.h"
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread.hpp>
#include <boost/shared_ptr.hpp>

/**
 * данные о выполнении задания
 */

class grid_task_execution{
protected:
	boost::posix_time::ptime start_time_, finish_time_;
	grid_task task_;
	std::string username_;

	pid_t child_process_;
	boost::thread async_thread_;

	void start();

	typedef union {
		char ch[sizeof(short)];
		short sh;
	} _char_short_t;

	bool m_interrupted;

public:
	grid_task_execution(const grid_task &task, const std::string &username);
	virtual ~grid_task_execution();

	// прогресс выполнения в процентах. 0 в случае незапущенного задания
	// или когда данные о прогессе выполнения не удается получить
	const short progress() const;

	bool active() const;
	bool finished() const;
	bool interrupted() const;

	const boost::posix_time::ptime& start_time() const;
	const boost::posix_time::ptime& finish_time() const;
	static boost::posix_time::ptime local_time();

	const grid_task& task() const;
	const std::string& username() const;

	void async_start();
	void interrupt();
	const pid_t child_process() const;
};

typedef boost::shared_ptr<grid_task_execution> grid_task_execution_ptr;

#endif //_GRID_TASK_EXECUTION_H
