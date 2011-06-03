#ifndef MENU_H_
#define MENU_H_

#include <string>
#include <vector>
#include <map>

typedef enum {
	QUIT, APPLY_TASK, REMOVE_TASK, REFRESH_STATUS, GET_RESULT, STATUS, TASKS
} command_meaning;

class menu_item {
public:
	std::string command, definition;

	menu_item() : command(), definition()	{};
	menu_item(const menu_item &mi) : command(mi.command), definition(mi.definition)	{};
	menu_item(const std::string &command_, const std::string &definition_) : command(command_), definition(definition_)	{};
};

typedef std::map<command_meaning, menu_item> menu_t;

menu_t get_menu();

#endif //MENU_H_
