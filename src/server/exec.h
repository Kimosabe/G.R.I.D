#ifndef _EXEC_H
#define _EXEC_H

#include <string>

#if defined(_WIN32) || defined(_WIN64) || defined(WIN32) || defined(WIN64)

#include <windows.h>

#define pid_t HANDLE
#define INVALID_PID INVALID_HANDLE_VALUE

inline bool is_invalid_pid(const pid_t pid)	{ return pid == INVALID_HANDLE_VALUE; };

#else

#include <unistd.h>

#define INVALID_PID -1

inline bool is_invalid_pid(const pid_t pid)	{ return pid < 0; };

#endif //Windows / Linux

pid_t execute(const std::string &command);

#endif //_EXEC_H
