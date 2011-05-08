#ifndef _SIMPLE_EXCEPTION_HPP
#define _SIMPLE_EXCEPTION_HPP

#include <exception>
#include <string>

class simple_exception : public std::exception
{
private : 
	std::string what_;
public :
	simple_exception() : what_()					{};
	simple_exception(const simple_exception &se) : what_(se.what_)	{};
	simple_exception(const char * const message) : what_(message)	{};
	simple_exception(const std::string &message) : what_(message)	{};
	virtual ~simple_exception()	throw()				{};
	
	virtual const char * what() const throw() { return what_.c_str(); };
};

#endif //_SIMPLE_EXCEPTION_HPP
