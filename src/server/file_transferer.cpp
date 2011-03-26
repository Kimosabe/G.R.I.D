#include "file_transferer.h"
#include <memory.h>

const boost::regex file_transferer::re_download = boost::regex("(?x)(^<file \\s+ parts=(\\d+) \\s+ partsize=(\\d+) \\s+ lastpartsize=(\\d+) \\s+ name=\"( (?: [^ \" \\\\ / \\* \\? : > < \\| ] )+ )\" >$)");

const boost::regex file_transferer::re_upload = boost::regex("(?x)(^<upload_request \\s+ local_name=\"( (?: [^ \" \\\\ / \\* \\? : > < \\| ] )+ )\" \\s+ remote_name=\"( (?: [^ \" \\\\ / \\* \\? : > < \\| ] )+ )\" >$)");

file_transferer::file_transferer() : file_data(NULL), parts(0), part_size(0), last_part_size(0),
		file_part_sizes()
{
}

file_transferer::file_transferer(const file_transferer &ft) : parts(ft.parts), part_size(ft.part_size),
	last_part_size(ft.last_part_size), file_part_sizes(ft.file_part_sizes)
{
	if( part_size > 0 )
	{
		file_data = new char[part_size];
		memcpy(file_data, ft.file_data, part_size);
	}
	else
		file_data = NULL;
}

file_transferer::~file_transferer()
{
	delete[] file_data;
}

bool file_transferer::is_download_request(const std::string &request) const
{
	return boost::regex_match(request, re_download);
}

bool file_transferer::is_upload_request(const std::string &request, std::string &local_name, std::string &remote_name) const
{
	boost::smatch smatch_res;
	if( boost::regex_match(request, smatch_res, re_upload) )
	{
		local_name = smatch_res[2];		remote_name = smatch_res[3];
		return true;
	}
	else
		return false;
}

bool file_transferer::request_file(const std::string &local_name, const std::string &remote_name, boost::asio::ip::tcp::socket &socket_)
{
	try{
		//меняем ме стами local_name и remote_name, потому что на удаленном хосте все будет наоборот
		const std::string header = std::string("<upload_request local_name=\"") + remote_name + 
			std::string("\" remote_name=\"") + local_name + std::string("\">\n");

		boost::asio::write(socket_, boost::asio::buffer(header, header.size()));
		return true;
	}
	catch(const std::exception &ex){
		std::cerr << ex.what() << std::endl;
	}
	return false;
}

bool file_transferer::recieve_file(const std::string &request, boost::asio::ip::tcp::socket &socket_)
{
	using namespace std;
	try{
		boost::smatch match_res;
		if( ! boost::regex_match(request, match_res, re_download) ) return false;

#if defined(_DEBUG) || defined(DEBUG)
		cout << "recieving " << match_res[2] << " parts, partsize " << match_res[3] 
			<< " lastpartsize " << match_res[4] << " of " << match_res[5] << endl;
#endif

		parts = boost::lexical_cast<size_t>(match_res[2]);
		part_size = boost::lexical_cast<size_t>(match_res[3]);
		last_part_size = boost::lexical_cast<size_t>(match_res[4]);

		if( parts == 0 ) return false;

		const string fname(match_res[5]);
		ofstream fout(fname.c_str(), ios_base::binary);
		if( !fout ){
			string errmes = string("Error : cannot open ") + fname;
			throw exception(errmes.c_str());
		}

		delete[] file_data;
		file_data = new char[part_size];

		for( size_t i = 0; i < parts - 1; ++i )
		{
			boost::asio::read(socket_, boost::asio::buffer(file_data, part_size));
			fout.write(file_data, part_size);
		}
		boost::asio::read(socket_, boost::asio::buffer(file_data, last_part_size));
		fout.write(file_data, last_part_size);

		/*
		file_part_sizes.resize(parts);
		for( std::vector<size_t>::iterator i=file_part_sizes.begin(); i<file_part_sizes.end(); ++i )
			*i = part_size;
		if( last_part_size < file_part_sizes[parts-1] )
			file_part_sizes[parts-1] = last_part_size;
		*/

		fout.close();
		return true;
	}
	catch( exception &ex ){
		cerr << ex.what() << endl;
	}
	return false;
}

bool file_transferer::send_file(const std::string &local_name, const std::string &remote_name,
								boost::asio::ip::tcp::socket &socket_)
{
	using namespace std;
	try{
		ifstream fin(local_name.c_str(), ios_base::binary | ios_base::ate);
		if( !fin ){
			string errmes = string("Error : cannot open file ") + local_name;
			throw std::exception(errmes.c_str());
		}

		const size_t file_size = fin.tellg();
		//64 kb
		const static size_t buf_size = 65536;

		char buf[buf_size];
		const size_t parts = file_size/buf_size + ( file_size%buf_size ? 1 : 0 );
		const size_t last_part_size = file_size%buf_size ? file_size%buf_size : buf_size; 

		const string header = string("<file parts=") + boost::lexical_cast<string>(parts)
			+ string(" partsize=") + boost::lexical_cast<string>(buf_size)
			+ string(" lastpartsize=") + boost::lexical_cast<string>(last_part_size)
			+ string(" name=\"") + remote_name + string("\">\n");

		fin.seekg(ios_base::beg);
		boost::asio::write(socket_, boost::asio::buffer(header, header.size()));
		for( size_t i = 0; i < parts; ++i )
		{
			boost::system::error_code erc;
			fin.read(buf, buf_size);
			size_t bytes_readed = fin.gcount();

#if defined(_DEBUG) || defined(DEBUG)
			cout << "sending " << bytes_readed << " bytes\n";
			cout.flush();
#endif

			boost::asio::write(socket_, boost::asio::buffer(buf, bytes_readed));
		}

		fin.close();
		return true;
	}
	catch(const std::exception &ex){
		std::cerr << ex.what() << std::endl;
	}
	return false;
}

