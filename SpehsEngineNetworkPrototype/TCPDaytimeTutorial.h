#pragma once
#include <string>
#include <boost/asio/io_service.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/bind.hpp>
#include <boost/asio/placeholders.hpp>
#include <boost/asio/socket_acceptor_service.hpp>
#include <boost/asio.hpp>
#include "MakeDaytimeString.h"
#include "Network.h"


class TCPDaytimeTutorial
{
public:
	TCPDaytimeTutorial();
	~TCPDaytimeTutorial();

	
	//Tutorial 1
	void tutorial1(std::string serverName);

	//Tutorial 2
	void tutorial2(std::string serverName);

	//Tutorial 3
	void tutorial3(std::string serverName);
};





//Tutorial 3
class TCPConnectionTut : public boost::enable_shared_from_this<TCPConnectionTut>
{
public:
	typedef boost::shared_ptr<TCPConnectionTut> pointer;
	static pointer create(boost::asio::io_service& ioService)
	{
		return pointer(new TCPConnectionTut(ioService));
	}
	boost::asio::ip::tcp::socket& getSocketRef()
	{
		return socket;
	}

	/*In the function start(), we call boost::asio::async_write() to serve the data to the client.
	Note that we are using boost::asio::async_write(), rather than ip::tcp::socket::async_write_some(),
	to ensure that the entire block of data is sent. */
	void start()
	{
		message = makeDaytimeString();

		/*When initiating the asynchronous operation, and if using boost::bind(),
		you must specify only the arguments that match the handler's parameter list.
		In this program, both of the argument placeholders (boost::asio::placeholders::error and
		boost::asio::placeholders::bytes_transferred) could potentially have been removed,
		since they are not being used in handle_write(). */
		boost::asio::async_write(socket, boost::asio::buffer(message),
			boost::bind(&TCPConnectionTut::handleWrite, shared_from_this()/*,
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred*/));
		/*Any further actions for this client connection are now the responsibility of handle_write().*/
	}

private:
	TCPConnectionTut(boost::asio::io_service& ioService) : socket(ioService){}
	void handleWrite(/*const boost::system::error_code& error, size_t bytesTransferred*/)
	{
	}

	std::string message;
	boost::asio::ip::tcp::socket socket;
};
class TCPServer
{
public:
	TCPServer(boost::asio::io_service& io_service) :
		acceptor(
			io_service,
			boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(),
			PORT_NUMBER_TCP))
	{
		startAccept();
	}
	void startAccept()
	{
		TCPConnectionTut::pointer newConnection = TCPConnectionTut::create(acceptor.get_io_service());
		acceptor.async_accept(newConnection->getSocketRef(),
			boost::bind(&TCPServer::handleAccept, this, newConnection,
			boost::asio::placeholders::error));
	}

	/*The function handle_accept() is called when the asynchronous accept operation initiated by start_accept() finishes.
	It services the client request, and then calls start_accept() to initiate the next accept operation. */
	void handleAccept(TCPConnectionTut::pointer newConnection,
		const boost::system::error_code& error)
	{
		if (!error)
			newConnection->start();
		startAccept();
	}

private:
	boost::asio::ip::tcp::acceptor acceptor;
};
