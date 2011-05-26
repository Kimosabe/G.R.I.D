#include "grid_task_execution.h"
#include "simple_exception.hpp"

grid_task_execution::grid_task_execution(const grid_task &task, const std::string &username) : task_(task), username_(username), start_time_(), finish_time_(),
	child_process_(INVALID_PID), async_thread_()
{}

grid_task_execution::~grid_task_execution()	
{
	// TODO : удаление специального файла с прогрессом выполнения
	// и возможно удаление вообще всех файлов задания
}

inline bool grid_task_execution::active() const
{
	return !start_time_.is_not_a_date_time() && finish_time_.is_not_a_date_time();
}

inline bool grid_task_execution::finished() const
{
	return !finish_time_.is_not_a_date_time();
}

inline const boost::posix_time::ptime& grid_task_execution::start_time() const
{
	return start_time_;
}

inline const boost::posix_time::ptime& grid_task_execution::finish_time() const
{
	return finish_time_;
}

inline boost::posix_time::ptime grid_task_execution::local_time()
{
	return boost::posix_time::second_clock::local_time();
}

const short grid_task_execution::progress() const
{
	if( active() )
	{
		//TODO : чтение прогресса из специального файла
	}
	else if( finished() )
	{
		return 100; 
	}
	return 0;
}

inline const grid_task& grid_task_execution::task() const
{
	return task_;
}

inline const std::string& grid_task_execution::username() const
{
	return username_;
}

void  grid_task_execution::interrupt()
{
	async_thread_.interrupt();
}

void grid_task_execution::async_start()
{
	async_thread_ = boost::thread(boost::bind(&grid_task_execution::start, this));
}

void grid_task_execution::start()
{
	start_time_ = boost::posix_time::second_clock::local_time();
	std::cout << "start time " << start_time_ << std::endl;

	try{		
		for(std::vector<std::string>::const_iterator i = task_.commands().begin(); i < task_.commands().end(); ++i)
		{
			boost::this_thread::interruption_point();
			child_process_ = execute(*i);
			if( is_invalid_pid(child_process_) )
				throw simple_exception("Cannot create ptrocess");
			int wait_time_mseconds = 4000; //4 секунды
			while(wait_process(child_process_, wait_time_mseconds) == false)
				boost::this_thread::interruption_point();
		}		
	}
	catch(boost::thread_interrupted &){
		terminate_process(child_process_);
	}
	catch(std::exception &){
	}
	finish_time_ = boost::posix_time::second_clock::local_time();

	std::cout << "completed " << task_.name() << std::endl;
	std::cout << " finish time " << finish_time_ << std::endl;
}
	