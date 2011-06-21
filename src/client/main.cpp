#include <cstdlib>
#include <iostream>
#include <fstream>
#include <algorithm>

#include <boost/thread.hpp>
#include <boost/algorithm/string.hpp> 

#include <cryptopp\ripemd.h>
#include <cryptopp\hex.h>

#include "grid_node.h"
#include "grid_client.h"
#include "input.h"
#include "grid_task.h"
#include "simple_exception.hpp"
#include "menu.h"
#include "acl.h"

Kimo::ACL::ACL_t getPrivilege()
{
	std::cout << "Privileges:" << std::endl
		<<  "0. Login" << std::endl
		<<  "1. Process execution" << std::endl
		<<  "2. All processes termination" << std::endl
		<<  "3. All users information managment" << std::endl
		<<  "4. All users information read" << std::endl
		<<  "5. All processes information read" << std::endl
		<<	"6. All privileges" << std::endl << std::endl
		<<  "7. Cancel" << std::endl;

	std::string value;
	int num;
	bool flag = true;
	for (int i = 0; i < 3 && flag; ++i)
	{
	try
	{
		std::getline(std::cin, value);
		num = boost::lexical_cast<Kimo::ACL::ACL_t>(value);
		if (num >= 0 && num < 8)
			flag = false;
		else if (num >= 48 && num < 56)
		{
			num -= 48;
			flag = false;
		}
		else
			std::cout << "wrong num, try again: ";
	}
	catch(boost::bad_lexical_cast& /*ex*/)
	{
		std::cout << "wrong num, try again: ";
	}
	}

	if (num == 7)
		return 3;
	else if (num == 6)
		return Kimo::ACL::PRIV_ALLPRIV;

	return Kimo::ACL::ACL_t(1 << num);
}

int main(int argc, char *argv[])
{
	typedef CryptoPP::RIPEMD256 HASHER;
	try
	{
		setlocale(LC_ALL, "Russian");
		CryptoPP::HexEncoder encoder;
		std::vector<std::string> addresses;
		std::vector< std::stack<int> > ports;
		if( !read_net(addresses, ports) )
            throw simple_exception("Error : reading grid net description failed");

		grid_client gc;
		gc.run(addresses, ports);

		// LOGIN
		HASHER hasher;
		std::string login;
		std::string password;
		char buffer[HASHER::DIGESTSIZE/* + 1*/];
		bool incorrect_login;
		do
		{
			incorrect_login = false;
			std::cout << "login: ";
			std::getline(std::cin, login);
			if (login.empty())
			{
				std::cout << "login is empty" << std::endl;
				incorrect_login = true;
				continue;
			}
			std::cout << "password: ";
			std::getline(std::cin, password);

			// хэшируем пароль
			hasher.CalculateDigest((byte*)buffer, (const byte*)password.c_str(), password.size());
			password.clear();
			encoder.Attach( new CryptoPP::StringSink( password ) );
			encoder.Put( (byte*)buffer, HASHER::DIGESTSIZE );
			encoder.MessageEnd();
		} while (incorrect_login || !gc.login(login, password));
		// LOGIN

		menu_t menu = get_menu();
		while(1)
		{
			std::string user_str;
			std::getline(std::cin, user_str);
			boost::to_lower(user_str);
			std::vector<std::string> split_vec;
			boost::algorithm::split(split_vec, user_str, boost::algorithm::is_any_of(" \t"));
			
			if( split_vec.empty() ) continue;

			std::string user_command = split_vec.front();
			std::string task_name = split_vec.size() > 1 ? split_vec[1] : std::string();
			grid_task gt;

			//***********************************************************************
			if( user_command == menu[QUIT].command )
			{
				break;
			}
			//***********************************************************************
			else if( user_command == menu[APPLY_TASK].command )
			{
				if( !task_name.empty() )
				{
					std::ifstream fin(task_name.data());
					if( fin )
					{
						if( parse_task(gt, fin) )
							gc.apply_task(gt);
					}
					else
						std::cerr << "Error : cannot open task.txt" << std::endl;
					fin.close();
				}
				else
					std::cout << menu[APPLY_TASK].definition << std::endl;
			}
			//***********************************************************************
			else if( user_command == menu[REMOVE_TASK].command )
			{
				if( !task_name.empty() )
					gc.remove_task(task_name);
				else
					std::cout << menu[REMOVE_TASK].definition << std::endl;
			}
			//***********************************************************************
			else if( user_command == menu[REFRESH_STATUS].command )
			{
				if( !task_name.empty() )
					gc.refresh_status(task_name);
				else
					std::cout << menu[REFRESH_STATUS].definition << std::endl;
			}
			//***********************************************************************
			else if( user_command == menu[GET_RESULT].command )
			{
				if( !task_name.empty() )
					gc.get_result(task_name);
				else
					std::cout << menu[GET_RESULT].definition << std::endl;
			}
			//***********************************************************************
			else if( user_command == menu[STATUS].command )
			{
				if( !task_name.empty() )
					std::cout << gc.task_status_message(task_name) << std::endl;
				else
					std::cout << menu[STATUS].definition << std::endl;
			}
			//***********************************************************************
			else if( user_command == menu[TASKS].command )
			{
				grid_client::pair_string_vector tasks;
				gc.tasks(tasks);
				for(grid_client::pair_string_vector::const_iterator i = tasks.begin(); i < tasks.end(); ++i)
					std::cout << i->first << '\t' << i->second << std::endl;
			}
			//***********************************************************************
			else if( user_command == menu[ADD_USER].command )
			{
				do
				{
					std::cout << "login: ";
					std::getline(std::cin, login);	
					incorrect_login = false;
					if (login.empty())
					{
						std::cout << "login is empty" << std::endl;
						incorrect_login = true;
						continue;
					}
				} while (incorrect_login);
				std::cout << "password: ";
				std::getline(std::cin, password);
				// хэшируем пароль
				hasher.CalculateDigest((byte*)buffer, (const byte*)password.c_str(), password.size());
				password.clear();
				encoder.Attach( new CryptoPP::StringSink( password ) );
				encoder.Put( (byte*)buffer, HASHER::DIGESTSIZE );
				encoder.MessageEnd();

				gc.add_user(login, password);
			}
			//***********************************************************************
			else if( user_command == menu[REMOVE_USER].command )
			{
				do
				{
					std::cout << "login: ";
					std::getline(std::cin, login);	
					incorrect_login = false;
					if (login.empty())
					{
						std::cout << "login is empty" << std::endl;
						incorrect_login = true;
						continue;
					}
				} while (incorrect_login);

				gc.remove_user(login);
			}
			//***********************************************************************
			else if( user_command == menu[ALLOW_PRIVILEGE].command )
			{
				do
				{
					std::cout << "login: ";
					std::getline(std::cin, login);	
					incorrect_login = false;
					if (login.empty())
					{
						std::cout << "login is empty" << std::endl;
						incorrect_login = true;
						continue;
					}
				} while (incorrect_login);

				Kimo::ACL::ACL_t privilege;
				privilege = getPrivilege();
				if (Kimo::ACL::isValidPrivilege(privilege))
					gc.allow_privilege(login, privilege);
			}
			//***********************************************************************
			else if( user_command == menu[DENY_PRIVILEGE].command )
			{
				do
				{
					std::cout << "login: ";
					std::getline(std::cin, login);	
					incorrect_login = false;
					if (login.empty())
					{
						std::cout << "login is empty" << std::endl;
						incorrect_login = true;
						continue;
					}
				} while (incorrect_login);

				Kimo::ACL::ACL_t privilege;
				privilege = getPrivilege();
				if (Kimo::ACL::isValidPrivilege(privilege))
					gc.deny_privilege(login, privilege);
			}
			//***********************************************************************
			else if( user_command == menu[REQUEST_ALL_PROCESSES].command )
			{
				gc.request_all_processes();
			}
			//***********************************************************************
			else if( user_command == menu[SHOW_ALL_PROCESSES].command )
			{
				gc.show_all_processes();
			}
			//***********************************************************************
			else if( user_command == menu[KILL_PROCESS].command )
			{
				if (task_name.empty())
					std::cout << menu[KILL_PROCESS].definition << std::endl;
				else
				{
					size_t process_num;
					try
					{
						process_num = boost::lexical_cast<size_t>(task_name);
						gc.kill(process_num);
					}
					catch(boost::bad_lexical_cast& /*ex*/)
					{
						std::cout << "bad process_num. maybe you need kill_task?" << std::endl;
					}
				}
			}
			//***********************************************************************
			else if( user_command == menu[KILL_TASK].command )
			{
				if (task_name.empty())
					std::cout << menu[KILL_TASK].definition << std::endl;
				else
					gc.kill(task_name);
			}
			//***********************************************************************
			else if( user_command == menu[GET_ACL].command )
			{
				gc.get_acl();
			}
			//***********************************************************************
			else
			{
				for(menu_t::const_iterator i = menu.begin(); i != menu.end(); ++i)
					std::cout << i->second.command << '\t' << i->second.definition << std::endl;
			}
			//***********************************************************************
		}

		gc.stop();
	}
	catch(const std::exception &ex)
	{
		std::cerr << ex.what() << std::endl;
	}
	return 0;
}
