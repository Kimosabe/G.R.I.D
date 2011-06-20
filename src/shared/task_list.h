#ifndef _KIMO_TASK_LIST_H
#define _KIMO_TASK_LIST_H

#include <vector>
#include <msgpack.hpp>

namespace Kimo
{

struct Task
{
	size_t node_id;
	std::string owner;
	std::string name;
	std::string start_date;
	std::string status;

	MSGPACK_DEFINE(node_id, owner, name, start_date, status);
};

typedef std::vector<Task> TaskList;

} // namespace Kimo
#else

namespace Kimo
{
struct Task;
}

#endif // _KIMO_TASK_LIST_H