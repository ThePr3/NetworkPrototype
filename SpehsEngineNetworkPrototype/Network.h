#pragma once
#include <stdint.h>
#define SERVER_SIZE 5
#define PORT_NUMBER_TCP 41624
#define PORT_NUMBER_UDP 41625
#define LOG_NETWORK false
#define TCP_DATAGRAM_SIZE 16000//65527
#define UDP_DATAGRAM_SIZE 32000//65527
//41624
//13
namespace packet
{
	/**Packet type:
	In most cases, if packet comes from client it defines the query type.
	Correspondingly a packet of certain type from server represents a response to given query type.*/
	enum PacketType : unsigned char
	{
		invalid = 0,
		//UDP
		update,
		//TCP
		enter,
		enterID,
		exit,
		createObj,
		createPart,
		destroyObj,
		destroyPart,
		chatIn,
		chatOut,
	};
}


/**Structure for containing all player state data*/
struct PlayerStateData
{
	PlayerStateData() : type(packet::update){}
	unsigned char type;
	uint32_t ID;

	//Input data
	int16_t mouseX;
	int16_t mouseY;
};
struct ObjectData
{
	uint32_t ID;
	int16_t x;
	int16_t y;
};
