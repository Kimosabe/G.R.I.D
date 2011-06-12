#include <iostream>
#include <sstream>

#include <boost/asio/read_until.hpp>
#include <boost/asio/write.hpp>
#include <boost/regex.hpp>
#include <boost/lexical_cast.hpp>

#include "transaction.h"

namespace Kimo
{
	Transaction::Transaction()
	: m_io_service(), m_socket(m_io_service)
	{
	}

	Transaction::~Transaction()
	{
	}

	void Transaction::set_name(const char* name)
	{
		m_name = name;
	}

	bool Transaction::connect(const std::string& address, short port)
	{
		boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::address::from_string(address), port);
		boost::system::error_code err;

		m_socket.connect(endpoint, err);
		if (err)
			return true;

		return false;
	}

	bool Transaction::begin(time_t timestamp)
	{
		if (!m_socket.is_open())
			return true;

		std::string msg = std::string("<transaction \"") + m_name + std::string("\" begin \"") +
			boost::lexical_cast<std::string>(timestamp) + std::string("\">\v");
		size_t bytes = boost::asio::write(m_socket, boost::asio::buffer(msg.data(), msg.size()));

		if (bytes != msg.size())
			return true;

		return get_response();
	}

	bool Transaction::transfer(char* data, size_t size)
	{
		if (!m_socket.is_open())
			return true;

		std::string msg = std::string("<transaction \"") + m_name + std::string("\" data \"");
		msg.append(data, size);
		msg += "\">\v";
		size_t bytes = boost::asio::write(m_socket, boost::asio::buffer(msg.data(), msg.size()));

		if (bytes != msg.size())
			return true;

		return get_response();
	}

	bool Transaction::end()
	{
		if (!m_socket.is_open())
			return true;

		std::string msg = std::string("<transaction \"") + m_name + std::string("\" end>\v");
		size_t bytes = boost::asio::write(m_socket, boost::asio::buffer(msg.data(), msg.size()));

		if (bytes != msg.size())
			return true;

		return get_response();
	}

	bool Transaction::get_response()
	{
		m_streambuf.consume(m_streambuf.size());
		size_t bytes_transferred = boost::asio::read_until(m_socket, m_streambuf, '\v');

		if(!bytes_transferred)
			return true;

		std::istream ss(&m_streambuf);
		std::string request;

		if(ss.eof())
			return true;

		const boost::regex re_status("(?xs)(^<transaction \\s+ \"(.+)\" \\s+ status \\s+ \"(.+)\">$)");
		std::getline(ss, request, '\v');

		boost::smatch match_res;
		if(!boost::regex_match(request, match_res, re_status) )
		{
			std::cerr << "Unknown request: " << request << std::endl;
			return true;
		}

		std::string name = match_res[2], status = match_res[3];
		if (name != m_name)
			return true;
		if (status != "ok")
			return true;

		return false;
	}
} // namespace Kimo