#include <iostream>
#include <thread>
#include <boost/bind.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/placeholders.hpp>
#include <boost/shared_ptr.hpp>
#include <SpehsEngine/Console.h>
#include <SpehsEngine/BitwiseOperations.h>
#include <SpehsEngine/Time.h>
#include <SpehsEngine/ApplicationData.h>
#include "GameServer.h"
#include "MakeDaytimeString.h"
#define PI 3.14159265359f
#define PERFORMANCE_TEST_OBJECT_COUNT 500









GameServer::GameServer() :
	acceptorTCP(ioService, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), PORT_NUMBER_TCP)),
	socketUDP(ioService, boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), PORT_NUMBER_UDP)),
	objectDataUDP{}, state (0)
{
	//Performance test
	objectMutex.lock();
	for (int i = 0; i < PERFORMANCE_TEST_OBJECT_COUNT; i++)
	{
		float angle = float(i) / float(PERFORMANCE_TEST_OBJECT_COUNT) * 2.0f * PI;
		objects.push_back(new Object);
		objects.back()->ID = i + 1;
		objects.back()->x = cos(angle) * float(applicationData->getWindowHeightHalf()) * float(i) / float(PERFORMANCE_TEST_OBJECT_COUNT) + applicationData->getWindowWidthHalf();
		objects.back()->y = sin(angle) * float(applicationData->getWindowHeightHalf()) * float(i) / float(PERFORMANCE_TEST_OBJECT_COUNT) + applicationData->getWindowHeightHalf();
	}
	objectMutex.unlock();
}
GameServer::~GameServer()
{
	for (unsigned i = 0; i < objects.size(); i++)
		delete objects[i];
	for (unsigned i = 0; i < newObjects.size(); i++)
		delete newObjects[i];
	for (unsigned i = 0; i < removedObjects.size(); i++)
		delete removedObjects[i];
}
/////////////////
// MAIN THREAD //
void GameServer::run()
{
	startReceiveTCP();
	startReceiveUDP();

	std::thread ioServiceThread(boost::bind(&boost::asio::io_service::run, &ioService));
	while (!checkBit(state, SERVER_EXIT_BIT))
	{
		//Object pre update
		//Object update
		update();
		//Object post update
		//Chunk update
		//Physics update
		sendUpdateData();
	}
	ioServiceThread.join();
}
void GameServer::update()
{
	std::lock_guard<std::recursive_mutex> objectLockGuardMutex(objectMutex);
	static float a = 0.0f;
	a += spehs::deltaTime / 50000.0f;
	if (a > 2 * PI)
		a = 0;
	for (int i = 0; i < PERFORMANCE_TEST_OBJECT_COUNT; i++)
	{
		float angle = float(i) / float(PERFORMANCE_TEST_OBJECT_COUNT) * 2.0f * PI + a;
		objects[i]->x = cos(angle) * float(applicationData->getWindowHeightHalf()) * float(i) / float(PERFORMANCE_TEST_OBJECT_COUNT) + applicationData->getWindowWidthHalf();
		objects[i]->y = sin(angle) * float(applicationData->getWindowHeightHalf()) * float(i) / float(PERFORMANCE_TEST_OBJECT_COUNT) + applicationData->getWindowHeightHalf();
	}
}
void GameServer::sendUpdateData()
{
	std::lock_guard<std::recursive_mutex> objectLockGuardMutex(objectMutex);

	//TCP packet
	std::vector<unsigned char> packetTCP;
	size_t offset = 0;
	if (newObjects.size() > 0)
	{
		packetTCP.resize(
			//Create object
			sizeof(unsigned char/*packet type*/) + sizeof(unsigned/*object count*/) + newObjects.size() * sizeof(ObjectData));
		packetTCP[offset] = packet::createObj;
		offset += sizeof(unsigned char);
		unsigned size = newObjects.size();
		memcpy(&packetTCP[offset], &size, sizeof(unsigned));
		offset += sizeof(unsigned);
		for (unsigned i = 0; i < size; i++)
		{
			//Create packet data
			ObjectData objectData;
			objectData.ID = newObjects[i]->ID;
			objectData.x = newObjects[i]->x;
			objectData.y = newObjects[i]->y;
			memcpy(&packetTCP[offset], &objectData, sizeof(ObjectData));
			offset += sizeof(ObjectData);

			//Push to objects
			objects.push_back(newObjects[i]);
		}

		//All new objects have been pushed to objects vector, clear newObjects vector
		newObjects.clear();
	}

	if (packetTCP.size() > 0)
	{
		std::lock_guard<std::recursive_mutex> clientLockGuardMutex(clientMutex);
		int activeClientCount = int(clients.size()) - 1;
		for (int i = 0; i < activeClientCount; i++)
		{
			clients[i]->socketMutex.lock();
			clients[i]->socket.send(boost::asio::buffer(packetTCP));
			clients[i]->socketMutex.unlock();
		}
	}

	//UDP packet
	offset = 0;
	//Create object data packet for each client
	objectDataUDP[offset] = packet::updateObj;
	offset += sizeof(packet::PacketType);
	//Write object count
	unsigned objectCount = objects.size();
	memcpy(&objectDataUDP[offset], &objectCount, sizeof(unsigned));
	offset += sizeof(objectCount);
	//Write objects
	for (unsigned i = 0; i < objects.size(); i++)
	{
		memcpy(&objectDataUDP[offset], objects[i], sizeof(ObjectData));
		offset += sizeof(ObjectData);
	}
	try
	{
		std::lock_guard<std::recursive_mutex> lockUdpSocket(udpSocketMutex);
		for (unsigned i = 0; i < clients.size(); i++)
		{
			if (clients[i]->udpEndpoint)
			{
				clients[i]->socketMutex.lock();
				socketUDP.send_to(boost::asio::buffer(objectDataUDP), *clients[i]->udpEndpoint);
				clients[i]->socketMutex.unlock();
			}
		}
	}
	catch (std::exception& e)
	{
		std::cerr << "\n" << e.what();
	}

}
/////////////////
/////////////////


/////////////////////////////
// IO SERVICE - RUN THREAD //
void GameServer::startReceiveTCP()
{
	std::lock_guard<std::recursive_mutex> clientLockGuardMutex(clientMutex);
	clients.push_back(Client::create(ioService, *this));
	//Give an ID for the client
	if (clients.size() > 1)
		clients.back()->ID = clients[clients.size() - 2]->ID + 1;//Take next ID
	else
		clients.back()->ID = 1;

	clients.back()->socketMutex.lock();
	acceptorTCP.async_accept(clients.back()->socket, boost::bind(&GameServer::handleAcceptClient, this, clients.back(), boost::asio::placeholders::error));
	clients.back()->socketMutex.unlock();
}
void GameServer::handleAcceptClient(boost::shared_ptr<Client> client, const boost::system::error_code& error)
{
	//Create object for player
	std::lock_guard<std::recursive_mutex> objectLockGuardMutex(objectMutex);
	newObjects.push_back(new Object());
	newObjects.back()->ID = objects.back()->ID + 1;
	
	std::lock_guard<std::recursive_mutex> clientLockGuardMutex(clientMutex);

	//Log enter
	clients.back()->socketMutex.lock();
	spehs::console::log("Client " + std::to_string(clients.back()->ID) + " [" + clients.back()->socket.remote_endpoint().address().to_string() + "] entered the game");
	clients.back()->socketMutex.unlock();

	//Data vector
	std::vector<unsigned char> packetTCP;

	//ID
	size_t offset = 0;
	packetTCP.resize(5/*packet type + uint32*/);

	packetTCP[offset] = packet::enterID;
	offset += sizeof(unsigned char);

	memcpy(&packetTCP[offset], &clients.back()->ID, sizeof(clients.back()->ID));
	offset += sizeof(clients.back()->ID);


	//Send all existing objects as createObj packet type
	packetTCP.resize(packetTCP.size() + sizeof(unsigned char/*packet type*/) + sizeof(unsigned/*object count*/) + objects.size() * sizeof(ObjectData));
	packetTCP[offset] = packet::createObj;
	offset += sizeof(unsigned char);

	unsigned size = objects.size();
	memcpy(&packetTCP[offset], &size, sizeof(unsigned));
	offset += sizeof(unsigned/*object count*/);

	for (unsigned i = 0; i < size; i++)
	{
		//Create packet data
		ObjectData objectData;
		objectData.ID = objects[i]->ID;
		objectData.x = objects[i]->x;
		objectData.y = objects[i]->y;
		memcpy(&packetTCP[offset], &objectData, sizeof(ObjectData));
		offset += sizeof(ObjectData);
	}

	//Start receiving again
	clients.back()->startReceiveTCP();
	clients.back()->socketMutex.lock();
	clients.back()->socket.send(boost::asio::buffer(packetTCP));
	clients.back()->socketMutex.unlock();
	startReceiveTCP();
	
}
void GameServer::clientExit(CLIENT_ID_TYPE ID)
{
	//Remove client from clients
	{
		std::lock_guard<std::recursive_mutex> clientLockGuardMutex(clientMutex);
		for (unsigned i = 0; i < clients.size(); i++)
		{
			if (clients[i]->ID == ID)
			{
				clients[i]->socketMutex.lock();
				spehs::console::log("Client " + std::to_string(clients[i]->ID) + " [" + clients[i]->socket.remote_endpoint().address().to_string() + "] exited the game");
				clients[i]->socketMutex.unlock();
				clients.erase(clients.begin() + i);
				break;
			}
		}
	}

	//Remove the object representing the client...
	{
		std::lock_guard<std::recursive_mutex> objectLockGuardMutex(objectMutex);
		for (unsigned i = 0; i < objects.size(); i++)
		{
			if (objects[i]->ID == ID)
			{
				removedObjects.push_back(objects[i]);
				objects.erase(objects.begin() + i);
				break;
			}
		}
	}
}
void GameServer::startReceiveUDP()
{
	std::lock_guard<std::recursive_mutex> lockUdpSocket(udpSocketMutex);
	socketUDP.async_receive_from(
		boost::asio::buffer(receiveBufferUDP), senderEndpointUDP,
		boost::bind(&GameServer::handleReceiveUDP, this,
		boost::asio::placeholders::error,
		boost::asio::placeholders::bytes_transferred));
}
void GameServer::handleReceiveUDP(const boost::system::error_code& error, std::size_t bytesTransferred)
{
	if (!error || error == boost::asio::error::message_size)
	{
		if (LOG_NETWORK)
			spehs::console::log("Game server: data received");
		
		unsigned char type;
		memcpy(&type, &receiveBufferUDP[0], sizeof(unsigned char));
		switch (type)
		{
		default:
		case packet::invalid:
			spehs::console::error("Server: packet with invalid packet type received!");
			break;
		case packet::enterUdpEndpoint:
		{
			clientMutex.lock();
			CLIENT_ID_TYPE id;
			memcpy(&id, &receiveBufferUDP[1], sizeof(id));
			std::lock_guard<std::recursive_mutex> lockUdpSocket(udpSocketMutex);
			for (unsigned i = 0; i < clients.size(); i++)
			{
				if (clients[i]->ID == id && !clients[i]->udpEndpoint)
				{
					clients[i]->udpEndpoint = new boost::asio::ip::udp::endpoint;
					*clients[i]->udpEndpoint = senderEndpointUDP;
					boost::array<unsigned char, 2> answer = { packet::enterUdpEndpointReceived, 1 };
					socketUDP.send_to(boost::asio::buffer(answer), senderEndpointUDP);//HACK: send 3 times to client for better chance of packet arrival
					socketUDP.send_to(boost::asio::buffer(answer), senderEndpointUDP);
					socketUDP.send_to(boost::asio::buffer(answer), senderEndpointUDP);
					break;
				}
			}
			clientMutex.unlock();
		}
			break;
		case packet::updateInput:
			receiveUpdate();
			break;
		}
	}

	//Start receiving again
	startReceiveUDP();
}
void GameServer::receiveUpdate()
{
	std::lock_guard<std::recursive_mutex> objectLockGuardMutex(objectMutex);

	//Mem copy so that packet becomes more readable...
	memcpy(&playerStateDataBufferUDP[0], &receiveBufferUDP[0], sizeof(PlayerStateData));

	//Do something to objects with player input...
	for (unsigned i = 0; i < objects.size(); i++)
	{
		if (objects[i]->ID == playerStateDataBufferUDP[0].ID)
		{
			objects[i]->x = playerStateDataBufferUDP[0].mouseX;
			objects[i]->y = playerStateDataBufferUDP[0].mouseY;
			if (LOG_NETWORK)
				spehs::console::log(
				"Object " + std::to_string(objects[i]->ID) + " position: " +
				std::to_string(objects[i]->x) + ", " + std::to_string(objects[i]->y));
			break;
		}
	}
}
//CLIENT (run from io service thread)
Client::Client(boost::asio::io_service& io, GameServer& s) : socket(io), server(s), udpEndpoint(nullptr){}
Client::~Client()
{
	if (udpEndpoint)
		delete udpEndpoint;
}
void Client::startReceiveTCP()
{
	socketMutex.lock();
	socket.async_receive(boost::asio::buffer(receiveBuffer), boost::bind(&Client::receiveHandler, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
	socketMutex.unlock();
}
void Client::receiveHandler(const boost::system::error_code& error, std::size_t bytesTransferred)
{
	if (bytesTransferred < 1)
		return;//Must have transferred at least 1 byte (packet type)
	if (error)
		return;//TODO

	bool restartToReceive = true;
	switch (receiveBuffer[0])
	{
	default:
		spehs::console::warning("Client" + std::to_string(ID) + " received invalid packet type (TCP)!");
		break;
	case packet::enter:
		//Enter practically useless packet type, enter server handled by acceptor
		break;
	case packet::exit:
		server.clientExit(ID);
		restartToReceive = false;
		break;
	}

	if (restartToReceive)
		startReceiveTCP();
}
/////////////////////////////
/////////////////////////////