#ifndef _SERVER_H
#define _SERVER_H

#include "session.h"
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/noncopyable.hpp>

class server : private boost::noncopyable {
public:
	server(boost::asio::io_service& io_service, const short port);
	virtual ~server();

	void run();
private:
	boost::asio::io_service& io_service_;
	boost::asio::ip::tcp::acceptor acceptor_;

	void handle_accept(session* new_session, const boost::system::error_code& error);
};

typedef boost::shared_ptr<server> server_ptr;

#endif //_SERVER_H
