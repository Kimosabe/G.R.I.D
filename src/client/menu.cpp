#include "menu.h"

menu_t get_menu()
{
	menu_t menu;
	menu[QUIT] = menu_item("quit", "quit grid client");
	menu[APPLY_TASK] = menu_item("apply", "apply <file with grid task definition>");
	menu[REMOVE_TASK] = menu_item("remove", "remove <taskname>");
	menu[REFRESH_STATUS] = menu_item("refresh", "refresh <taskname> - asynchronous refreshing task status");
	menu[GET_RESULT] = menu_item("result", "result <taskname> - download output files of <taskname> from node");
	menu[STATUS] = menu_item("status", "status <taskname> - displays current <taskname> status. It's useful to refresh it sometimes");
	menu[TASKS] = menu_item("tasks", "displays all known tasks");

	return menu;
}