#pragma once
#include <string>
#include <boost/asio.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/bind.hpp>

std::string makeDaytimeString();

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
class TCPConnection : public boost::enable_shared_from_this<TCPConnection>
{
public:
	typedef boost::shared_ptr<TCPConnection> pointer;
	static pointer create(boost::asio::io_service& ioService)
	{
		return pointer(new TCPConnection(ioService));
	}
	boost::asio::ip::tcp::socket& getSocketRef()
	{
		return socket;
	}
	void start()
	{
		message = makeDaytimeString();
		boost::asio::async_write(socket, boost::asio::buffer(message),
			boost::bind(&TCPConnection::handleWrite, boost::shared_from_this()/*,
																			  boost::asio::placeholders::error,
																			  boost::asio::placeholders::bytes_transferred*/));
	}

private:
	TCPConnection(boost::asio::io_service& ioService) : socket(ioService){}
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
			13))
	{
		startAccept();
	}
	void startAccept()
	{
		TCPConnection::pointer newConnection = TCPConnection::create(acceptor.get_io_service());
		acceptor.async_accept(newConnection->getSocketRef(),
			boost::bind(&TCPServer::handleAccept, this, newConnection,
			boost::asio::placeholders::error));
	}
	void handleAccept(TCPConnection::pointer newConnection,
		const boost::system::error_code& error)
	{
		if (!error)
			newConnection->start();
		startAccept();
	}

private:
	boost::asio::ip::tcp::acceptor acceptor;
};
