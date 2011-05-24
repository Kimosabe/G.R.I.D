#ifndef GRID_NODE_H_
#define GRID_NODE_H_

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif //_WIN32_WINNT

#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/noncopyable.hpp>
#include <vector>
#include <string>
#include <stack>
#include "file_transferer.h"

class grid_task;

class grid_node : private boost::noncopyable{
private:
	boost::asio::io_service &io_serv;
	boost::asio::ip::tcp::socket socket_;
	std::string address;
	file_transferer file_transf;
	boost::asio::streambuf streambuf_;
	bool active;

	const static size_t maxsize = 4096;
	char buf[maxsize];

	void handle_connect(const boost::system::error_code& err,
		boost::asio::ip::tcp::resolver::iterator endpoint_iterator,
		std::stack<int> &ports);

	void start();
	void handle_read(const boost::system::error_code& error, size_t bytes_transferred);
public:
	grid_node(boost::asio::io_service& io_serv_, const std::string &address_, 
		const std::stack<int> &ports_);
	virtual ~grid_node();

	bool is_active() const			{ return active; };

	//these 3 functions should be removed in release
	//------------------------------------------------
	bool send_file(const std::string &local_name, const std::string &remote_name);
	bool send_command(const std::string &command);
	bool request_file(const std::string &local_name, const std::string &remote_name);
	//------------------------------------------------

	void apply_task(const grid_task &task);
};

typedef boost::shared_ptr<grid_node> node_ptr;

#endif //GRID_NODE_H_
