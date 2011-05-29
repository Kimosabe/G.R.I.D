#include "session.h"
#include "server.h"
#include "grid_node.h"
#include "exec.h"

int main(int argc, char* argv[])
{
	try
	{
		//if (argc != 2)
		//{
		//	std::cerr << "Usage: async_tcp_echo_server <port>\n";
		//	return 1;
		//}

		//boost::asio::io_service io_service;

		
		//server s(io_service, boost::lexical_cast<int>(argv[1]));
		//io_service.run();

		grid_node node(5986);
		//grid_node node(boost::lexical_cast<short>(argv[1]));
		node.run();

		std::cin.unsetf(std::ios::skipws);
		char c;
		std::cin >> c;

		return 0;
	}
	catch (const std::exception& e)
	{
		std::cerr << "Exception: " << e.what() << "\n";
	}

	return 1;
}