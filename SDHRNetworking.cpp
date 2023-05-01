#include "SDHRNetworking.h"
#include "SDHRManager.h"

ENET_RES socket_bind_and_listen(__SOCKET* server_fd, const sockaddr_in& server_addr)
{
#ifdef __NETWORKING_WINDOWS__
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		std::cerr << "WSAStartup failed" << std::endl;
		return ENET_RES::ERR;
	}
	if ((*server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET) {
		std::cerr << "Error creating socket" << std::endl;
		WSACleanup();
		return ENET_RES::ERR;
	}
	if (bind(*server_fd, (SOCKADDR*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
		std::cerr << "Error binding socket" << std::endl;
		closesocket(*server_fd);
		WSACleanup();
		return ENET_RES::ERR;
	}
	if (listen(*server_fd, 1) == SOCKET_ERROR) {
		std::cerr << "Error listening on socket" << std::endl;
		closesocket(*server_fd);
		WSACleanup();
		return ENET_RES::ERR;
	}
#else // not __NETWORKING_WINDOWS__
	if ((*server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		std::cerr << "Error creating socket" << std::endl;
		return ENET_RES::ERR;
	}
	if (bind(*server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
		std::cerr << "Error binding socket" << std::endl;
		return ENET_RES::ERR;
	}
	if (listen(*server_fd, 1) == -1) {
		std::cerr << "Error listening on socket" << std::endl;
		return ENET_RES::ERR;
	}
#endif

	return ENET_RES::OK;
}

int socket_server_thread(uint16_t port)
{
	// commands socket and descriptors
	__SOCKET server_fd, client_fd;
	struct sockaddr_in server_addr, client_addr;
	socklen_t client_len = sizeof(client_addr);

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr.s_addr = INADDR_ANY;

	if (socket_bind_and_listen(&server_fd, server_addr) == ENET_RES::ERR)
		return 1;

	auto sdhrMgr = SDHRManager::GetInstance();
	uint8_t* a2mem = sdhrMgr->GetApple2MemPtr();

	while (true) {
		std::cout << "Waiting for connection..." << std::endl;

		// Here the thread will block waiting for a connection
		// In order to kill the thread when we quit the app, we need to connect
		// to it locally to unblock accept(), which then breaks on bShouldTerminateNetworking
#ifdef __NETWORKING_WINDOWS__
		if ((client_fd = accept(server_fd, (SOCKADDR*)&client_addr, &client_len)) == INVALID_SOCKET) {
			closesocket(server_fd);
			WSACleanup();
#else
		if ((client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len)) == -1) {
			close(client_fd);
#endif
			std::cerr << "Error accepting connection" << std::endl;
			return 1;
		}

		std::cout << "Client connected" << std::endl;

		if (bShouldTerminateNetworking)
			break;

		SDHRPacket packet;
		int bytes_received;

		while ((bytes_received = recv(client_fd, (char*)&packet, sizeof(packet), 0)) > 0) {
			if (bytes_received == sizeof(packet)) {
				/*
				std::cout << "Received packet:" << std::endl;
				std::cout << "  address: " << std::hex << packet.addr << std::endl;
				std::cout << "  data: " << std::hex << static_cast<unsigned>(packet.data) << std::endl;
				std::cout << "  pad: " << std::hex << static_cast<unsigned>(packet.pad) << std::endl;
				*/

				if ((packet.addr >= 0x200) && (packet.addr <= 0xbfff))
				{
					// it's a memory write
					a2mem[packet.addr] = packet.data;
					continue;
				}
				if ((packet.addr != CXSDHR_CTRL) && (packet.addr != CXSDHR_DATA))
				{
					// BAD PACKET TYPE
					std::cerr << "BAD PACKET! addr " << std::hex << packet.addr << ", data " << packet.data << std::endl;
					continue;
				}
				SDHRCtrl_e _ctrl;
				switch (packet.addr & 0x0f)
				{
				case 0x00:
					// std::cout << "This is a control packet!" << std::endl;
					_ctrl = (SDHRCtrl_e)packet.data;
					switch (_ctrl)
					{
					case SDHR_CTRL_DISABLE:
						std::cout << "CONTROL: Disable SDHR" << std::endl;
						sdhrMgr->ToggleSdhr(false);
						break;
					case SDHR_CTRL_ENABLE:
						std::cout << "CONTROL: Enable SDHR" << std::endl;
						sdhrMgr->ToggleSdhr(true);
						break;
					case SDHR_CTRL_RESET:
						std::cout << "CONTROL: Reset SDHR" << std::endl;
						sdhrMgr->ResetSdhr();
						break;
					case SDHR_CTRL_PROCESS:
					{
						/*
						At this point we have a complete set of commands to process.
						Some more data may be in the kernel socket receive buffer, but we don't care.
						They'll be processed in the next batch.
						Continue processing commands until the framebuffer is flipped. Once the framebuffer
						has flipped, run the framebuffer drawing with the current state and schedule a flip.
						Rince and repeat.
						*/
						// std::cout << "CONTROL: Process SDHR" << std::endl;
						bool processingSucceeded = sdhrMgr->ProcessCommands();
						// Whether or not the processing worked, clear the buffer. If the processing failed,
						// the data was corrupt and shouldn't be reprocessed
						sdhrMgr->ClearBuffer();
						if (processingSucceeded && sdhrMgr->IsSdhrEnabled())
						{
							// We have processed some commands.
							// TODO: ???
						}
						break;
					}
					default:
						break;
					}
					break;
				case 0x01:
					// std::cout << "This is a data packet" << std::endl;
					sdhrMgr->AddPacketDataToBuffer(packet.data);
					break;
				}
			}
			else {
				std::cerr << "Error receiving data or incomplete data" << std::endl;
			}
		}

#ifdef __NETWORKING_WINDOWS__
		if (bytes_received == SOCKET_ERROR) {
#else
		if (bytes_received == -1) {
#endif
			std::cerr << "Error receiving data" << std::endl;
		}

		std::cerr << "Client Closing" << std::endl;
#ifdef __NETWORKING_WINDOWS__
		closesocket(client_fd);
#else
		close(client_fd);
#endif
		std::cerr << "    Client Closed" << std::endl;
	}
#ifdef __NETWORKING_WINDOWS__
	closesocket(server_fd);
	WSACleanup();
#else
	close(server_fd);
#endif
	return 0;
}

bool socket_unblock_accept(uint16_t port)
{
#ifdef __NETWORKING_WINDOWS__
	WSADATA wsaData;
	int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (result != 0) {
		std::cerr << "WSAStartup failed: " << result << std::endl;
		return false;
	}
#endif

	struct sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

	auto client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#ifdef __NETWORKING_WINDOWS__
	if (client_socket == INVALID_SOCKET) {
		WSACleanup();
#else
	if (client_socket == -1) {
#endif
		std::cerr << "Error creating socket: " << WSAGetLastError() << std::endl;
		return false;
	}
#ifdef __NETWORKING_WINDOWS__
	if (connect(client_socket, (SOCKADDR*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
		closesocket(client_socket);
		WSACleanup();
#else
	if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
		close(client_socket);
#endif
		std::cerr << "Error connecting to server: " << WSAGetLastError() << std::endl;
		return false;
	}
	// Do nothing, we've already unblocked the server's accept()
	// so it will quit if bShouldTerminateNetworking is true
#ifdef __NETWORKING_WINDOWS__
	closesocket(client_socket);
	WSACleanup();
#else
	close(client_socket);
#endif
	WSACleanup();
	return true;
}