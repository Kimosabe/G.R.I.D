#ifndef _KIMO_TRANSACTION_H
#define _KIMO_TRANSACTION_H

#include <string>

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/streambuf.hpp>

namespace Kimo
{
class Transaction
{
public:
	Transaction();
	~Transaction();

	void set_name(const char* name);
	bool connect(const std::string& address, short port);
	bool begin(time_t timestamp);
	bool transfer(const char* data, size_t size);
	bool end();

private:
	Transaction(const Transaction&);
	Transaction& operator=(const Transaction&);

	bool get_response();

	std::string m_name;
	boost::asio::io_service m_io_service;
	boost::asio::ip::tcp::socket m_socket;
	boost::asio::streambuf m_streambuf;
};
}
#endif // _KIMO_TRANSACTION_H