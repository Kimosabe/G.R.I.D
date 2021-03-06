#ifndef _EXEC_H
#define _EXEC_H

#include <string>

#if defined(_WIN32) || defined(_WIN64) || defined(WIN32) || defined(WIN64)

#include <windows.h>
#include <direct.h>

#define pid_t HANDLE
#define INVALID_PID INVALID_HANDLE_VALUE

inline bool is_invalid_pid(const pid_t pid)	{ return pid == INVALID_HANDLE_VALUE; };

// POSIX � Win ������� ���������� �� 1 �������������� � �����
inline int change_dir(const char *pathname){
	return _chdir(pathname);
}

#else

#include <unistd.h>

#define INVALID_PID -1

inline bool is_invalid_pid(const pid_t pid)	{ return pid < 0; };

inline int change_dir(const char *pathname){
	return chdir(pathname);
}

#endif //Windows / Linux

pid_t execute(const std::string &command);

bool wait_process(pid_t pid, unsigned long miliseconds);

void terminate_process(pid_t &pid);

#endif //_EXEC_H
