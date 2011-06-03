#include "session.h"
#include "server.h"
#include "grid_node.h"
#include "exec.h"
#include "input.h"
#include "simple_exception.hpp"
#include <fstream>

// наверное, нехорошо, что ось устанавливается в конфиг файле
// но это в стиле C позволять выстрелить себе в ногу
std::string os;

int main(int argc, char* argv[])
{
	try
	{
		if (argc != 2)
		{
			std::cerr << "Usage: async_tcp_echo_server <port>\n";
			return 1;
		}


		config_t config;
		if( !read_config(config) )
			throw simple_exception(std::string("Error : cannot find ") + std::string(CONFIG_FILE));
		if( config.count("os") == 0 )
			throw simple_exception(std::string("Error : os not defined in") + std::string(CONFIG_FILE) + 
					std::string(". Add something like os=windows or os=linux"));
		if( config.count("workdir") > 0 )
			if( change_dir(config["workdir"].data()) != 0 )
				std::cerr << "Warning : cannot change directory to " << config["workdir"] << std::endl;

		std::vector<std::string> addresses;
		std::vector< std::stack<int> > ports;
		if( !read_net(addresses, ports) )
			std::cerr << "Warning : cannot read net description file " << NET_DESCRIPTION_FILE << std::endl;

		os = config["os"];

		grid_node node(boost::lexical_cast<short>(argv[1]), addresses, ports);
		node.run();

		return 0;
	}
	catch (std::exception& e)
	{
		std::cerr << "Exception: " << e.what() << "\n";
	}

	return 1;
}