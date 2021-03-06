#ifndef _LOCKABLE_VECTOR_HPP
#define _LOCKABLE_VECTOR_HPP

#include <vector>
#include <boost/thread/mutex.hpp>

template <class _ElemType>
class lockable_vector : public std::vector<_ElemType> {
private:
	mutable boost::mutex mutex_;

public:
	typedef std::vector<_ElemType> _Mybase;
	typedef lockable_vector<_ElemType> _Myt;

	lockable_vector() : _Mybase(), mutex_()					{};
	lockable_vector(const _Mybase &v) : _Mybase(v), mutex_(){};

	virtual ~lockable_vector()	{};

	void lock()	const		{ mutex_.lock(); };
	bool try_lock()	const	{ return mutex_.try_lock(); };
	void unlock() const		{ mutex_.unlock(); };

	_Myt& operator= (const _Mybase &v)	{ _Mybase::operator =(v); return *this; };
};

#endif //_LOCKABLE_VECTOR_HPP
