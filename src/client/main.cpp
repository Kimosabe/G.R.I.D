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

int main(int argc, char *argv[])
{
	typedef CryptoPP::RIPEMD256 HASHER;
	try
	{
		setlocale(LC_ALL, "Russian");
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
			//buffer[hasher.DigestSize()] = '\0';
			//password = buffer;

			password.clear();

			CryptoPP::HexEncoder encoder;
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
