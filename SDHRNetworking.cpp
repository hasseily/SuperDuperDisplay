#include "ConcurrentQueue.h"
#include "SDHRNetworking.h"
#include "MemoryManager.h"
#include "A2VideoManager.h"
#include "SoundManager.h"
#include "MockingboardManager.h"
#include "SDHRManager.h"
#include "CycleCounter.h"
#include "EventRecorder.h"
#include "MainMenu.h"
#include <time.h>
#include <fcntl.h>
#include <chrono>
#ifdef __NETWORKING_WINDOWS__
#define FTD3XX_STATIC
#include "ftd3xx_win.h"
#else
#include "ftd3xx.h"
#endif
#include <charconv>

// Write to Appletini
#define FT_PIPE_WRITE_ID 0x02
// Read from Appletini
#define FT_PIPE_READ_ID 0x82

// The USB handle to the Appletini
static FT_HANDLE g_ftHandle = NULL;

static EventRecorder *eventRecorder;
static bool bIsConnected = false;
static uint64_t num_processed_packets = 0;
static uint64_t duration_packet_processing_ns = 0;
static uint64_t duration_network_processing_ns = 0;
static FT_STATUS ftStatus, ftStatusPrevious;// latest FT60x statuses
static FT_DEVICE_LIST_INFO_NODE activeNode; // currently active USB device

// Only do a single reset if a string of reset events arrive
static bool event_reset = 1;
static bool event_reset_prev = 1;

static ConcurrentQueue<std::shared_ptr<Packet>> packetInQueue;
static ConcurrentQueue<std::shared_ptr<Packet>> packetFreeQueue;

const uint64_t get_number_packets_processed() { return num_processed_packets; };
const uint64_t get_duration_packet_processing_ns() { return duration_packet_processing_ns; };
const uint64_t get_duration_network_processing_ns() { return duration_network_processing_ns; };
const size_t get_packet_pool_count() { return packetFreeQueue.max_size(); };
const size_t get_max_incoming_packets() { return packetInQueue.max_size(); };

std::vector<uint8_t> rx_message_buffer;

static bool bUSBImGUiWindowIsOpen = false;
static bool bUSBImGUiIsIncrement = false;
static int iUSBImGUIAddressStart = 0;
static char cUSBImGUIData[1020 + 254];
static char cUSBImGUIDataError[1024];

const std::string get_ft_status_message(FT_STATUS status)
{
	switch (status)
	{
	case FT_OK:
		return "OK";
	case FT_INVALID_HANDLE:
		return "Invalid g_ftHandle";
	case FT_DEVICE_NOT_FOUND:
		return "Device not found";
	case FT_DEVICE_NOT_OPENED:
		return "Failed to open device";
	case FT_IO_ERROR:
		return "Input/output error";
	case FT_INSUFFICIENT_RESOURCES:
		return "Insufficient resources";
	case FT_INVALID_PARAMETER:
		return "Invalid parameter provided";
	case FT_INVALID_BAUD_RATE:
		return "Invalid baud rate specified";
	case FT_DEVICE_NOT_OPENED_FOR_ERASE:
		return "Device not opened for erase";
	case FT_DEVICE_NOT_OPENED_FOR_WRITE:
		return "Device not opened for write";
	case FT_FAILED_TO_WRITE_DEVICE:
		return "Failed to write to device";
	case FT_EEPROM_READ_FAILED:
		return "Failed to read from EEPROM";
	case FT_EEPROM_WRITE_FAILED:
		return "Failed to write to EEPROM";
	case FT_EEPROM_ERASE_FAILED:
		return "Failed to erase EEPROM";
	case FT_EEPROM_NOT_PRESENT:
		return "EEPROM not present on device";
	case FT_EEPROM_NOT_PROGRAMMED:
		return "EEPROM is not programmed";
	case FT_INVALID_ARGS:
		return "Invalid arguments provided";
	case FT_NOT_SUPPORTED:
		return "Operation not supported";
	case FT_NO_MORE_ITEMS:
		return "No more items available";
	case FT_TIMEOUT:
		return "Operation timed out";
	case FT_OPERATION_ABORTED:
		return "Operation was aborted";
	case FT_RESERVED_PIPE:
		return "A reserved pipe was accessed";
	case FT_INVALID_CONTROL_REQUEST_DIRECTION:
		return "Invalid control request direction";
	case FT_INVALID_CONTROL_REQUEST_TYPE:
		return "Invalid control request type";
	case FT_IO_PENDING:
		return "Input/output operation is pending";
	case FT_IO_INCOMPLETE:
		return "Input/output operation is incomplete";
	case FT_HANDLE_EOF:
		return "End of file g_ftHandle reached";
	case FT_BUSY:
		return "Device is busy";
	case FT_NO_SYSTEM_RESOURCES:
		return "No system resources available";
	case FT_DEVICE_LIST_NOT_READY:
		return "Device list is not ready";
	case FT_DEVICE_NOT_CONNECTED:
		return "Device not connected";
	case FT_INCORRECT_DEVICE_PATH:
		return "Incorrect device path";
	case FT_OTHER_ERROR:
		return "Unknown error";
	default:
		return "Unknown status code";
	}
}

const std::string get_tini_name_string() { return std::string(activeNode.Description); };
const uint32_t get_tini_last_error() { return (uint32_t)ftStatus; };
const std::string get_tini_last_error_string() { return get_ft_status_message(ftStatus); };

const bool tini_is_ok()
{
	if (!bIsConnected)
		return FT_SUCCESS(FT_DEVICE_NOT_CONNECTED);
	return FT_SUCCESS(ftStatus);
}

const bool client_is_connected()
{
	return bIsConnected;
}

void clear_queues()
{
	packetInQueue.clear();
	packetFreeQueue.clear();
	for (size_t allocSize = 0; allocSize < 10000; allocSize++) // preallocate 10,000 packets
	{
		auto packet = std::make_shared<Packet>();
		packetFreeQueue.push(std::move(packet));
	}
}

void insert_event(SDHREvent *e)
{
	(void)e; // mark as unused
	assert("ERROR: CANNOT INSERT EVENT");
}

void terminate_processing_thread()
{
	// Force a dummy packet to process, so that shouldTerminateProcessing is triggered
	// and the loop is closed cleanly.
	auto packet = std::make_shared<Packet>();
	packetInQueue.push(std::move(packet));
}

void process_single_event(SDHREvent &e)
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
	VBLState_e vblState = VBLState_e::Unknown;
	if ((e.addr == 0xC019) && e.rw)
	{
		if ((e.data >> 7) == (e.is_iigs ? 1 : 0))
			vblState = VBLState_e::On;
		else
			vblState = VBLState_e::Off;
	}
	CycleCounter::GetInstance()->IncrementCycles(1, vblState);

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

	if (e.is_iigs && e.m2sel)
	{
		// ignore updates from iigs_mode firmware with m2sel high
		return;
	}
	if (e.rw && ((e.addr & 0xF000) != 0xC000))
	{
		// ignoring all read events not softswitches
		return;
	}

	auto memMgr = MemoryManager::GetInstance();

	/*
	 *********************************
	 HANDLE SIMPLE MEMORY WRITE EVENTS
	 *********************************
	 */
	if ((e.addr >= _A2_MEMORY_SHADOW_BEGIN) && (e.addr < _A2_MEMORY_SHADOW_END))
	{
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
	if ((e.is_iigs == true) || ((e.addr != CXSDHR_CTRL) && (e.addr != CXSDHR_DATA)))
	{
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
	// std::cerr << "cmd " << e.addr << " " << (uint32_t) e.data << std::endl;
	auto sdhrMgr = SDHRManager::GetInstance();
	auto a2VideoMgr = A2VideoManager::GetInstance();
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
			// #ifdef DEBUG
			std::cout << "CONTROL: Enable SDHR" << std::endl;
			// #endif
			sdhrMgr->ToggleSdhr(true);
			a2VideoMgr->ToggleA2Video(false);
			break;
		case SDHR_CTRL_RESET:
			// #ifdef DEBUG
			std::cout << "CONTROL: Reset SDHR" << std::endl;
			// #endif
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
			while (sdhrMgr->dataState != DATASTATE_e::DATA_IDLE)
			{
			};
			bool processingSucceeded = sdhrMgr->ProcessCommands();
			sdhrMgr->dataState = DATASTATE_e::DATA_UPDATED;
			if (processingSucceeded)
			{
#ifdef DEBUG
				std::cout << "Processing SDHR succeeded!" << std::endl;
#endif
			}
			else
			{
				// #ifdef DEBUG
				std::cerr << "ERROR: Processing SDHR failed!" << std::endl;
				// #endif
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

int process_usb_events_thread(std::atomic<bool> *shouldTerminateProcessing)
{
	std::cout << "starting usb processing thread" << std::endl;
	while (!(*shouldTerminateProcessing))
	{
		auto packet = packetInQueue.pop();
		rx_message_buffer.insert(rx_message_buffer.end(),
								 packet->data, packet->data + packet->size);
		packetFreeQueue.push(std::move(packet));
		uint32_t *s = (uint32_t *)&rx_message_buffer[0];
		uint32_t *b = s;
		auto word_size = rx_message_buffer.size() / 4;
		uint32_t *e = b + word_size;
		while (b < e)
		{
			if ((e - b) < 2)
			{
				// not enough for a header
				break;
			}
			bool addr_incr = (b[0] & (1 << 31)) != 0;
			uint32_t data_count = b[0] & 0xff;
			if ((e - b) < (2 + data_count))
			{
				// not enough for all data
				break;
			}
			uint32_t addr = b[1];
			b += 2;
			// printf("%u\n", data_count);
			for (uint32_t i = 0; i < data_count; ++i)
			{
				switch (addr)
				{
				case 0x1000:
				{
					uint32_t bus_event_state = b[i];
					std::cerr << "received bus event state: " << bus_event_state << std::endl;
				}
				break;
				case 0x1004:
				{
					uint32_t event = b[i];
					uint16_t addr = event & 0xffff;
					uint8_t misc = (event >> 16) & 0x0f;
					uint8_t data = (event >> 20) & 0xff;
					bool rw = (misc & 0x01) == 0x01;
					event_reset = ((misc & 0x02) == 0x02);
					// printf("A:%04x D:%02x RW:%u\n", addr, data, rw);
					if ((event_reset == 0) && (event_reset_prev == 1))
					{
						A2VideoManager::GetInstance()->bShouldReboot = true;
					}
					event_reset_prev = event_reset;
					SDHREvent ev(0, 0, 0, rw, addr, data);
					process_single_event(ev);
				}
				}
				if (addr_incr)
				{
					addr += 4;
				}
			}
			b += data_count;
		}
		auto data_removed = (b - s);
		if (data_removed > 0)
		{
			if (data_removed * 4 == rx_message_buffer.size())
			{
				rx_message_buffer.clear();
			}
			else
			{
				rx_message_buffer.erase(rx_message_buffer.begin(),
										rx_message_buffer.begin() + data_removed * 4);
			}
		}
	}
	return 0;
}

int usb_server_thread(std::atomic<bool> *shouldTerminateNetworking)
{
	eventRecorder = EventRecorder::GetInstance();
	clear_queues();
	std::cout << "Starting USB thread" << std::endl;
	ftStatusPrevious = 0xFFFF;
	bIsConnected = false;
	std::chrono::steady_clock::time_point next_connect_timeout{};
	while (!(*shouldTerminateNetworking))
	{
		if (!bIsConnected)
		{
			if (next_connect_timeout > std::chrono::steady_clock::now())
			{
				SDL_Delay(200);
				continue;
			}
			next_connect_timeout = std::chrono::steady_clock::now() + std::chrono::milliseconds(1000);
#ifdef __NETWORKING_WINDOWS__
			unsigned long count;
#else
			uint32_t count;
#endif
			FT_DEVICE_LIST_INFO_NODE nodes[16];

			activeNode = _FT_DEVICE_LIST_INFO_NODE();
			std::copy(std::begin("NO DEVICE"), std::end("NO DEVICE"), activeNode.Description);

			ftStatus = FT_CreateDeviceInfoList(&count);
			if (ftStatus != FT_OK)
			{
				if (ftStatus != ftStatusPrevious)
					std::cerr << "Failed to list FPGA usb devices: " << get_ft_status_message(ftStatus) << std::endl;
				ftStatusPrevious = ftStatus;
				continue;
			}
			if (count == 0)
			{
				ftStatus = FT_DEVICE_NOT_FOUND;
				if (ftStatus != ftStatusPrevious)
					std::cerr << "No FPGA usb devices found" << std::endl;
				ftStatusPrevious = ftStatus;
				continue;
			}

			ftStatus = FT_GetDeviceInfoList(nodes, &count);
			if (ftStatus != FT_OK)
			{
				if (ftStatus != ftStatusPrevious)
					std::cerr << "Failed to get FPGA usb device info list: " << get_ft_status_message(ftStatus) << std::endl;
				ftStatusPrevious = ftStatus;
				continue;
			}

			// Open the first available device
#ifdef __NETWORKING_WINDOWS__
			ftStatus = FT_Create((PVOID)nodes[0].SerialNumber, FT_OPEN_BY_SERIAL_NUMBER, &g_ftHandle);
#else
			FT_TRANSFER_CONF conf;
			memset(&conf, 0, sizeof(conf));
			conf.wStructSize = sizeof(conf);
			conf.pipe[FT_PIPE_DIR_IN].fNonThreadSafeTransfer = true;
			conf.pipe[FT_PIPE_DIR_OUT].fNonThreadSafeTransfer = true;
			for (uint32_t i = 0; i < 4; ++i)
			{
				FT_SetTransferParams(&conf, i);
			}
			ftStatus = FT_Create(0, FT_OPEN_BY_INDEX, &g_ftHandle);
#endif
			if (ftStatus != FT_OK)
			{
				std::cerr << "Failed to open FPGA usb device g_ftHandle: " << get_ft_status_message(ftStatus) << std::endl;
				continue;
			}

			// Set pipe timeouts. Probably only necessary on Windows
			ftStatus = FT_SetPipeTimeout(g_ftHandle, FT_PIPE_WRITE_ID, 1000); // Pipe to write to
			if (ftStatus != FT_OK)
			{
				std::cerr << "Failed to set write pipe timeout: " << get_ft_status_message(ftStatus) << std::endl;
				FT_Close(g_ftHandle);
				g_ftHandle = NULL;
				continue;
			}
			ftStatus = FT_SetPipeTimeout(g_ftHandle, FT_PIPE_READ_ID, 1000); // Pipe to read from
			if (ftStatus != FT_OK)
			{
				std::cerr << "Failed to set read pipe timeout: " << get_ft_status_message(ftStatus) << std::endl;
				FT_Close(g_ftHandle);
				g_ftHandle = NULL;
				continue;
			}
			/*
			ftStatus = FT_SetStreamPipe(g_ftHandle, true, true, 0, PKT_BUFSZ);
			if (ftStatus != FT_OK) {
				std::cerr << "Failed to set Stream pipe size" << std::endl;
				FT_Close(g_ftHandle);
				g_ftHandle = NULL;
				continue;
			}
			*/

			activeNode = nodes[0];
			std::cerr << "Connected to FPGA usb device" << std::endl;
			bIsConnected = true;

			// set the no slot clock time
			time_t t = time(NULL);
			struct tm time_val;
#ifdef __NETWORKING_WINDOWS__
			localtime_s(&time_val, &t);
#else
			localtime_r(&t, &time_val);
#endif
			uint32_t set_time_buf[4];
			set_time_buf[0] = 0x80000002; // incr set, 2 data fields;
			set_time_buf[1] = 0x00000014; // address of time set location
			ULONG bytes_transferred;
			uint8_t* p = (uint8_t*)(&set_time_buf[2]);
			*p++ = 0;
			*p++ = ((time_val.tm_sec / 10) << 4) + (time_val.tm_sec % 10);
			*p++ = ((time_val.tm_min / 10) << 4) + (time_val.tm_min % 10);
			*p++ = ((time_val.tm_hour / 10) << 4) + (time_val.tm_hour % 10);
			*p++ = (((time_val.tm_wday+1) / 10) << 4) + ((time_val.tm_wday+1) % 10);
			*p++ = ((time_val.tm_mday / 10) << 4) + (time_val.tm_mday % 10);
			*p++ = (((time_val.tm_mon+1) / 10) << 4) + ((time_val.tm_mon+1) % 10);
			*p++ = (((time_val.tm_year % 100) / 10) << 4) + ((time_val.tm_year % 100) % 10);
			printf("setting time: %04x%04x\n",set_time_buf[3],set_time_buf[2]);
			uint32_t set_time_msg_len = 16;
			ftStatus = FT_WritePipeEx(g_ftHandle, 0, (uint8_t *)set_time_buf, set_time_msg_len, &bytes_transferred, 0);
			if (ftStatus != FT_OK)
			{
				std::cerr << "Failed to set time to FPGA: " << get_ft_status_message(ftStatus) << std::endl;
			}

			// enable bus events
			uint32_t enable_msg_buf[3];
			enable_msg_buf[0] = 0x80000001; // incr set, 1 data field
			enable_msg_buf[1] = 0x00001000; // address of bus_event_control
			enable_msg_buf[2] = 0x00000001; // bit 0 indicates enable bus events
			uint32_t enable_msg_buf_len = 12;
			ftStatus = FT_WritePipeEx(g_ftHandle, 0, (uint8_t *)enable_msg_buf, enable_msg_buf_len, &bytes_transferred, 0);
			if (ftStatus != FT_OK)
			{
				std::cerr << "Failed to enable FPGA bus events: " << get_ft_status_message(ftStatus) << std::endl;
			}
		}

		auto packet = packetFreeQueue.pop();
		// Synchronous Read
#ifdef __NETWORKING_WINDOWS__
		ftStatus = FT_ReadPipeEx(g_ftHandle, FT_PIPE_READ_ID, packet->data, PKT_BUFSZ, (ULONG *)&(packet->size), NULL);
#else
		ftStatus = FT_ReadPipeEx(g_ftHandle, 0, packet->data, PKT_BUFSZ, (ULONG *)&(packet->size), 1000);
#endif
		if (ftStatus != FT_OK)
		{
			// FT_TIMEOUT means the Apple is off. No data is coming in
			if (ftStatus != FT_TIMEOUT)
			{
				std::cerr << "Failed to read from FPGA usb packet pipe: " << get_ft_status_message(ftStatus) << std::endl;
			}
			FT_AbortPipe(g_ftHandle, FT_PIPE_READ_ID);
			packetFreeQueue.push(std::move(packet));
			continue;
		}

		if (!eventRecorder->IsInReplayMode())
		{
			packetInQueue.push(std::move(packet));
		}
		else
		{
			packetFreeQueue.push(std::move(packet));
		}
	}
	std::cout << "ending usb read loop" << std::endl;
	return 0;
}

uint32_t usb_write_register(uint32_t addressStart, const std::vector<uint32_t>* vData, bool setIncrement)
{
	if (g_ftHandle == NULL)
		return 0;
	uint32_t enable_msg_buf[256];	// max 256 entries, 254 data fields
	uint32_t vDataSize = (uint32_t)vData->size();
	if (vDataSize > ((sizeof(enable_msg_buf) / sizeof(enable_msg_buf[0])) - 2))
	{
		std::cerr << "ERROR: Too much data sent to usb_write_register!" << std::endl;
		return 0;
	}
	enable_msg_buf[0] = 0x0;
	if (setIncrement)
		enable_msg_buf[0] = 0x80000000;
	enable_msg_buf[0] += vDataSize;
	enable_msg_buf[1] = addressStart;
	for (size_t i = 0; i < vDataSize; ++i)
	{
		enable_msg_buf[2 + i] = vData->at(i);
	}
	uint32_t enable_msg_buf_len = (2 + vDataSize) * sizeof(enable_msg_buf[0]);
	ULONG bytes_transferred;
	ftStatus = FT_WritePipeEx(g_ftHandle, 0, (uint8_t*)enable_msg_buf, enable_msg_buf_len, &bytes_transferred, 0);
	if (ftStatus != FT_OK)
	{
		std::cerr << "Failed to write pipe ex: " << get_ft_status_message(ftStatus) << std::endl;
		return 0;
	}
	return 1;
}

void usb_display_imgui_window(bool* p_open)
{
	bUSBImGUiWindowIsOpen = p_open;
	if (p_open)
	{
		ImGui::SetNextWindowSizeConstraints(ImVec2(420, 180), ImVec2(FLT_MAX, FLT_MAX));
		ImGui::Begin("Appletini Communications", p_open);
		if (!ImGui::IsWindowCollapsed())
		{
			ImGui::Checkbox("Increment", &bUSBImGUiIsIncrement);
			ImGui::DragInt("Start Address", &iUSBImGUIAddressStart, 1.f, 0, 0x4000, "%04X");
			ImGui::InputText("Data", cUSBImGUIData, sizeof(cUSBImGUIData));
			ImGui::SetItemTooltip("Data is space-delimited 4 bytes in hex, e.g.: 4ce20001 0000ffa2. Max of 254 4-byte values.");
			if (ImGui::Button("Write to Appletini"))
			{
				constexpr size_t TOKEN_LEN = 8;  // 8 hex chars == 4 bytes
				std::string_view sv(cUSBImGUIData);
				std::vector<uint32_t> result;
				size_t i = 0, n = sv.size();

				while (i < n) {
					// Ensure enough characters remain
					if (i + TOKEN_LEN > n) {
						snprintf(cUSBImGUIDataError, sizeof(cUSBImGUIDataError), "Unexpected end of data at position %zu", i);
						goto ENDWRITE;
					}

					// Extract the 8-char token
					auto token = sv.substr(i, TOKEN_LEN);

					// Validate each is a hex digit
					for (char c : token) {
						if (!std::isxdigit(static_cast<unsigned char>(c))) {
							snprintf(cUSBImGUIDataError, sizeof(cUSBImGUIDataError), "Invalid hex digit %c in token at pos %zu", c, i);
							goto ENDWRITE;
						}
					}

					// Parse hex to uint32_t
					uint32_t value = 0;
					auto [ptr, ec] = std::from_chars(token.data(), token.data() + token.size(), value, 16);
					if (ec != std::errc()) {
						snprintf(cUSBImGUIDataError, sizeof(cUSBImGUIDataError), "Failed to parse token %s as hex at pos %zu", std::string(token).c_str(), i);
						goto ENDWRITE;
					}
					result.push_back(value);

					i += TOKEN_LEN;
					if (i == n) {
						break;  // reached end exactly
					}

					// Next character must be a space
					if (sv[i] != ' ') {
						snprintf(cUSBImGUIDataError, sizeof(cUSBImGUIDataError), "Expected space at position %zu, found %c", i, sv[i]);
						goto ENDWRITE;
					}
					++i;  // skip the space
				}
				// now send to appletini
				auto _res = usb_write_register(iUSBImGUIAddressStart, &result, bUSBImGUiIsIncrement);
				if (_res == 0)
					snprintf(cUSBImGUIDataError, sizeof(cUSBImGUIDataError), "FT Write Pipe Ex failed: %s", get_ft_status_message(ftStatus).c_str());
				else
					cUSBImGUIDataError[0] = '\0';
			}
		ENDWRITE:
			if (cUSBImGUIDataError[0] != '\0')
				ImGui::TextColored(ImVec4(0.9f, 0.f, 0.f, 1.f), "%s", cUSBImGUIDataError);
		}
		ImGui::End();
	}
}
