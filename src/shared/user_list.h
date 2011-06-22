#ifndef _KIMO_USER_LIST_H
#define _KIMO_USER_LIST_H

#include <vector>
#include <msgpack.hpp>

namespace Kimo
{

struct UserInfo
{
	int id;
	std::string name;

	MSGPACK_DEFINE(id, name);
};

typedef std::vector<UserInfo> UserInfoList;

} // namespace Kimo
#else

namespace Kimo
{
struct Usernfo;
}

#endif // _KIMO_USER_LIST_H