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

#define PKT_BUFSZ 2048

#pragma pack(push, 1)

struct Packet {
	uint8_t data[PKT_BUFSZ];
	uint32_t size;
	Packet() : size(1) {
		memset(data, 0, 1);
	}
};

struct SDHRPacketHeader {
	uint32_t seqno;
	uint32_t cmdtype;
};

#pragma pack(pop)

struct SDHREvent {
    bool is_iigs;   // 2gs == 1, 2e == 0
    bool m2b0; 
	bool m2sel;
	bool rw;        // read == 1, write == 0
	uint16_t addr;
	uint8_t data;
	SDHREvent(bool is_iigs_, bool m2b0_, bool m2sel_, bool rw_, uint16_t addr_, uint8_t data_) :
		is_iigs(is_iigs_), m2b0(m2b0_), m2sel(m2sel_), rw(rw_), addr(addr_), data(data_) {}
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
int usb_server_thread(uint16_t port, bool* shouldTerminateNetworking);

// Call this method as a new thread
// It loops indefinitely and processes the packets queue
// Each packet contains a minumum of 64 events.
// If the events are SDHR data, it appends them to a command_buffer
// When it parses a SDHR_PROCESS_EVENTS event, it calls SDHRManager
// which itself processes the command_buffer
int process_usb_events_thread(bool* shouldTerminateProcessing);
void process_single_event(SDHREvent& e);
void terminate_processing_thread();

void clear_queues();

const uint64_t get_number_packets_processed();
const uint64_t get_duration_packet_processing_ns();
const uint64_t get_duration_network_processing_ns();
const size_t get_packet_pool_count();
const size_t get_max_incoming_packets();
const bool client_is_connected();
