#ifndef KIMO_ACL_H_
#define KIMO_ACL_H_

#include <msgpack.hpp>

#ifdef _MSC_VER

typedef unsigned __int8 uint8_t;
typedef unsigned __int16 uint16_t;
typedef unsigned __int32 uint32_t;
typedef unsigned __int64 uint64_t;

#else

#include <stdint.h>

#endif
namespace Kimo{

class ACL
{
public:
	ACL();
	ACL(const ACL& acl);
	ACL& operator=(const ACL& acl);
	~ACL();

	//! привилегии в списке доступа
	enum PRIVILEGE
	{
		PRIV_LOGIN = 0x01,
		PRIV_PROCEXEC = 0x02,
		PRIV_PROCTERM = 0x04,
		PRIV_USERWR = 0x08,
		PRIV_USERRD = 0x10,
		PRIV_PROCRD = 0x20,
	    
		PRIV_MAXPRIV, // НЕ ИСПОЛЬЗОВАТЬ В КАЧЕСТВЕ ПРИВИЛЕГИИ!!!
		PRIV_ALLPRIV = ((PRIV_MAXPRIV - 1) << 1) - 1 // MAXPRIV и ALLPRIV должны быть в конце
	};

	// минимизация размера переменной под список доступа
	#if PRIV_MAXPRIV < (1 << 8)
		typedef uint8_t ACL_t;
	#elif PRIV_MAXPRIV < (1 << 16)
		typedef uint16_t ACL_t;
	#elif PRIV_MAXPRIV < (1 << 32)
		typedef uint32_t ACL_t;
	#else
		typedef uint64_t ACL_t;
	#endif

	bool isAllowed(const PRIVILEGE privilege);
	bool isDenied(const PRIVILEGE privilege);
	void allow(const PRIVILEGE privilege);
	void deny(const PRIVILEGE privilege);

	ACL_t acl();

private:
	ACL_t m_acl;

public:
	// Сериализация
	MSGPACK_DEFINE(m_acl);
};

} // namespace Kimo
#else

namespace Kimo{

class ACL;

}

#endif // KIMO_ACL_H_