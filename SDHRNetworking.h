#pragma once

//////////////////////////////////////////////////////////////////////////
//
// Utility header file to provide cross-platform initialization of networking
// It just sets up the server file descriptor given a server sockaddr
//
//////////////////////////////////////////////////////////////////////////


#if defined(__MINGW32__) || defined(__MINGW64__) || defined(_WIN32) || defined(_WIN64)
#define __NETWORKING_WINDOWS__
#endif

#include <iostream>
#include <cstring>

#ifdef __NETWORKING_WINDOWS__
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
typedef SOCKET        __SOCKET;
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
typedef int        __SOCKET;
#endif

#pragma pack(push, 1)
struct SDHRPacket {
	uint16_t addr;
	uint8_t data;
	uint8_t pad;
};
#pragma pack(pop)

enum class ENET_RES
{
	OK = 0,
	ERR = 1
};

#define CXSDHR_CTRL 0xC0B0	// SDHR command
#define CXSDHR_DATA 0xC0B1	// SDHR data

// Call this method as a new thread
// It loops infinitely and waits for packets
// It instantly updates memory, and only updates
// other data when a PROCESS_COMMAND packet arrives
int socket_server_thread(uint16_t port);

// set this to true to terminate the networking thread
// and then call socket_unblock_accept()
static bool bShouldTerminateNetworking = false;
bool socket_unblock_accept(uint16_t port);
