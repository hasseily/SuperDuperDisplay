#include "SDHRNetworking.h"
#include "A2VideoManager.h"
#include "SDHRManager.h"
#include <time.h>
#include <fcntl.h>

static ConcurrentQueue<SDHREvent> events;

ENET_RES socket_bind_and_listen(__SOCKET* server_fd, const sockaddr_in& server_addr)
{
	events.clear();

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
	if (bind(*server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
		std::cerr << "Error binding socket" << std::endl;
		return ENET_RES::ERR;
	}
#endif

	return ENET_RES::OK;
}

void terminate_processing_thread()
{
	// Force a dummy event to process, so that shouldTerminateProcessing is triggered
	// and the loop is closed cleanly.
	SDHREvent e = SDHREvent(1, 0, 0);
	events.push(e);
}

// Platform-independent event processing
// Filters and assigns events to memory, control or data
// Events assigned to data have their data bytes appended to a command_buffer
// The command_buffer is then further processed by SDHRManager
int process_events_thread(bool* shouldTerminateProcessing)
{
	std::cout << "Starting Processing Thread\n";
	SDHRManager* sdhrMgr = SDHRManager::GetInstance();
	A2VideoManager* a2VideoMgr = A2VideoManager::GetInstance();
	uint8_t* a2mem = sdhrMgr->GetApple2MemPtr();
	while (!(*shouldTerminateProcessing)) {
		auto event_list = events.drain();
		for (auto& e : event_list) {
			if (e.rw == 1)	// read or dummy event, disregard
				continue;
			// std::cout << e.rw << " " << std::hex << e.addr << " " << (uint32_t)e.data << std::endl;
			if ((e.addr >= 0x200) && (e.addr < 0xc000)) {
				a2mem[e.addr] = e.data;
				a2VideoMgr->NotifyA2MemoryDidChange(e.addr);
				if ((e.addr >= 0x400) && (e.addr < 0x800))
				continue;
			}	
			if ((e.addr != CXSDHR_CTRL) && (e.addr != CXSDHR_DATA)) {
				// ignore non-control
				continue;
			}
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
					//#ifdef DEBUG
					std::cout << "CONTROL: Disable SDHR" << std::endl;
					//#endif
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
	}
	std::cout << "Stopped Processing Thread\n";
	return 0;
}

int socket_server_thread(uint16_t port, bool* shouldTerminateNetworking)
{
	// commands socket and descriptors
	//__SOCKET server_fd, client_fd;
	//struct sockaddr_in server_addr, client_addr;
	//socklen_t client_len = sizeof(client_addr);
	std::cout << "Starting Network Thread\n";

#ifdef __NETWORKING_WINDOWS__

#define VLEN 16
#define BUFSZ 2048

	__SOCKET sockfd;
	struct sockaddr_in serveraddr;
	ZeroMemory(&serveraddr, sizeof(serveraddr));

	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons((unsigned short)port);
	serveraddr.sin_addr.s_addr = INADDR_ANY;

	uint8_t RecvBuf[BUFSZ];
	int BufLen = BUFSZ;

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
	uint16_t prev_addr = 0;
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
				std::cerr << "Error in recvmmsg" << std::endl;
				return 1;
			}
			if (connected && nsec > last_recv_nsec + 10000000000ll) {
				std::cout << "Client disconnected" << std::endl;
				connected = false;
				first_drop = true;
				continue;
			}
			if (retval == -1) {
				continue;
			}
			if (!connected) {
				connected = true;
				std::cout << "Client connected" << std::endl;
			}
			last_recv_nsec = nsec;

			SDHRPacketHeader* h = (SDHRPacketHeader*)RecvBuf;
			uint32_t seqno = h->seqno[0];
			seqno += (uint32_t)h->seqno[1] << 8;
			seqno += (uint32_t)h->seqno[2] << 16;
			seqno += (uint32_t)h->seqno[3] << 24;

			if (seqno < prev_seqno)
				std::cerr << "FOUND EARLIER PACKET" << std::endl;
			if (seqno != prev_seqno + 1) {
				if (first_drop) {
					first_drop = false;
				}
				else {
					std::cerr << "seqno drops: "
						<< seqno - prev_seqno + 1 << std::endl;
					// this is pretty bad, should probably go into error
				}
			}
			prev_seqno = seqno;
			if (h->cmdtype != 0) {
				std::cerr << "ignoring cmd" << std::endl;
				// currently ignoring anything not a bus event
				continue;
			}
			uint8_t* p = RecvBuf + sizeof(SDHRPacketHeader);

			while (p - RecvBuf < retval) {
				SDHRBusChunk* c = (SDHRBusChunk*)p;
				size_t chunk_len = 10;
				uint32_t addr_count = 0;
				for (int j = 0; j < 8; ++j) {
					bool rw = (c->rwflags & (1 << j)) != 0;
					uint16_t addr;
					bool addr_flag = (c->seqflags & (1 << j)) != 0;
					if (addr_flag) {
						chunk_len += 2;
						addr = c->addrs[addr_count * 2 + 1];
						addr <<= 8;
						addr += c->addrs[addr_count * 2];
						++addr_count;
					}
					else {
						addr = ++prev_addr;
					}
					prev_addr = addr;
					if (rw) {
						// ignoring all read events
						continue;
					}
					SDHREvent e(rw, addr, c->data[j]);
					events.push(e);
				}
				p += chunk_len;
			}
		}
	}

	std::cout << "Client Closing" << std::endl;
	closesocket(sockfd);
	std::cout << "    Client Closed" << std::endl;
	return 0;
#else
#define VLEN 1024
#define BUFSZ 2048

	__SOCKET sockfd;
	struct sockaddr_in serveraddr;
	bzero((char*)&serveraddr, sizeof(serveraddr));

	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons((unsigned short)port);
	serveraddr.sin_addr.s_addr = INADDR_ANY;

	if (socket_bind_and_listen(&sockfd, serveraddr) == ENET_RES::ERR)
		return 1;

	struct mmsghdr msgs[VLEN];
	struct iovec iovecs[VLEN];
	uint8_t bufs[VLEN][BUFSZ];
	for (int i = 0; i < VLEN; ++i) {
		iovecs[i].iov_base = bufs[i];
		iovecs[i].iov_len = BUFSZ;
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
	uint16_t prev_addr = 0;
	int64_t last_recv_nsec;

	while (!(*shouldTerminateNetworking)) {
		struct timespec ts;
		clock_gettime(CLOCK_REALTIME, &ts);
		int64_t nsec = ts.tv_sec * 1000000000ll + ts.tv_nsec;
		struct timespec wait_ts;
		wait_ts.tv_sec = 1;
		wait_ts.tv_nsec = 0;
		int retval = recvmmsg(sockfd, msgs, VLEN, 0, &wait_ts);
		if (retval < 0 && errno != EWOULDBLOCK) {
			std::cerr << "Error in recvmmsg" << std::endl;
			return 1;
		}
		if (connected && nsec > last_recv_nsec + 10000000000ll) {
			std::cout << "Client disconnected" << std::endl;
			connected = false;
			first_drop = true;
			continue;
		}
		if (retval == -1) {
			continue;
		}
		if (!connected) {
			connected = true;
			std::cout << "Client connected" << std::endl;
		}
		last_recv_nsec = nsec;

		for (int i = 0; i < retval; ++i) {
			SDHRPacketHeader* h = (SDHRPacketHeader*)bufs[i];
			uint32_t seqno = h->seqno[0];
			seqno += (uint32_t)h->seqno[1] << 8;
			seqno += (uint32_t)h->seqno[2] << 16;
			seqno += (uint32_t)h->seqno[3] << 24;

			if (seqno < prev_seqno)
				std::cerr << "FOUND EARLIER PACKET" << std::endl;

			if (seqno != prev_seqno + 1) {
				if (first_drop) {
					first_drop = false;
				}
				else {
					std::cerr << "seqno drops: "
						<< seqno - prev_seqno + 1 << std::endl;
					// this is pretty bad, should probably go into error
				}
			}
			prev_seqno = seqno;
			if (h->cmdtype != 0) {
				std::cerr << "ignoring cmd" << std::endl;
				// currently ignoring anything not a bus event
				continue;
			}
			uint8_t* p = bufs[i] + sizeof(SDHRPacketHeader);
			while (p - bufs[i] < msgs[i].msg_len) {
				SDHRBusChunk* c = (SDHRBusChunk*)p;
				size_t chunk_len = 10;
				uint32_t addr_count = 0;
				for (int j = 0; j < 8; ++j) {
					bool rw = (c->rwflags & (1 << j)) != 0;
					uint16_t addr;
					bool addr_flag = (c->seqflags & (1 << j)) != 0;
					if (addr_flag) {
						chunk_len += 2;
						addr = c->addrs[addr_count * 2 + 1];
						addr <<= 8;
						addr += c->addrs[addr_count * 2];
						++addr_count;
					}
					else {
						addr = ++prev_addr;
					}
					prev_addr = addr;
					if (rw) {
						// ignoring all read events
						continue;
					}
					SDHREvent e(rw, addr, c->data[j]);
					events.push(e);
				}
				p += chunk_len;
			}
		}
	}

	std::cout << "Client Closing" << std::endl;
	close(sockfd);
	std::cout << "    Client Closed" << std::endl;
	return 0;
#endif // __NETWORKING_WINDOWS__
}
