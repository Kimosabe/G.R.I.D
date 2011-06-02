#include "session.h"
#include "server.h"
#include "grid_node.h"
#include "exec.h"
#include <fstream>

int main(int argc, char* argv[])
{
	try
	{
		if (argc != 2)
		{
			std::cerr << "Usage: async_tcp_echo_server <port>\n";
			return 1;
		}

		grid_node node(boost::lexical_cast<short>(argv[1]));
		node.run();

		//std::cin.unsetf(std::ios::skipws);
		//char c;
		//std::cin >> c;

		return 0;
	}
	catch (const std::exception& e)
	{
		std::cerr << "Exception: " << e.what() << "\n";
	}

	return 1;
}