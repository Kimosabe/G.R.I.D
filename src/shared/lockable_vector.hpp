#ifndef _LOCKABLE_VECTOR_HPP
#define _LOCKABLE_VECTOR_HPP

#include <vector>
#include <boost/thread/mutex.hpp>

template <typename _ElemType>
class lockable_vector : public std::vector<typename _ElemType> {
private:
	boost::mutex mutex_;

public:
	typedef std::vector<_ElemType> _Mybase;
	typedef lockable_vector<_ElemType> _Myt;

	lockable_vector() : _Mybase(), mutex_()					{};
	lockable_vector(const _Myt &lv) : _Mybase(lv), mutex_()	{};
	virtual ~lockable_vector()	{};

	void lock()			{ mutex_.lock(); };
	bool try_lock()		{ return mutex_.try_lock(); };
	void unlock()		{ mutex_.unlock(); };

	_Myt& operator= (const _Myt &lv)	{ _Mybase::operator =(lv); return *this; };
};

#endif //_LOCKABLE_VECTOR_HPP
