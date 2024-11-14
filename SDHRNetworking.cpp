#include "ConcurrentQueue.h"
#include "SDHRNetworking.h"
#include "MemoryManager.h"
#include "A2VideoManager.h"
#include "SoundManager.h"
#include "MockingboardManager.h"
#include "SDHRManager.h"
#include "CycleCounter.h"
#include "EventRecorder.h"
#include <time.h>
#include <fcntl.h>
#include <chrono>


#ifdef __NETWORKING_WINDOWS__
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#pragma comment(lib, "Ws2_32.lib")
typedef SOCKET        __SOCKET;
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <poll.h>
typedef int        __SOCKET;
#ifdef __NETWORKING_APPLE__
//#include <sys/uio.h>
struct mmsghdr {
	struct msghdr msg_hdr;  // The standard msghdr structure
	unsigned int  msg_len;  // Number of bytes received or sent
};
#endif
#endif

static EventRecorder* eventRecorder;
static uint32_t prev_seqno;
static bool bFirstDrop;
static bool bIsConnected = false;
static uint64_t num_processed_packets = 0;
static uint64_t duration_packet_processing_ns = 0;
static uint64_t duration_network_processing_ns = 0;

static ConcurrentQueue<std::shared_ptr<Packet>> packetInQueue;
static ConcurrentQueue<std::shared_ptr<Packet>> packetFreeQueue;

const uint64_t get_number_packets_processed() { return num_processed_packets; };
const uint64_t get_duration_packet_processing_ns() { return duration_packet_processing_ns; };
const uint64_t get_duration_network_processing_ns() { return duration_network_processing_ns; };
const size_t get_packet_pool_count() { return packetFreeQueue.max_size(); };
const size_t get_max_incoming_packets() { return packetInQueue.max_size(); };

const bool client_is_connected()
{
	return bIsConnected;
}

void clear_queues()
{
	packetInQueue.clear();
	packetFreeQueue.clear();
	for (size_t allocSize = 0; allocSize < 10000; allocSize++)	// preallocate 10,000 packets
	{
		auto packet = std::make_shared<Packet>();
		packetFreeQueue.push(std::move(packet));
	}
}

void insert_event(SDHREvent* e)
{
	(void)e;	// mark as unused
	assert("ERROR: CANNOT INSERT EVENT");
}

void terminate_processing_thread()
{
	// Force a dummy packet to process, so that shouldTerminateProcessing is triggered
	// and the loop is closed cleanly.
	auto packet = std::make_shared<Packet>();
	packetInQueue.push(std::move(packet));
}

void process_single_event(SDHREvent& e)
{
	/*
		Uncomment the below code to log specific events between 2 gates at 03FE and 03FF
		For example, this would log all when the PC is between 0304 and 0308

		0300  F8                         SED
		0301  8D FE 03                   STA $03FE
		0304  69 55                      ADC #$55
		0306  E9 55                      SBC #$55
		0308  8D FF 03                   STA $03FF
		030B  60                         RTS
	*/
	/*
	static bool _should_debug = false;
	if (e.addr == 0x03fe)
		_should_debug = true;
	if (e.addr == 0x03ff)
		_should_debug = false;
	if (_should_debug)
	{
		std::cout << e.m2sel << " " << e.rw << " " << std::hex << e.addr << " " << (uint32_t)e.data << std::endl;
	}
	*/

	// std::cout << e.is_iigs << " " << e.rw << " " << std::hex << e.addr << " " << (uint32_t)e.data << std::endl;
	
	eventRecorder = EventRecorder::GetInstance();
	if (eventRecorder->IsRecording())
		eventRecorder->RecordEvent(&e);
	// Update the cycle counting and VBL hit
	bool isVBL = ((e.addr == 0xC019) && e.rw && ((e.data >> 7) == (e.is_iigs ? 1 : 0)));
	CycleCounter::GetInstance()->IncrementCycles(1, isVBL);
	
	/*
	 *********************************
	 HANDLE SOUND AND PASSTHROUGH
	 *********************************
	 */
	auto soundMgr = SoundManager::GetInstance();
	soundMgr->EventReceived((e.addr & 0xFFF0) == 0xC030);
	
	/*
	 *********************************
	 HANDLE MOCKINGBOARD EVENTS
	 *********************************
	 */
	auto mockingboardMgr = MockingboardManager::GetInstance();
	mockingboardMgr->EventReceived(e.addr, e.data, e.rw);
	
	if (e.is_iigs && e.m2sel) {
		// ignore updates from iigs_mode firmware with m2sel high
		return;
	}
	if (e.rw && ((e.addr & 0xF000) != 0xC000)) {
		// ignoring all read events not softswitches
		return;
	}

	auto memMgr = MemoryManager::GetInstance();
	auto sdhrMgr = SDHRManager::GetInstance();
	auto a2VideoMgr = A2VideoManager::GetInstance();
	
	/*
	 *********************************
	 HANDLE SIMPLE MEMORY WRITE EVENTS
	 *********************************
	 */
	if ((e.addr >= _A2_MEMORY_SHADOW_BEGIN) && (e.addr < _A2_MEMORY_SHADOW_END)) {
		memMgr->WriteToMemory(e.addr, e.data, e.m2b0, e.is_iigs);
		return;
	}
	/*
	 *********************************
	 HANDLE SOFT SWITCHES EVENTS
	 *********************************
	 */
	// TODO: *** SDHR IS DISABLED FOR 2GS ***
	//		because we're getting spurious 0xC0A0 events from the GS
	if ((e.is_iigs == true) || ((e.addr != CXSDHR_CTRL) && (e.addr != CXSDHR_DATA))) {
		if (e.addr >> 8 == 0xc0)
			memMgr->ProcessSoftSwitch(e.addr, e.data, e.rw, e.is_iigs);
		// ignore non-control
		return;
	}
	/*
	 *********************************
	 HANDLE SDHR (0xC0A0/1) EVENTS
	 *********************************
	 */
	//std::cerr << "cmd " << e.addr << " " << (uint32_t) e.data << std::endl;
	SDHRCtrl_e _ctrl;
	switch (e.addr & 0x0f)
	{
		case 0x00:
			// std::cout << "This is a control packet!" << std::endl;
			_ctrl = (SDHRCtrl_e)e.data;
			switch (_ctrl)
			{
				case SDHR_CTRL_DISABLE:
#ifdef DEBUG
					std::cout << "CONTROL: Disable SDHR" << std::endl;
#endif
					sdhrMgr->ToggleSdhr(false);
					a2VideoMgr->ToggleA2Video(true);
					break;
				case SDHR_CTRL_ENABLE:
					//#ifdef DEBUG
					std::cout << "CONTROL: Enable SDHR" << std::endl;
					//#endif
					sdhrMgr->ToggleSdhr(true);
					a2VideoMgr->ToggleA2Video(false);
					break;
				case SDHR_CTRL_RESET:
					//#ifdef DEBUG
					std::cout << "CONTROL: Reset SDHR" << std::endl;
					//#endif
					sdhrMgr->ResetSdhr();
					break;
				case SDHR_CTRL_PROCESS:
				{
					/*
					 At this point we have a complete set of commands to process.
					 Wait for the main thread to finish loading any previous changes into the GPU, then process
					 the commands.
					 */
					
#ifdef DEBUG
					std::cout << "CONTROL: Process SDHR" << std::endl;
#endif
					while (sdhrMgr->dataState != DATASTATE_e::DATA_IDLE) {};
					bool processingSucceeded = sdhrMgr->ProcessCommands();
					sdhrMgr->dataState = DATASTATE_e::DATA_UPDATED;
					if (processingSucceeded)
					{
#ifdef DEBUG
						std::cout << "Processing SDHR succeeded!" << std::endl;
#endif
					}
					else {
						//#ifdef DEBUG
						std::cerr << "ERROR: Processing SDHR failed!" << std::endl;
						//#endif
					}
					sdhrMgr->ClearBuffer();
					break;
				}
				default:
					std::cerr << "ERROR: Unknown control packet type: " << std::hex << (uint32_t)e.data << std::endl;
					break;
			}
			break;
		case 0x01:
			// std::cout << "This is a data packet" << std::endl;
			sdhrMgr->AddPacketDataToBuffer(e.data);
			break;
		default:
			std::cerr << "ERROR: Unknown packet type: " << std::hex << e.addr << std::endl;
			break;
	}
}

void process_single_packet_header(SDHRPacketHeader* h,
								  uint32_t packet_size)
{
	uint32_t seqno = h->seqno;
	
	// std::cerr << "packet size is: " << packet_size << std::endl;
	// std::cerr << "Receiving seqno " << seqno << std::endl;
	
	if (seqno < prev_seqno)
		std::cerr << "FOUND EARLIER PACKET" << std::endl;
	if (seqno != prev_seqno + 1) {
		if (bFirstDrop) {
			bFirstDrop = false;
		}
		else {
			std::cerr << "seqno drops: " << seqno - prev_seqno + 1 << std::endl;
			// this is pretty bad, should probably go into error
		}
	}
	prev_seqno = seqno;
	switch (h->cmdtype)
	{
		case 0:    // bus event
			break;
		case 1:    // echo
				   // TODO
			return;
		case 2: // computer reset
			A2VideoManager::GetInstance()->bShouldReboot = true;
			return;
		case 3: // datetime request
				// TODO
			return;
		default:
			std::cerr << "ignoring cmd" << std::endl;
			return;
	};
	
	// bus event
	uint8_t* p = (uint8_t*)h + sizeof(SDHRPacketHeader);
	uint8_t* e = (uint8_t*)h + packet_size;
	
	while (p < e) {
		uint32_t* event = (uint32_t*)p;
		p += 4;
		uint8_t ctrl_bits = (*event >> 24) & 0xff;
		uint16_t addr = (*event >> 8) & 0xffff;
		uint8_t data = *event & 0xff;
		bool m2sel = (ctrl_bits & 0x02) == 0x02;
		bool m2b0 = (ctrl_bits & 0x04) == 0x04;
		bool iigs_mode = (ctrl_bits & 0x80) == 0x80;
		bool rw = (ctrl_bits & 0x01) == 0x01;
		
		SDHREvent ev(iigs_mode, m2b0, m2sel, rw, addr, data);
		process_single_event(ev);
	}
}

// Platform-independent event processing
// Filters and assigns events to memory, control or data
// Events assigned to data have their data bytes appended to a command_buffer
// The command_buffer is then further processed by SDHRManager
int process_events_thread(bool* shouldTerminateProcessing)
{
	std::cout << "Starting Processing Thread\n";
	std::chrono::nanoseconds totalDuration(0);
	auto start = std::chrono::high_resolution_clock::now();

	while (!(*shouldTerminateProcessing)) {
		// Get a packet from the queue
		// Each packet should have a minimum of 64 events
		auto p = packetInQueue.pop();
		start = std::chrono::high_resolution_clock::now();
		if (p->size > 0)
		{
			SDHRPacketHeader* h = (SDHRPacketHeader*)p->data;
			process_single_packet_header(h, p->size);
			// std::cerr << "Processing: " << h->seqno << std::endl;
		}
		else {
			std::cerr << "Empty packet!\n";
		}
		packetFreeQueue.push(std::move(p));
		totalDuration += std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - start);
		++num_processed_packets;
		if (num_processed_packets % 100000 == 0)
		{
			// std::cerr << "Max sizes: " << packetInQueue.max_size() << " > " << packetFreeQueue.max_size() << std::endl;
			duration_packet_processing_ns = totalDuration.count() / 100000;
			totalDuration = std::chrono::nanoseconds::zero();
		}
	}
	std::cout << "Stopped Processing Thread\n";
	return 0;
}

int socket_server_thread(uint16_t port, bool* shouldTerminateNetworking)
{
	std::cout << "Starting Network Thread\n";
	std::chrono::nanoseconds totalDuration(0);
	auto start = std::chrono::high_resolution_clock::now();

	eventRecorder = EventRecorder::GetInstance();

	clear_queues();

	prev_seqno = 0;
	bFirstDrop = false;

#ifdef __NETWORKING_WINDOWS__
	WSADATA wsaData;
	int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (result != 0) {
		std::cerr << "WSAStartup failed: " << result << std::endl;
		return 1;
	}
#endif
	
	auto sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0) {
		std::cerr << "Error opening socket" << std::endl;
#ifdef __NETWORKING_WINDOWS__
		WSACleanup();
#endif
		return 1;
	}
	
	struct sockaddr_in serveraddr;
	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons((unsigned short)port);
	serveraddr.sin_addr.s_addr = INADDR_ANY;
	
	if (bind(sockfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0) {
		std::cerr << "Error on binding" << std::endl;
#ifdef __NETWORKING_WINDOWS__
		closesocket(sockfd);
		WSACleanup();
#else
		close(sockfd);
#endif
		return 1;
	}
	
	// Polling structure
#ifdef __NETWORKING_WINDOWS__
	WSAPOLLFD fds[1];
#else
	struct pollfd fds[1];
#endif
	fds[0].fd = sockfd;
	fds[0].events = POLLIN; // Check for data to read
	
	std::cout << "Waiting for packets..." << std::endl;
	bIsConnected = false;
	
	int64_t last_recv_nsec = 0;
	int64_t num_packets_received = 0;

	while (!(*shouldTerminateNetworking)) {
#ifdef __NETWORKING_WINDOWS__
		LARGE_INTEGER frequency;        // ticks per second
		LARGE_INTEGER t1;               // ticks
		QueryPerformanceFrequency(&frequency);
		QueryPerformanceCounter(&t1);
		int64_t nsec = t1.QuadPart * 100'000'000'0ll / frequency.QuadPart;
#else
		timespec ts;
		clock_gettime(CLOCK_MONOTONIC, &ts);
		int64_t nsec = ts.tv_sec * 100'000'000'0ll + ts.tv_nsec;
#endif
		int retval = 0;
		
		// Poll and timeout every second to allow for thread termination
		// Timeout is 1000 ms
#ifdef __NETWORKING_WINDOWS__
		retval = WSAPoll(fds, 1, 1000);
#else
		retval = poll(fds, 1, 1000);
#endif
		
		if (retval < 0 && errno != EWOULDBLOCK) {
			std::cerr << "Error in recvmmsg" << std::endl;
			return 1;
		}
		if (bIsConnected && nsec > last_recv_nsec + 1'000'000'000'0ll) {
			bIsConnected = false;
			prev_seqno = 0;
			bFirstDrop = true;
			A2VideoManager::GetInstance()->DeactivateBeam();
			std::cout << "Client disconnected" << std::endl;
			continue;
		}
		if (retval < 1) {	// no data
			continue;
		}

		// From now on there's data!

		if (!bIsConnected) {
			bIsConnected = true;
			A2VideoManager::GetInstance()->ActivateBeam();
			std::cout << "Client connected" << std::endl;
		}
		last_recv_nsec = nsec;
		
		if (fds[0].revents & POLLIN) {
			start = std::chrono::high_resolution_clock::now();
			auto packet = packetFreeQueue.pop();	// there should _always_ be a free packet available
			sockaddr_in src_addr;
			socklen_t addrlen = sizeof(src_addr);
			
			packet->size = (uint32_t)recvfrom(sockfd, reinterpret_cast<char*>(packet->data), PKT_BUFSZ, 0, (struct sockaddr *)&src_addr, &addrlen);
			++num_packets_received;
			if (!eventRecorder->IsInReplayMode())
				packetInQueue.push(std::move(packet));
			else
				packetFreeQueue.push(std::move(packet));
			totalDuration += std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - start);
			if (num_packets_received % 100000 == 0)
			{
				duration_network_processing_ns = totalDuration.count() / 100000;
				totalDuration = std::chrono::nanoseconds::zero();
			}
		}
	}
	
#ifdef __NETWORKING_WINDOWS__
	closesocket(sockfd);
	WSACleanup();
#else
	close(sockfd);
#endif
	
	return 0;
}
