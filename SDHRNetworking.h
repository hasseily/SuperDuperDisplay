#pragma once

//////////////////////////////////////////////////////////////////////////
//
// Utility header file to provide cross-platform initialization of networking
// It just sets up the server file descriptor given a server sockaddr
//
//////////////////////////////////////////////////////////////////////////


#if defined(__MINGW32__) || defined(__MINGW64__) || defined(_WIN32) || defined(_WIN64)
#define __NETWORKING_WINDOWS__
#elif defined(__APPLE__)
#define __NETWORKING_APPLE__
#elif defined(__linux) || defined(__linux__) || defined(linux) || defined(__gnu_linux__) ||    defined(__GNUC__)
#define __NETWORKING_LINUX__
#else
#error "OS NOT SUPPORTED"
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
#include <arpa/inet.h>
#include <unistd.h>
typedef int        __SOCKET;
#ifdef __NETWORKING_APPLE__
//#include <sys/uio.h>
struct mmsghdr {
    struct msghdr msg_hdr;  // The standard msghdr structure
    unsigned int  msg_len;  // Number of bytes received or sent
};
#endif
#endif

#pragma pack(push, 1)
struct SDHRPacketHeader {
	uint32_t seqno;
	uint32_t cmdtype;
};

#pragma pack(pop)

struct SDHREvent {
    bool is_iigs;   // 2gs == 1, 2e == 0
	bool rw;        // read == 1, write == 0
	uint16_t addr;
	uint8_t data;
	SDHREvent(bool is_iigs_, bool rw_, uint16_t addr_, uint8_t data_) :
		is_iigs(is_iigs_),
		rw(rw_),
		addr(addr_),
		data(data_) {}
};

enum class ENET_RES
{
	OK = 0,
	ERR = 1
};

#define CXSDHR_CTRL 0xC0A0	// SDHR command
#define CXSDHR_DATA 0xC0A1	// SDHR data

// Call this method as a new thread
// It loops infinitely and waits for packets
// And puts it in an events queue
int socket_server_thread(uint16_t port, bool* shouldTerminateNetworking);

// Call this method as a new thread
// It loops indefinitely and processes the events queue
// If the events are SDHR data, it appends them to a command_buffer
// When it parses a SDHR_PROCESS_EVENTS event, it calls SDHRManager
// which itself processes the command_buffer
int process_events_thread(bool* shouldTerminateProcessing);
void terminate_processing_thread();
