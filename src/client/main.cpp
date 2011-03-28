#include "grid_node.h"
#include "grid_client.h"
#include "input.h"
#include "grid_task.h"
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <boost/thread.hpp>

int main(int argc, char *argv[])
{
	try
	{
		setlocale(LC_ALL, "Russian");
		std::vector<std::string> addresses;
		std::vector< std::stack<int> > ports;
		if( !read_net(addresses, ports) )
			//throw std::exception();
            throw new MyException("Error : reading grid net description failed");

		grid_client gc;
		gc.run(addresses, ports);
		
		gc.debug_method();

		std::cin.unsetf( std::ios_base::skipws );
		char c = std::cin.get();
		gc.stop();
/*
		grid_task gt;
		std::ifstream fin("task.txt");
		if( !fin ) throw std::exception("Error : cannot open task.txt");
		if( parse_task(gt, fin) )
		{
			std::cout << gt.name() << std::endl;
			std::cout << std::endl;
			for(std::vector< std::string >::const_iterator i = gt.commands().begin(); i < gt.commands().end(); ++i)
				std::cout << *i << std::endl;
			std::cout << std::endl;
			for(std::vector< std::pair< std::string, std::string > >::const_iterator i = gt.input_files().begin(); i < gt.input_files().end(); ++i)
				std::cout << i->first << '\t' << i->second << std::endl;
			std::cout << std::endl;
			for(std::vector< std::pair< std::string, std::string > >::const_iterator i = gt.output_files().begin(); i < gt.output_files().end(); ++i)
				std::cout << i->first << '\t' << i->second << std::endl;
		}
		fin.close();
*/
	}
	catch(const std::exception &ex)
	{
		std::cerr << ex.what() << std::endl;
	}
	return 0;
}
