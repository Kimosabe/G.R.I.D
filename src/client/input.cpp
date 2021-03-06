#include "input.h"
#include "grid_task.h"
#include "simple_exception.hpp"
#include <fstream>
#include <sstream>
#include <boost/regex.hpp>

#define NET_DESCRIPTION_FILE "..\\..\\test\\net.dat"
#define BUF_SIZE 2048

inline bool is_ws(const char x){
	return x == ' ' || x == '\t' || x == '\n';
}

bool read_net(std::vector<std::string> &addresses, std::vector< std::stack<int> > &ports)
{
	using namespace std;
	ifstream fin(NET_DESCRIPTION_FILE);
	if( !fin )
		return false;

	addresses.clear();
	ports.clear();
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

// TODO : �������� ��� �� ���������� �������� �������
// ��� �������� ������ ����, ����� ������ ������ ���������
// ���� �� �������������� ���� � ������ ������, ��� ��� ��� ������ ������������� � ������� �������� 
bool parse_task(grid_task &gt, std::istream &fin)
{
	const static boost::regex re_task_begin("(?xs)(^<\\s* task \\s+ name \\s* = \\s* ( [[:alpha:][:digit:]_]+ ) \\s*>)"),
		re_task_end("(?xs)(^<\\s* /task \\s*>)"),
		re_cmd_begin("(?xs)(^<\\s* command \\s*>)"),
		re_cmd_end("(?xs)(^<\\s* /command \\s*>)"),
		re_inp_begin("(?xs)(^<\\s* input \\s*>)"),
		re_inp_end("(?xs)(^<\\s* /input \\s*>)"),
		re_outp_begin("(?xs)(^<\\s* output \\s*>)"),
		re_outp_end("(?xs)(^<\\s* /output \\s*>)"),
		re_os_begin("(?xs)(^<\\s* os \\s*>)"),
		re_os_end("(?xs)(^<\\s* /os \\s*>)"),
		re_filenames1("(?xs)(^\\s* (\" (?: [^ \" \\\\ / \\* \\? : > < \\| ] )+ \") \\s* \\| \\s* (\" (?: [^ \" \\\\ / \\* \\? : > < \\| ] )+ \") \\s*$)"),
		re_filenames2("(?xs)(^\\s* ( (?: [^ \" \\\\ / \\* \\? : > < \\| ] )+ ) \\s* \\| \\s* ( (?: [^ \" \\\\ / \\* \\? : > < \\| ] )+ ) \\s*$)");
	try{
		grid_task temp;
		if(fin.eof() || fin.fail())
            throw simple_exception("Error : invalid file descriptor (task parsing)");

		bool task_started = false, task_finished = false;
		char buf[BUF_SIZE];
		while(!fin.eof())
		{
			fin.getline(buf, BUF_SIZE);
			size_t len = fin.gcount();
			size_t cursor = 0;

			for( ; cursor < len && is_ws(buf[cursor]); ++cursor);
			if(cursor == len || buf[cursor] == '#')
				continue;

			char * cursor_ptr = buf + cursor;
			const std::string sbuf(cursor_ptr);
			boost::smatch smatch_res;

			//******************************** <task> ****************************
			if( boost::regex_match(sbuf, smatch_res, re_task_begin) )
				if( !task_started )
				{
					temp.rename(smatch_res[2]);
					task_started = true;
				}
				else
					throw simple_exception("Error : unexpeted internal task definition");

			//******************************** </task> ****************************
			else if( boost::regex_match(sbuf, re_task_end) )
				if( task_started )
				{
					task_finished = true;
					break;
				}
				else
					throw simple_exception("Error : unexpected end of task clause");

			//******************************** <command> ****************************
			else if( boost::regex_match(sbuf, re_cmd_begin) )
				if( task_started )
				{
					bool cmd_finished = false;
					do{
						fin.getline(buf, BUF_SIZE);
						size_t len = fin.gcount();
						size_t cursor = 0;

						for( ; cursor < len && is_ws(buf[cursor]); ++cursor);
						if(cursor == len || buf[cursor] == '#')
							continue;

						char * cursor_ptr = buf + cursor;
						const std::string sbuf(cursor_ptr);

						if( boost::regex_match(sbuf, re_cmd_end) )
							cmd_finished = true;
						else
							temp.add_command(sbuf);

					}while(!fin.eof() && !cmd_finished);

					if( !cmd_finished )
						throw simple_exception("Error : unexpected end of file in command section");
				}
				else
					std::cerr << "Warning : command definition out of task will be ignored";

			//******************************** <input> ****************************
			else if( boost::regex_match(sbuf, re_inp_begin) )
				if( task_started )
				{
					bool inp_finished = false;
					do{
						fin.getline(buf, BUF_SIZE);
						size_t len = fin.gcount();
						size_t cursor = 0;

						for( ; cursor < len && is_ws(buf[cursor]); ++cursor);
						if(cursor == len || buf[cursor] == '#')
							continue;

						char * cursor_ptr = buf + cursor;
						const std::string sbuf(cursor_ptr);

						if( boost::regex_match(sbuf, re_inp_end) )
							inp_finished = true;
						else
							if( boost::regex_match(sbuf, smatch_res, re_filenames1) || boost::regex_match(sbuf, smatch_res, re_filenames2) )
								temp.add_input_file(smatch_res[2], smatch_res[3]);
							else
								std::cerr << "Warning : unrecognized string " << sbuf << std::endl;

					}while(!fin.eof() && !inp_finished);

					if( !inp_finished )
						throw simple_exception("Error : unexpected end of file in input section");
				}
				else
					std::cerr << "Warning : input definition out of task will be ignored";

			//******************************** <output> ****************************
			else if( boost::regex_match(sbuf, re_outp_begin) )
				if( task_started )
				{
					bool outp_finished = false;
					do{
						fin.getline(buf, BUF_SIZE);
						size_t len = fin.gcount();
						size_t cursor = 0;

						for( ; cursor < len && is_ws(buf[cursor]); ++cursor);
						if(cursor == len || buf[cursor] == '#')
							continue;

						char * cursor_ptr = buf + cursor;
						const std::string sbuf(cursor_ptr);

						if( boost::regex_match(sbuf, re_outp_end) )
							outp_finished = true;
						else
							if( boost::regex_match(sbuf, smatch_res, re_filenames1) || boost::regex_match(sbuf, smatch_res, re_filenames2) )
								temp.add_output_file(smatch_res[2], smatch_res[3]);
							else
								std::cerr << "Warning : unrecognized string " << sbuf << std::endl;

					}while(!fin.eof() && !outp_finished);

					if( !outp_finished )
						throw simple_exception("Error : unexpected end of file in output section");
				}
				else
					std::cerr << "Warning : output definition out of task will be ignored";

			//******************************** <os> ****************************
			else if( boost::regex_match(sbuf, re_os_begin) )
				if( task_started )
				{
					bool os_finished = false;
					do{
						fin.getline(buf, BUF_SIZE);
						size_t len = fin.gcount();
						size_t cursor = 0;

						for( ; cursor < len && is_ws(buf[cursor]); ++cursor);
						if(cursor == len || buf[cursor] == '#')
							continue;

						char * cursor_ptr = buf + cursor;
						const std::string sbuf(cursor_ptr);

						if( boost::regex_match(sbuf, re_os_end) )
							os_finished = true;
						else
							temp.set_os(sbuf);

					}while(!fin.eof() && !os_finished);

					if( !os_finished )
						throw simple_exception("Error : unexpected end of file in os section");
				}
				else
					std::cerr << "Warning : os definition out of task will be ignored";
			else
				std::cerr << "Warning : unrecognized string " << sbuf << std::endl;
		}

		if( task_started && task_finished )
		{
			gt = temp;
			return true;
		}
		else
			return false;
	}
	catch( std::exception &ex){
		std::cerr << ex.what() << std::endl;
	}
	return false;
} 

