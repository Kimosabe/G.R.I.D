#ifndef _LOCKABLE_MAP_HPP
#define _LOCKABLE_MAP_HPP

#include <map>
#include <boost/thread/mutex.hpp>

template <class _ElemType1, class _ElemType2>
class lockable_map : public std::map<_ElemType1, _ElemType2> {
private:
	mutable boost::mutex mutex_;

public:
	typedef std::map<_ElemType1, _ElemType2> _Mybase;
	typedef lockable_map<_ElemType1, _ElemType2> _Myt;

	lockable_map() : _Mybase(), mutex_()					{};
	lockable_map(const _Mybase &m) : _Mybase(m), mutex_()	{};
	virtual ~lockable_map()	{};

	void lock()	const		{ mutex_.lock(); };
	bool try_lock()	const	{ return mutex_.try_lock(); };
	void unlock() const		{ mutex_.unlock(); };

	_Myt& operator= (const _Mybase &m)	{ _Mybase::operator =(m); return *this; };
};

#endif //_LOCKABLE_MAP_HPP
