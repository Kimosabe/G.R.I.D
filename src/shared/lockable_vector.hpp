#ifndef _LOCKABLE_VECTOR_HPP
#define _LOCKABLE_VECTOR_HPP

#include <vector>
#include <boost/thread/mutex.hpp>

template <typename _ElemType>
class lockable_vector : public std::vector<typename _ElemType> {
private:
	boost::mutex mutex_;

public:
	lockable_vector() : std::vector<_ElemType>(), mutex_()		{};
	lockable_vector(const lockable_vector &lv) : std::vector<_ElemType>(lv), mutex_()	{};
	virtual ~lockable_vector()	{};

	void lock()			{ mutex_.lock(); };
	bool try_lock()		{ return mutex_.try_lock(); };
	void unlock()		{ mutex_.unlock(); };

	lockable_vector& operator= (const lockable_vector &lv)	{ std::vector<_ElemType>::operator =(lv); return *this; };
};

#endif //_LOCKABLE_VECTOR_HPP
