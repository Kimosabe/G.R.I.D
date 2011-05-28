#ifndef _LOCKABLE_MAP_HPP
#define _LOCKABLE_MAP_HPP

#include <map>
#include <boost/thread/mutex.hpp>

template <typename _ElemType1, typename _ElemType2>
class lockable_map : public std::map<_ElemType1, _ElemType2> {
private:
	boost::mutex mutex_;

public:
	typedef std::map<_ElemType1, _ElemType2> _Mybase;
	typedef lockable_map<_ElemType1, _ElemType2> _Myt;

	lockable_map() : _Mybase(), mutex_()					{};
	lockable_map(const _Myt &lm) : _Mybase(lm), mutex_()	{};
	virtual ~lockable_map()	{};

	void lock()			{ mutex_.lock(); };
	bool try_lock()		{ return mutex_.try_lock(); };
	void unlock()		{ mutex_.unlock(); };

	_Myt& operator= (const _Myt &lm)	{ _Mybase::operator =(lm); return *this; };
};

#endif //_LOCKABLE_VECTOR_HPP
