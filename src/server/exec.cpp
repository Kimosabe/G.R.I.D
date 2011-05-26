#include "exec.h"

#if defined(_WIN32) || defined(_WIN64) || defined(WIN32) || defined(WIN64)

#include <exception>
#include <iostream>
#include <memory.h>

pid_t execute(const std::string &command)
{
	try{
		char *buf = new char[command.size()+1];
		memset(buf, 0, (command.size() + 1)*sizeof(char));
		memcpy(buf, command.c_str(), command.size()*sizeof(char));

		STARTUPINFO si;
		PROCESS_INFORMATION pi;

		ZeroMemory(&si, sizeof(si));
		si.cb = sizeof(si);
		si.dwFlags = STARTF_USESTDHANDLES;
		si.hStdError = si.hStdInput = si.hStdOutput = INVALID_HANDLE_VALUE;
		ZeroMemory(&pi, sizeof(pi));

		CreateProcess(NULL, buf, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi);

		delete[] buf;
		return pi.hProcess;
	}
	catch(const std::exception &ex){
		std::cerr << ex.what() << std::endl;
	}

	return INVALID_HANDLE_VALUE;
}

bool wait_process(pid_t pid, unsigned long miliseconds)
{
	return WaitForSingleObject(pid, miliseconds) != WAIT_TIMEOUT;
}

void terminate_process(pid_t &pid)
{
	TerminateProcess(pid, 1);
	pid = INVALID_PID;
}


#else

#include <cstdlib>
#include <signal.h>

pid_t execute(const std::string &command)
{
	pid_t pid = fork();
	if( pid != 0 ) return pid;
	
	close(0);
	close(1);
	close(2);
	
	system(command.c_str());
	exit(0);
	return 0;
}

bool wait_process(pid_t pid, unsigned long miliseconds)
{
	int status = 0;
	unsigned long microseconds = miliseconds * 1000, step = miliseconds * 100;
	for(int i = 0; i < 10; ++i)
	{
		pid_t res = waitpid(pid, &status, WNOHANG | WUNTRACED);
		if( res > 0 )
			return true;
		else
			usleep(step);
	}
	return false;
}

void terminate_process(pid_t &pid)
{
	kill(pid, SIGKILL);
	pid = INVALID_PID;
}

#endif //Windows / Linux
