#include "grid_client.h"
#include "grid_task.h"
#include <iostream>

grid_client::grid_client() : io_serv(), nodes(), thread_pool()
{
}

void grid_client::stop()
{
	io_serv.stop();
	using namespace boost;
	for( std::vector< shared_ptr<thread> >::iterator i = thread_pool.begin(); i< thread_pool.end(); ++i)
		(*i)->join();
	thread_pool.clear();
}

bool grid_client::run(const std::vector<std::string> &addresses,
					  const std::vector< std::stack<int> > &ports)
{
	using namespace std;
	size_t nodes_num = addresses.size() < ports.size() ? addresses.size() : ports.size();
	nodes.clear();

	vector<string>::const_iterator addr_iter = addresses.begin();
	vector< stack<int> >::const_iterator port_iter = ports.begin();

	for( size_t i = 0; i < nodes_num; ++addr_iter,++port_iter,++i )
	{
		try{
			node_ptr newnode = node_ptr( new grid_node(io_serv, *addr_iter, *port_iter) );
			nodes.push_back( newnode );
		}
		catch(std::exception &ex){
			std::cerr << ex.what() << std::endl;
		}
	}

	thread_pool.clear();
	thread_pool.resize( nodes.size() );
	vector<thread_ptr>::iterator thread_iter = thread_pool.begin();

	for( ; thread_iter < thread_pool.end(); ++thread_iter )
		*thread_iter = thread_ptr( new boost::thread( boost::bind(&boost::asio::io_service::run,
									&io_serv) ) );

#if defined(_DEBUG) || defined(DEBUG)
	std::cout << nodes.size() << " nodes total" << std::endl;
	cout.flush();
#endif

	return true;
}

grid_client::~grid_client()
{
	this->stop();
	nodes.clear();
}

bool grid_client::apply_task(const grid_task &gt)
{
	if(gt.empty()) return false;
	try{
		/*
			а вот тут должен быть job manager
		*/
		const int target_node = gt.node() == GRID_ANY_NODE ? 0 : gt.node();
		//
		//
		typedef std::vector< std::pair<std::string, std::string> > pair_name_vector;
		for(pair_name_vector::const_iterator i = gt.input_files().begin(); i < gt.input_files().end(); ++i)
			if( this->nodes[target_node]->send_file(i->first, i->second) == false)
				throw std::exception( (std::string("Error : sending ") + i->first + std::string(" failed")).c_str() );
		//
		return true;
	}
	catch(const std::exception &ex){
		std::cerr << ex.what() << std::endl;
	}
	return false;
}

void grid_client::debug_method()
{
	using 
		namespace std;
	for( vector<node_ptr>::iterator i = nodes.begin(); i < nodes.end(); ++i )
		if( (*i)->is_active() )
		{
			//(*i)->send_file(std::string("gg.exe"), std::string("gg.exe"));
			(*i)->request_file(std::string("gg2.txt"), std::string("gg2.txt"));
			(*i)->send_command(std::string("gg.exe"));
			(*i)->request_file(std::string("gg2.txt"), std::string("gg2.txt"));
		}
}
