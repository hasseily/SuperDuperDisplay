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
#include "ftd3xx.h"

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

int process_usb_events_thread(bool* shouldTerminateProcessing) {
	std::cout << "starting usb processing thread" << std::endl;
	bIsConnected = true;
	while (!(*shouldTerminateProcessing)) {
		auto packet = packetInQueue.pop();
		uint32_t* p = (uint32_t*)packet->data;
		while ((uint8_t*)p < packet->data + packet->size) {
			uint32_t hdr = *p;
			uint32_t packet_len = hdr & 0x0000ffff;
			uint32_t packet_version = (hdr & 0xff000000) >> 24;
			uint32_t packet_type = (hdr & 0x00ff0000) >> 16;
			if (packet_len == 0 || packet_len > 1024 || ((packet_len % 4) != 0)) {
				printf("invalid packet len: %u\n", packet_len);
				// this packet is garbage somehow, stop processing
				break;
			}
			if ((uint8_t*)p + packet_len > packet->data + packet->size) {
				// shouldn't happen, but also garbage condition
				printf("packet len exceeds buffer\n");
				break;
			}
			if (packet_version != 1) {
				// shouldn't happen
				printf("packet error, version must be 1\n");
				p += (packet_len / 4);
				continue;
			}
			if (packet_type != 1) {
				// when we start doing different messages we'd handle it here
				p += (packet_len / 4);
				continue;
			}
			else {
				++p;
				for (uint32_t i = 1; i < packet_len / 4; ++i) {
					uint32_t event = *p++;
					uint16_t addr = (event >> 16) & 0xffff;
					uint8_t misc = (event) & 0x0f;
					uint8_t data = (event >> 4) & 0xff;
					bool rw = ((misc & 0x01) == 0x01);
					static bool event_reset = ((misc & 0x02) == 0x02);
                    static bool event_reset_prev = 1;
					if ((event_reset == 0) && (event_reset_prev == 1)) {
						A2VideoManager::GetInstance()->bShouldReboot = true;
					}
                    event_reset_prev = event_reset;
					SDHREvent ev(0, 0, 0, rw, addr, data);
					process_single_event(ev);
				}
			}
		}
		packetFreeQueue.push(std::move(packet));
	}
	return 0;
}

int usb_server_thread(uint16_t port, bool* shouldTerminateNetworking) {
	eventRecorder = EventRecorder::GetInstance();
	clear_queues();
	std::cout << "Starting USB thread" << std::endl;
	FT_HANDLE handle = NULL;
	bool connected = false;
	std::chrono::steady_clock::time_point next_connect_timeout{};
	while (!(*shouldTerminateNetworking)) {
		if (!connected) {
			if (next_connect_timeout > std::chrono::steady_clock::now()) {
				continue;
			}
			next_connect_timeout = std::chrono::steady_clock::now() + std::chrono::milliseconds(1000);
			uint32_t count;
			FT_DEVICE_LIST_INFO_NODE nodes[16];
			if (FT_OK != FT_CreateDeviceInfoList(&count)) {
				std::cerr << "Failed to find FPGA usb device" << std::endl;
				continue;
			}
			if (!count || (FT_OK != FT_GetDeviceInfoList(nodes, &count))) {
				std::cerr << "Failed to iterate FPGA usb devices" << std::endl;
				continue;
			}
			FT_TRANSFER_CONF conf;
			memset(&conf, 0, sizeof(conf));
			conf.wStructSize = sizeof(conf);
			conf.pipe[FT_PIPE_DIR_IN].fNonThreadSafeTransfer = true;
			conf.pipe[FT_PIPE_DIR_OUT].fNonThreadSafeTransfer = true;
			for (uint32_t i = 0; i < 4; ++i) {
				FT_SetTransferParams(&conf, i);
			}
			FT_Create(0, FT_OPEN_BY_INDEX, &handle);
			if (!handle) {
				std::cerr << "Failed to open FPGA usb device handle" << std::endl;
				continue;
			}
			std::cerr << "connected to FPGA usb device" << std::endl;
			connected = true;
		}
		auto packet = packetFreeQueue.pop();
		if (FT_OK != FT_ReadPipeEx(handle, 0, packet->data, PKT_BUFSZ, &(packet->size), 1000)) {
			std::cerr << "Failed to read from FPGA usb packet pipe" << std::endl;
			FT_Close(handle);
			handle = NULL;
			connected = false;
			continue;
		}
		if (!eventRecorder->IsInReplayMode()) {
			packetInQueue.push(std::move(packet));
		}
		else {
			packetFreeQueue.push(std::move(packet));
		}
	}
	std::cout << "ending usb read loop" << std::endl;
	return 0;
}
