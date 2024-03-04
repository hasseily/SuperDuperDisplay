#include "SDHRNetworking.h"
#include "A2VideoManager.h"
#include "SDHRManager.h"
#include "CycleCounter.h"
#include "EventRecorder.h"
#include <time.h>
#include <fcntl.h>


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

static ConcurrentQueue<std::shared_ptr<Packet>> packetQueue;
static EventRecorder* eventRecorder;
static uint32_t prev_seqno;
static bool bFirstDrop;

ENET_RES socket_bind_and_listen(__SOCKET* server_fd, const sockaddr_in& server_addr)
{
	packetQueue.clear();

#ifdef __NETWORKING_WINDOWS__
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		std::cerr << "WSAStartup failed" << std::endl;
		return ENET_RES::ERR;
	}
	if ((*server_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == INVALID_SOCKET) {
		std::cerr << "Error creating socket" << std::endl;
		WSACleanup();
		return ENET_RES::ERR;
	}
	BOOL optval = true;
	setsockopt(*server_fd, SOL_SOCKET, SO_REUSEADDR,
		(const char*)&optval, sizeof(BOOL));
	int rcvbuf = 1024 * 1024;
	int result = setsockopt(*server_fd, SOL_SOCKET, SO_RCVBUF, (const char*)&rcvbuf, sizeof(int));
	if (result == SOCKET_ERROR) {
		printf("setsockopt for SO_RCVBUF failed with error: %u\n", WSAGetLastError());
	}

	if (bind(*server_fd, (SOCKADDR*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
		std::cerr << "Error binding socket" << std::endl;
		closesocket(*server_fd);
		WSACleanup();
		return ENET_RES::ERR;
	}
#else // not __NETWORKING_WINDOWS__
	if ((*server_fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		std::cerr << "Error creating socket" << std::endl;
		return ENET_RES::ERR;
	}
	int optval = 1;
	setsockopt(*server_fd, SOL_SOCKET, SO_REUSEADDR,
		(const char*)&optval, sizeof(int));
	if (::bind(*server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
		std::cerr << "Error binding socket" << std::endl;
		return ENET_RES::ERR;
	}
#endif

	return ENET_RES::OK;
}

void insert_event(SDHREvent* e)
{
	assert("ERROR: CANNOT INSERT EVENT");
}

void clear_queue()
{
	packetQueue.clear();
}

void terminate_processing_thread()
{
	// Force a dummy packet to process, so that shouldTerminateProcessing is triggered
	// and the loop is closed cleanly.
	auto packet = std::make_shared<Packet>();
	packetQueue.push(std::move(packet));
}

void process_single_event(SDHREvent& e)
{
	// std::cout << e.is_iigs << " " << e.rw << " " << std::hex << e.addr << " " << (uint32_t)e.data << std::endl;
	
	eventRecorder = EventRecorder::GetInstance();
	if (eventRecorder->IsRecording())
		eventRecorder->RecordEvent(&e);
	// Update the cycle counting and VBL hit
	bool isVBL = ((e.addr == 0xC019) && e.rw && ((e.data >> 7) == 0));
	CycleCounter::GetInstance()->IncrementCycles(1, isVBL);
	if (e.is_iigs && e.m2b0) {
		// ignore updates from iigs_mode firmware with m2sel high
		return;
	}
	if (e.rw && ((e.addr & 0xF000) != 0xC000)) {
		// ignoring all read events not softswitches
		return;
	}

	auto sdhrMgr = SDHRManager::GetInstance();
	auto a2VideoMgr = A2VideoManager::GetInstance();
	
	/*
	 *********************************
	 HANDLE SIMPLE MEMORY WRITE EVENTS
	 *********************************
	 */
	if ((e.addr >= _A2_MEMORY_SHADOW_BEGIN) && (e.addr < _A2_MEMORY_SHADOW_END)) {
		uint8_t _sw = 0;	// switches state
		if (a2VideoMgr->IsSoftSwitch(A2SS_80STORE))
			_sw |= 0b001;
		if (a2VideoMgr->IsSoftSwitch(A2SS_RAMWRT))
			_sw |= 0b010;
		if (a2VideoMgr->IsSoftSwitch(A2SS_PAGE2))
			_sw |= 0b100;
		bool bIsAux = false;
		switch (_sw)
		{
			case 0b010:
				// Only writes 0000-01FF to MAIN
				bIsAux = true;
				break;
			case 0b011:
				// anything not page 1 (including 0000-01FFF goes to AUX
				if ((e.addr >= 0x400 && e.addr < 0x800)
					|| (e.addr >= 0x2000 && e.addr < 0x4000))
					bIsAux = false;
				else
					bIsAux = true;
				break;
			case 0b101:
				// Page 1 is in AUX
				if ((e.addr >= 0x400 && e.addr < 0x800)
					|| (e.addr >= 0x2000 && e.addr < 0x4000))
					bIsAux = true;
				break;
			case 0b110:
				// All writes to AUX except for 0000-01FF
				bIsAux = true;
				break;
			case 0b111:
				// All writes to AUX except for 0000-01FF
				bIsAux = true;
				break;
			default:
				break;
		}
		if (e.is_iigs && e.m2b0)
			bIsAux = true;
		
		if (bIsAux)
		{
			sdhrMgr->GetApple2MemAuxPtr()[e.addr] = e.data;
			a2VideoMgr->NotifyA2MemoryDidChange(e.addr + 0x10000);
		}
		else {
			sdhrMgr->GetApple2MemPtr()[e.addr] = e.data;
			a2VideoMgr->NotifyA2MemoryDidChange(e.addr);
		}
		return;
	}
	/*
	 *********************************
	 HANDLE SOFT SWITCHES EVENTS
	 *********************************
	 */
	if ((e.addr != CXSDHR_CTRL) && (e.addr != CXSDHR_DATA)) {
		// Send soft switches to the A2VideoManager
		if (e.addr >> 8 == 0xc0)
			a2VideoMgr->ProcessSoftSwitch(e.addr, e.data, e.rw, e.is_iigs);
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
		// bool m2sel = (ctrl_bits & 0x02) == 0x02;
		bool m2b0 = (ctrl_bits & 0x04) == 0x04;
		bool iigs_mode = (ctrl_bits & 0x80) == 0x80;
		bool rw = (ctrl_bits & 0x01) == 0x01;
		
		SDHREvent ev(iigs_mode, m2b0, rw, addr, data);
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
	while (!(*shouldTerminateProcessing)) {
		// Get a packet from the queue
		// Each packet should have a minimum of 64 events
		auto p = packetQueue.pop();
		
		if (p->size > 0)
		{
			SDHRPacketHeader* h = (SDHRPacketHeader*)p->data;
			process_single_packet_header(h, p->size);
			// std::cerr << "Processing: " << h->seqno << std::endl;
		}
		else {
			std::cerr << "Empty packet!\n";
		}
	}
	std::cout << "Stopped Processing Thread\n";
	return 0;
}

int socket_server_thread(uint16_t port, bool* shouldTerminateNetworking)
{
	std::cout << "Starting Network Thread\n";
	eventRecorder = EventRecorder::GetInstance();

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
	
	int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
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
	bool connected = false;
	
	int64_t last_recv_nsec = 0;
	
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
		if (connected && nsec > last_recv_nsec + 1'000'000'000'0ll) {
			connected = false;
			prev_seqno = 0;
			bFirstDrop = true;
			A2VideoManager::GetInstance()->DeactivateBeam();
			std::cout << "Client disconnected" << std::endl;
			continue;
		}
		if (retval == -1) {
			continue;
		}
		
		if (!connected) {
			connected = true;
			A2VideoManager::GetInstance()->ActivateBeam();
			std::cout << "Client connected" << std::endl;
		}
		last_recv_nsec = nsec;
		
		if (fds[0].revents & POLLIN) {
			auto packet = std::make_shared<Packet>();
			sockaddr_in src_addr;
			socklen_t addrlen = sizeof(src_addr);
			
			packet->size = recvfrom(sockfd, reinterpret_cast<char*>(packet->data), PKT_BUFSZ, 0, (struct sockaddr *)&src_addr, &addrlen);
			if (packet->size > 0) {
				if (!eventRecorder->IsInReplayMode())
					packetQueue.push(std::move(packet));
				// std::cout << "Received packet size: " << packet->size << std::endl;
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

	
	
	/*
	
ORIGINAL CODE
	
	
	
	
#ifdef __NETWORKING_WINDOWS__

	__SOCKET sockfd;
	struct sockaddr_in serveraddr;
	ZeroMemory(&serveraddr, sizeof(serveraddr));

	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons((unsigned short)port);
	serveraddr.sin_addr.s_addr = INADDR_ANY;

	uint8_t RecvBuf[PKT_BUFSZ];
	int BufLen = PKT_BUFSZ;

	if (socket_bind_and_listen(&sockfd, serveraddr) == ENET_RES::ERR)
		return 1;

	WSAPOLLFD fdArray[1];  // 1 connection
	// Set up the fd_set for WSAPoll()
	fdArray[0].fd = sockfd;
	fdArray[0].events = POLLRDNORM;

	DWORD flags = 0;

	u_long mode = 1;  // 1 to enable non-blocking socket
	ioctlsocket(sockfd, FIONBIO, &mode);

	std::cout << "Waiting for connection..." << std::endl;
	bool connected = false;

	bool first_drop = true;
	uint32_t prev_seqno = 0;
	int64_t last_recv_nsec;

	while (!(*shouldTerminateNetworking)) {
		LARGE_INTEGER frequency;        // ticks per second
		LARGE_INTEGER t1;               // ticks
		QueryPerformanceFrequency(&frequency);
		QueryPerformanceCounter(&t1);
		int64_t nsec = t1.QuadPart * 1000000000ll / frequency.QuadPart;
		int retval = 0;

		// Use WSAPoll() to wait for an incoming UDP packet
		WSAPoll(fdArray, 1, 1000);  // Poll and timeout every second to allow for thread termination

		if (fdArray[0].revents & POLLRDNORM)  // if any event occurred
		{
			sockaddr_in SenderAddr;
			int SenderAddrSize = sizeof(SenderAddr);

			// Receive a datagram
			retval = recvfrom(sockfd, (char*)RecvBuf, BufLen, 0, (SOCKADDR*)&SenderAddr, &SenderAddrSize);
			if (retval < 0 && errno != EWOULDBLOCK) {
				std::cerr << "Error in recvfrom" << std::endl;
				return 1;
			}
			if (connected && nsec > last_recv_nsec + 10000000000ll) {
				connected = false;
				first_drop = true;
				A2VideoManager::GetInstance()->DeactivateBeam();
				std::cout << "Client disconnected" << std::endl;
				continue;
			}
			if (retval == -1) {
				continue;
			}
			if (!connected) {
				connected = true;
				A2VideoManager::GetInstance()->ActivateBeam();
				std::cout << "Client connected" << std::endl;
			}
			last_recv_nsec = nsec;

			SDHRPacketHeader* h = (SDHRPacketHeader*)RecvBuf;
			process_single_packet_header(h, retval, prev_seqno, first_drop);
		}
	}

	std::cout << "Client Closing" << std::endl;
	closesocket(sockfd);
	std::cout << "    Client Closed" << std::endl;
	return 0;
#endif
#ifdef __NETWORKING_APPLE__

    int sockfd;
    struct sockaddr_in serveraddr;
    memset(&serveraddr, 0, sizeof(serveraddr));

    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(port);
    serveraddr.sin_addr.s_addr = INADDR_ANY;

    if (socket_bind_and_listen(&sockfd, serveraddr) == ENET_RES::ERR)
        return 1;

    struct msghdr msg;
    struct iovec iovec;
    uint8_t* buf;

    buf = new uint8_t[PKT_BUFSZ];
    iovec.iov_base = buf;
    iovec.iov_len = PKT_BUFSZ;
    msg.msg_iov = &iovec;
    msg.msg_iovlen = 1;
    
    int flags = fcntl(sockfd, F_GETFL, 0);
    flags |= O_NONBLOCK;
    fcntl(sockfd, F_SETFL, flags);

    std::cout << "Waiting for connection..." << std::endl;
    bool connected = false;

    bool first_drop = true;
    uint32_t prev_seqno = 0;
    int64_t last_recv_nsec = 0;

    timespec ts;
    while (!(*shouldTerminateNetworking)) {
        clock_gettime(CLOCK_MONOTONIC, &ts);
        int64_t nsec = ts.tv_sec * 1000000000ll + ts.tv_nsec;

        int retval = (int)recvmsg(sockfd, &msg, 0);
        if (retval < 0 && errno != EWOULDBLOCK) {
            std::cerr << "Error in recvmsg" << std::endl;
            return 1;
        }
        if (connected && nsec > last_recv_nsec + 10000000000ll) {
            connected = false;
            first_drop = true;
			A2VideoManager::GetInstance()->DeactivateBeam();
			std::cout << "Client disconnected" << std::endl;
            continue;
        }
        if (retval == -1) {
            continue;
        }

        if (!connected) {
            connected = true;
			A2VideoManager::GetInstance()->ActivateBeam();
            std::cout << "Client connected" << std::endl;
        }
        last_recv_nsec = nsec;

        SDHRPacketHeader* h = (SDHRPacketHeader*)buf;
        process_single_packet_header(h, retval, prev_seqno, first_drop);
    }

    std::cout << "Client Closing" << std::endl;
    close(sockfd);
    std::cout << "    Client Closed" << std::endl;

    delete[] buf;

    return 0;
#endif
#ifdef __NETWORKING_LINUX__
#define VLEN 256

	__SOCKET sockfd;
	struct sockaddr_in serveraddr;
	bzero((char*)&serveraddr, sizeof(serveraddr));

	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons((unsigned short)port);
	serveraddr.sin_addr.s_addr = INADDR_ANY;

	if (socket_bind_and_listen(&sockfd, serveraddr) == ENET_RES::ERR)
		return 1;

    struct mmsghdr* msgs = new struct mmsghdr[VLEN];
    struct iovec* iovecs = new struct iovec[VLEN];
    uint8_t** bufs = new uint8_t*[VLEN];

	for (int i = 0; i < VLEN; ++i) {
        bufs[i] = new uint8_t[PKT_BUFSZ];
		iovecs[i].iov_base = bufs[i];
		iovecs[i].iov_len = PKT_BUFSZ;
		msgs[i].msg_hdr.msg_iov = &iovecs[i];
		msgs[i].msg_hdr.msg_iovlen = 1;
	}

	int flags = fcntl(sockfd, F_GETFL, 0);
	flags |= O_NONBLOCK;
	fcntl(sockfd, F_SETFL, flags);

	std::cout << "Waiting for connection..." << std::endl;
	bool connected = false;

	bool first_drop = true;
	uint32_t prev_seqno = 0;
	int64_t last_recv_nsec;

	while (!(*shouldTerminateNetworking)) {
		struct timespec ts;
		clock_gettime(CLOCK_REALTIME, &ts);
		int64_t nsec = ts.tv_sec * 1000000000ll + ts.tv_nsec;

		int retval = recvmmsg(sockfd, msgs, VLEN, 0, NULL);
		if (retval < 0 && errno != EWOULDBLOCK) {
			std::cerr << "Error in recvmmsg" << std::endl;
			return 1;
		}
		if (connected && nsec > last_recv_nsec + 10000000000ll) {
			connected = false;
			first_drop = true;
			A2VideoManager::GetInstance()->DeactivateBeam();
			std::cout << "Client disconnected" << std::endl;
			continue;
		}
		if (retval == -1) {
			continue;
		}

        if (!connected) {
			connected = true;
			A2VideoManager::GetInstance()->ActivateBeam();
			std::cout << "Client connected" << std::endl;
		}
		last_recv_nsec = nsec;

		for (int i = 0; i < retval; ++i) {
			SDHRPacketHeader* h = (SDHRPacketHeader*)bufs[i];
            process_single_packet_header(h, msgs[i].msg_len, prev_seqno, first_drop);
		}
	}

	std::cout << "Client Closing" << std::endl;
	close(sockfd);
	std::cout << "    Client Closed" << std::endl;

    for (int i = 0; i < VLEN; ++i) {
        delete[] bufs[i];
    }
    delete[] bufs;
    delete[] iovecs;
    delete[] msgs;

	return 0;
#endif // __NETWORKING_WINDOWS__
*/
