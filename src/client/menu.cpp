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
	menu[ADD_USER] = menu_item("add_user", "add user to grid");
	menu[REMOVE_USER] = menu_item("remove_user", "remove user from grid");
	menu[ALLOW_PRIVILEGE] = menu_item("allow_privilege", "allow privilege to user");
	menu[DENY_PRIVILEGE] = menu_item("deny_privilege", "deny privilege to user");
	menu[REQUEST_ALL_PROCESSES] = menu_item("request_all_processes", "request all processes info on every node");
	menu[SHOW_ALL_PROCESSES] = menu_item("show_all_processes", "shows all processes on every node");
	menu[KILL_PROCESS] = menu_item("kill", "kill <process_num> - kills a process");

	return menu;
}