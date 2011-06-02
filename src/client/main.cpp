#include "grid_node.h"
#include "grid_client.h"
#include "input.h"
#include "grid_task.h"
#include "simple_exception.hpp"
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
            throw simple_exception("Error : reading grid net description failed");

		grid_client gc;
		gc.run(addresses, ports);

		grid_task gt;
		std::ifstream fin("..\\..\\test\\task.txt");
		if( !fin ) throw simple_exception("Error : cannot open task.txt");
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

			gc.apply_task(gt);
			//gt.rename(std::string("newtask1"));
			//gc.apply_task(gt);
			//gt.rename(std::string("newtask2"));
			//gc.apply_task(gt);
			//std::cout << gc.task_status_message(gt.name()) << std::endl; 
		}
		fin.close();

		std::cin.unsetf( std::ios_base::skipws );
		char c = std::cin.get();
		gc.refresh_status(gt.name());
		c = std::cin.get();
		std::cout << gc.task_status_message(gt.name()) << std::endl;
		/*
		gc.get_result(gt.name());
		c = std::cin.get();
		//gc.remove_task(gt.name());
		*/
		gc.stop();

	}
	catch(const std::exception &ex)
	{
		std::cerr << ex.what() << std::endl;
	}
	return 0;
}
