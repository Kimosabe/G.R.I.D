#include "input.h"
#include "grid_task.h"
#include "simple_exception.hpp"
#include <fstream>
#include <sstream>
#include <boost/algorithm/string.hpp>

#define BUF_SIZE 2048

inline bool is_ws(const char x){
	return x == ' ' || x == '\t' || x == '\n';
}

bool read_net(std::vector<std::string> &addresses, std::vector< std::stack<int> > &ports)
{
	addresses.clear();
	ports.clear();

	using namespace std;
	ifstream fin(NET_DESCRIPTION_FILE);
	if( !fin )
		return false;

	while( !fin.eof() )
	{
		stack<int> curports;
		char strbuf[BUF_SIZE];
		fin.getline(strbuf, BUF_SIZE);

		stringstream ssin(strbuf);
		char addrbuf[BUF_SIZE];
		ssin.getline(addrbuf, BUF_SIZE, ':');
		while( !ssin.eof() ){
			int newport;
			ssin >> newport;
			curports.push( newport );

			if( ssin.eof() ) break;
			int c;
			do
				c = ssin.get();
			while( !ssin.eof() && c == ' ' || c =='\t' );
		}
		if( !curports.empty() && strlen(addrbuf) > 0 ){
			addresses.push_back( string(addrbuf) );
			ports.push_back( curports );
		}
	}

	fin.close();
	return !addresses.empty() && !ports.empty();
}

bool read_config(config_t &config)
{
	config.clear();
	std::ifstream fin(CONFIG_FILE);
	if( fin )
	{
		std::string line;
		while( !fin.eof() )
		{
			std::getline(fin, line);
			std::vector<std::string> split_vec;
			boost::algorithm::split(split_vec, line, boost::algorithm::is_any_of("="));
			if( split_vec.size() > 1 )
			{
				boost::to_lower(split_vec[0]);
				if( split_vec[0] != std::string("workdir") )
					boost::to_lower(split_vec[1]);
				config[split_vec[0]] = split_vec[1];
			}
		}
		return true;
	}
	else
		return false;
}

