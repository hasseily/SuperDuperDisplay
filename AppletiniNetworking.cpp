#include "ConcurrentQueue.h"
#include "AppletiniNetworking.h"
#include "MemoryManager.h"
#include "A2VideoManager.h"
#include "SoundManager.h"
#include "MockingboardManager.h"
#include "CycleCounter.h"
#include "EventRecorder.h"
#include "MainMenu.h"
#include <time.h>
#include <fcntl.h>
#include <chrono>
#include <bitset>
#include <sstream>
#include <charconv>
#ifdef __NETWORKING_WINDOWS__
// Native WinUSB transport for the Appletini SDD vendor device. The card
// carries MS OS 2.0 descriptors, so Windows auto-binds winusb.sys and
// registers the device interface GUID below -- no driver install.
#include <windows.h>
#include <setupapi.h>
#include <winusb.h>
#include <initguid.h>
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "winusb.lib")
// {F5A31C8E-7D3B-4E1C-9A64-52AA35C10B71} -- must match the firmware's
// MS OS 2.0 DeviceInterfaceGUIDs registry property.
DEFINE_GUID(GUID_DEVINTERFACE_APPLETINI_SDD,
    0xF5A31C8E, 0x7D3B, 0x4E1C,
    0x9A, 0x64, 0x52, 0xAA, 0x35, 0xC1, 0x0B, 0x71);
#endif

// Appletini SDD vendor device: single bulk pair on EP1.
// (On Windows these are WinUSB pipe IDs; on macOS/Linux they are the
// libusb endpoint addresses. Same values either way.)
#define TINI_PIPE_WRITE 0x01
#define TINI_PIPE_READ  0x81

// Kept for UI compatibility: error 19 has always meant "read timed out"
// (the Apple II is off / idle) and MainMenu special-cases it.
#define TINI_ERR_OK      0
#define TINI_ERR_TIMEOUT 19
#define TINI_ERR_IO      4
#define TINI_ERR_NODEV   3

#ifdef __NETWORKING_WINDOWS__
// The USB handles to the Appletini
static HANDLE g_tiniFile = INVALID_HANDLE_VALUE;
static WINUSB_INTERFACE_HANDLE g_tiniWinusb = NULL;
#endif

static EventRecorder *eventRecorder;
static bool bIsConnected = false;
static uint64_t num_processed_packets = 0;
static uint64_t duration_packet_processing_ns = 0;
static uint64_t duration_network_processing_ns = 0;
static uint32_t ftStatus = TINI_ERR_NODEV, ftStatusPrevious = 0xFFFF;
static std::string activeDeviceName = "NO DEVICE";

// Only do a single reset if a string of reset events arrive
static bool event_reset = 1;
static bool event_reset_prev = 1;

// Atomics to ask the server process to send messages to the tini
std::atomic<bool> bRequestEnableBusEvents = false;
// Ask the server thread for a full stream resync: disable bus events, drain
// everything in flight, then re-enable. This is the recovery path whenever
// byte-stream framing may have been lost (parser desync, reconnect, replay),
// as opposed to bRequestEnableBusEvents which assumes framing is intact.
std::atomic<bool> bRequestResyncBusEvents = false;
// Incremented by the server thread after each drain, before re-enabling.
// Every received packet is stamped with it; the processing thread discards
// its partial message buffer when the stamp changes, so stale pre-resync
// bytes can never be glued to the fresh, aligned stream.
static std::atomic<uint32_t> streamGeneration{0};

// The Appletini's register message format is [header][address][data words...],
// where header bit 31 is the address-increment flag, bits 24-30 are unused,
// and bits 0-23 are the data word count. Real bursts are at most 16640 bytes
// (4160 words), so anything above this cap - or nonzero unused bits - means
// the byte stream lost framing (e.g. a truncated USB read) and every
// buffered byte is unusable.
#define TINI_MAX_MSG_WORDS 0x10000
// Insurance cap on the reassembly buffer; a legitimate incomplete message
// can never buffer more than TINI_MAX_MSG_WORDS*4 + one USB packet.
#define TINI_MAX_RX_BUFFER (1024 * 1024)
// After this much silence the watchdog forces a stream resync. Harmless if
// the Apple is simply off; recovers a silently wedged stream otherwise.
#define TINI_WATCHDOG_SECONDS 5

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

static float fUSBMouseSensitivity = 1.0f;

const std::string get_ft_status_message(uint32_t status)
{
	switch (status)
	{
	case TINI_ERR_OK:
		return "OK";
	case TINI_ERR_TIMEOUT:
		return "Operation timed out";
	case TINI_ERR_IO:
		return "Input/output error";
	case TINI_ERR_NODEV:
		return "Device not found";
	default:
		return "Unknown status code";
	}
}

constexpr bool state_has_flag(uint32_t value, BusEventFlags flag) {
	return (value & static_cast<uint32_t>(flag)) != 0;
}

std::string bus_event_state_to_string(uint32_t state) {
	std::ostringstream oss;
	oss << "0x" << std::hex << state
		<< " [" << std::bitset<8>(state) << "] "; // only show low 8 bits here
	if (state_has_flag(state, BusEventFlags::EventEnable)) oss << "EventEnable ";
	if (state_has_flag(state, BusEventFlags::Overflow))    oss << "Overflow ";
	if (!(state & ((1u << 2) - 1))) oss << "None";
	return oss.str();
}

const std::string get_tini_name_string() { return activeDeviceName; };
const uint32_t get_tini_last_error() { return ftStatus; };
const std::string get_tini_last_error_string() { return get_ft_status_message(ftStatus); };
const std::string get_tini_last_error_string_async()
{
	return get_ft_status_message(ftStatus);
}

const bool tini_is_ok()
{
	if (!bIsConnected)
		return false;
	return (ftStatus == TINI_ERR_OK) || (ftStatus == TINI_ERR_TIMEOUT);
}

const bool client_is_connected()
{
	return bIsConnected;
}

void clear_queues()
{
	packetInQueue.clear();
	packetFreeQueue.clear();
	for (size_t allocSize = 0; allocSize < 1024; allocSize++) // preallocate ~4s of full-rate buffering
	{
		auto packet = std::make_shared<Packet>();
		packetFreeQueue.push(std::move(packet));
	}
}

void insert_event(NetEvent *e)
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

void process_single_event(NetEvent &e)
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
	if (e.is_iigs == true)
	{
		if (e.addr >> 8 == 0xc0)
			memMgr->ProcessSoftSwitch(e.addr, e.data, e.rw, e.is_iigs);
		// ignore non-control
		return;
	}
}

int process_usb_events_thread(std::atomic<bool> *shouldTerminateProcessing)
{
	std::cout << "starting usb processing thread" << std::endl;
	uint32_t currentGeneration = streamGeneration.load(std::memory_order_acquire);
	bool bParserDesynced = false;
	while (!(*shouldTerminateProcessing))
	{
		auto packet = packetInQueue.pop();
		if (packet->generation != currentGeneration)
		{
			// A stream resync completed: the new generation starts at a clean
			// message boundary, so drop any partial message from the old stream.
			currentGeneration = packet->generation;
			rx_message_buffer.clear();
			bParserDesynced = false;
		}
		else if (bParserDesynced)
		{
			// Framing was lost: everything is garbage until the resync
			packetFreeQueue.push(std::move(packet));
			continue;
		}
		rx_message_buffer.insert(rx_message_buffer.end(),
								 packet->data, packet->data + packet->size);
		packetFreeQueue.push(std::move(packet));
		bool bDesyncDetected = (rx_message_buffer.size() > TINI_MAX_RX_BUFFER);
		uint32_t *s = (uint32_t *)&rx_message_buffer[0];
		uint32_t *b = s;
		auto word_size = rx_message_buffer.size() / 4;
		uint32_t *e = b + word_size;
		while (!bDesyncDetected && (b < e))
		{
			if ((e - b) < 2)
			{
				// not enough for a header
				break;
			}
			bool addr_incr = (b[0] & (1 << 31)) != 0;
			uint32_t data_count = b[0] & 0xffffff;
			if (((b[0] & 0x7F000000u) != 0) || (data_count > TINI_MAX_MSG_WORDS))
			{
				// impossible header: the byte stream lost framing
				bDesyncDetected = true;
				break;
			}
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
					std::cerr << "Received state event: " << bus_event_state_to_string(bus_event_state) << std::endl;
					if (state_has_flag(bus_event_state, BusEventFlags::Overflow))
					{
						// we're in overflow mode, re-enable bus events
						std::cerr << "Lost synchronization, resynching now." << std::endl;
						bRequestEnableBusEvents.store(true, std::memory_order_release);
					}
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
						//printf("A:%04x D:%02x RW:%u\n", addr, data, rw);
						A2VideoManager::GetInstance()->bShouldReboot = true;
					}
					event_reset_prev = event_reset;
					NetEvent ev(0, 0, 0, rw, addr, data);
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
		if (bDesyncDetected)
		{
			std::cerr << "ERROR: Appletini bus stream desynchronized, forcing a stream resync" << std::endl;
			rx_message_buffer.clear();
			bParserDesynced = true;
			bRequestResyncBusEvents.store(true, std::memory_order_release);
			continue;
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

#ifdef __NETWORKING_WINDOWS__

static void tini_close()
{
	if (g_tiniWinusb != NULL) {
		WinUsb_Free(g_tiniWinusb);
		g_tiniWinusb = NULL;
	}
	if (g_tiniFile != INVALID_HANDLE_VALUE) {
		CloseHandle(g_tiniFile);
		g_tiniFile = INVALID_HANDLE_VALUE;
	}
	bIsConnected = false;
}

// Find the first Appletini SDD interface and open it. Returns true on
// success and fills activeDeviceName.
static bool tini_open()
{
	HDEVINFO devs = SetupDiGetClassDevs(&GUID_DEVINTERFACE_APPLETINI_SDD,
		NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
	if (devs == INVALID_HANDLE_VALUE)
		return false;

	SP_DEVICE_INTERFACE_DATA ifData;
	ifData.cbSize = sizeof(ifData);
	bool opened = false;

	if (SetupDiEnumDeviceInterfaces(devs, NULL,
			&GUID_DEVINTERFACE_APPLETINI_SDD, 0, &ifData)) {
		BYTE detailBuf[1024] = { 0 };
		auto detail = reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA>(detailBuf);
		detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
		if (SetupDiGetDeviceInterfaceDetail(devs, &ifData, detail,
				sizeof(detailBuf), NULL, NULL)) {
			g_tiniFile = CreateFile(detail->DevicePath,
				GENERIC_READ | GENERIC_WRITE,
				FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
				OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL);
			if (g_tiniFile != INVALID_HANDLE_VALUE) {
				if (WinUsb_Initialize(g_tiniFile, &g_tiniWinusb)) {
					// 1 s read timeout: distinguishes "Apple is off"
					// from a dead device without blocking forever.
					ULONG timeout = 1000;
					WinUsb_SetPipePolicy(g_tiniWinusb, TINI_PIPE_READ,
						PIPE_TRANSFER_TIMEOUT, sizeof(timeout), &timeout);
					timeout = 1000;
					WinUsb_SetPipePolicy(g_tiniWinusb, TINI_PIPE_WRITE,
						PIPE_TRANSFER_TIMEOUT, sizeof(timeout), &timeout);
					activeDeviceName = "Appletini SDD Stream";
					opened = true;
				} else {
					CloseHandle(g_tiniFile);
					g_tiniFile = INVALID_HANDLE_VALUE;
				}
			}
		}
	}
	SetupDiDestroyDeviceInfoList(devs);
	return opened;
}

static bool tini_write(const uint8_t *buf, uint32_t len)
{
	ULONG sent = 0;
	if (g_tiniWinusb == NULL)
		return false;
	if (!WinUsb_WritePipe(g_tiniWinusb, TINI_PIPE_WRITE,
			const_cast<PUCHAR>(buf), len, &sent, NULL))
		return false;
	return sent == len;
}

// Resynchronize the bus event stream: disable bus events, drain everything
// still in flight (a full read timeout proves the pipeline is empty), bump
// the stream generation, then re-enable. After this the next byte received
// is guaranteed to start a fresh message. Returns false on any transport
// failure, in which case the caller should drop the handle and reconnect.
static bool tini_resync_stream()
{
	static uint8_t drainBuf[PKT_BUFSZ];
	uint32_t ctl_msg_buf[3];
	ctl_msg_buf[0] = 0x00000001; // 1 data field
	ctl_msg_buf[1] = 0x00001000; // address of bus_event_control
	ctl_msg_buf[2] = 0x00000000; // disable bus events
	if (!tini_write((uint8_t *)ctl_msg_buf, 12))
	{
		std::cerr << "Appletini resync: disable write failed" << std::endl;
		return false;
	}
	// Bound the drain in case the device ignores the disable
	auto drain_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
	while (true)
	{
		ULONG got = 0;
		BOOL ok = WinUsb_ReadPipe(g_tiniWinusb, TINI_PIPE_READ,
			drainBuf, PKT_BUFSZ, &got, NULL);
		if (!ok)
		{
			if (GetLastError() == ERROR_SEM_TIMEOUT)
				break;	// drained
			std::cerr << "Appletini resync: drain read failed" << std::endl;
			return false;
		}
		if (std::chrono::steady_clock::now() > drain_deadline)
		{
			std::cerr << "Appletini resync: stream did not stop on disable" << std::endl;
			return false;
		}
	}
	// Everything received from here on belongs to the new, aligned stream
	streamGeneration.fetch_add(1, std::memory_order_release);
	ctl_msg_buf[2] = 0x00000001; // enable bus events
	if (!tini_write((uint8_t *)ctl_msg_buf, 12))
	{
		std::cerr << "Appletini resync: enable write failed" << std::endl;
		return false;
	}
	return true;
}

int usb_server_thread(std::atomic<bool> *shouldTerminateNetworking)
{
	eventRecorder = EventRecorder::GetInstance();
	clear_queues();
	std::cout << "Starting USB thread (Appletini native WinUSB)" << std::endl;
	ftStatusPrevious = 0xFFFF;
	bIsConnected = false;
	std::chrono::steady_clock::time_point next_connect_timeout{};
	std::chrono::steady_clock::time_point last_data_time = std::chrono::steady_clock::now();
	std::chrono::steady_clock::time_point next_watchdog_resync{};
	bool bWatchdogAnnounced = false;

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

			activeDeviceName = "NO DEVICE";
			if (!tini_open())
			{
				ftStatus = TINI_ERR_NODEV;
				if (ftStatus != ftStatusPrevious)
					std::cerr << "No Appletini SDD device found" << std::endl;
				ftStatusPrevious = ftStatus;
				continue;
			}

			std::cerr << "Connected to Appletini SDD device" << std::endl;
			ftStatus = TINI_ERR_OK;
			bIsConnected = true;

			// set the no slot clock time
			time_t tt = time(NULL);
			struct tm time_val;
			localtime_s(&time_val, &tt);
			uint32_t set_time_buf[4];
			set_time_buf[0] = 0x80000002; // incr set, 2 data fields;
			set_time_buf[1] = 0x00000014; // address of time set location
			uint8_t* tp = (uint8_t*)(&set_time_buf[2]);
			*tp++ = 0;
			*tp++ = ((time_val.tm_sec / 10) << 4) + (time_val.tm_sec % 10);
			*tp++ = ((time_val.tm_min / 10) << 4) + (time_val.tm_min % 10);
			*tp++ = ((time_val.tm_hour / 10) << 4) + (time_val.tm_hour % 10);
			*tp++ = (((time_val.tm_wday + 1) / 10) << 4) + ((time_val.tm_wday + 1) % 10);
			*tp++ = ((time_val.tm_mday / 10) << 4) + (time_val.tm_mday % 10);
			*tp++ = (((time_val.tm_mon + 1) / 10) << 4) + (time_val.tm_mon % 10);
			*tp++ = (((time_val.tm_year % 100) / 10) << 4) + ((time_val.tm_year % 100) % 10);
			printf("Setting time... ");
			if (!tini_write((uint8_t *)set_time_buf, 16))
				std::cerr << "failed!" << std::endl;
			else
				std::cerr << "done!" << std::endl;

			// Full resync rather than a plain enable: the device FIFO may
			// still hold a partial message from before the (re)connect
			bRequestResyncBusEvents.store(true, std::memory_order_release);
			last_data_time = std::chrono::steady_clock::now();
			bWatchdogAnnounced = false;
		}

		// full stream resync when necessary
		if (bRequestResyncBusEvents.load(std::memory_order_acquire)) {
			if (!tini_resync_stream()) {
				// Leave the request set: the reconnect path requests a
				// resync again once the device is reopened
				tini_close();
				continue;
			}
			bRequestResyncBusEvents.store(false, std::memory_order_release);
			last_data_time = std::chrono::steady_clock::now();
		}

		// enable bus events when necessary
		if (bRequestEnableBusEvents.load(std::memory_order_acquire)) {
			std::cerr << "Enabling Appletini bus events... ";
			uint32_t enable_msg_buf[3];
			enable_msg_buf[0] = 0x00000001; // 1 data field
			enable_msg_buf[1] = 0x00001000; // address of bus_event_control
			enable_msg_buf[2] = 0x00000001; // bit 0 indicates enable bus events
			if (!tini_write((uint8_t *)enable_msg_buf, 12)) {
				// A failed 12-byte control write means the device is gone
				// or wedged: drop the handle and reconnect
				std::cerr << "failed!" << std::endl;
				tini_close();
				continue;
			}
			std::cerr << "done!" << std::endl;
			// Only clear the request once the FPGA has received it
			bRequestEnableBusEvents.store(false, std::memory_order_release); // reset
		}

		auto packet = packetFreeQueue.pop();
		ULONG got = 0;
		BOOL ok = WinUsb_ReadPipe(g_tiniWinusb, TINI_PIPE_READ,
			packet->data, PKT_BUFSZ, &got, NULL);
		packet->size = got;

		if (!ok)
		{
			DWORD err = GetLastError();
			if (err == ERROR_SEM_TIMEOUT)
			{
				// The Apple is off / no events flowing. Stay connected.
				ftStatus = TINI_ERR_TIMEOUT;
				// Watchdog: the Apple bus generates events on every cycle
				// when the machine is running, so prolonged silence is either
				// "Apple off" (a resync is harmless) or a wedged stream
				// (a resync recovers it). Resync periodically until data flows.
				auto now = std::chrono::steady_clock::now();
				if ((now - last_data_time > std::chrono::seconds(TINI_WATCHDOG_SECONDS))
					&& (now >= next_watchdog_resync))
				{
					if (!bWatchdogAnnounced)
					{
						std::cerr << "No Appletini bus events, resyncing stream" << std::endl;
						bWatchdogAnnounced = true;
					}
					bRequestResyncBusEvents.store(true, std::memory_order_release);
					next_watchdog_resync = now + std::chrono::seconds(TINI_WATCHDOG_SECONDS);
				}
			}
			else
			{
				// Real transport failure (device rebooted, personality
				// switched, unplugged): drop the handle and reconnect.
				ftStatus = TINI_ERR_IO;
				if (ftStatus != ftStatusPrevious)
					std::cerr << "Appletini read failed (err " << err
						  << "), reconnecting" << std::endl;
				ftStatusPrevious = ftStatus;
				tini_close();
			}
			packetFreeQueue.push(std::move(packet));
			continue;
		}
		ftStatus = TINI_ERR_OK;
		packet->generation = streamGeneration.load(std::memory_order_acquire);
		last_data_time = std::chrono::steady_clock::now();
		bWatchdogAnnounced = false;

		if (!eventRecorder->IsInReplayMode() && packet->size > 0)
		{
			packetInQueue.push(std::move(packet));
		}
		else
		{
			packetFreeQueue.push(std::move(packet));
		}
	}
	std::cout << "ending usb read loop" << std::endl;
	tini_close();
	return 0;
}

#else /* !__NETWORKING_WINDOWS__ */

// Native libusb-1.0 transport for the Appletini SDD vendor device
// (macOS / Linux). Same semantics as the WinUSB path above: open by
// VID/PID, claim the single vendor interface, 1 s read timeout
// distinguishes "the Apple is off" from a dead device, and any real
// transport error drops the handle and reconnects.
//
// Linux note: non-root access needs a udev rule -- see
// 99-appletini-sdd.rules at the repo root.
#include <libusb.h>

#define TINI_USB_VID 0x1209
#define TINI_USB_PID 0xA271

static libusb_context *g_usbCtx = nullptr;
static libusb_device_handle *g_tiniDev = nullptr;

static void tini_close()
{
	if (g_tiniDev != nullptr) {
		libusb_release_interface(g_tiniDev, 0);
		libusb_close(g_tiniDev);
		g_tiniDev = nullptr;
	}
	bIsConnected = false;
}

static bool tini_open()
{
	if (g_usbCtx == nullptr) {
		if (libusb_init(&g_usbCtx) != 0)
			return false;
	}
	g_tiniDev = libusb_open_device_with_vid_pid(g_usbCtx,
		TINI_USB_VID, TINI_USB_PID);
	if (g_tiniDev == nullptr)
		return false;
	// No kernel driver binds a vendor interface on Linux, but be safe;
	// this is a no-op on macOS.
	libusb_set_auto_detach_kernel_driver(g_tiniDev, 1);
	if (libusb_claim_interface(g_tiniDev, 0) != 0) {
		libusb_close(g_tiniDev);
		g_tiniDev = nullptr;
		return false;
	}

	// A bulk-IN buffer that ends in the middle of a USB packet can overflow on
	// libusb's Darwin backend.  Verify the descriptor at runtime as well as
	// keeping PKT_BUFSZ aligned for full/high/SuperSpeed devices.
	int maxPacketSize = libusb_get_max_packet_size(
		libusb_get_device(g_tiniDev), TINI_PIPE_READ);
	if ((maxPacketSize <= 0) || ((PKT_BUFSZ % maxPacketSize) != 0)) {
		std::cerr << "Invalid Appletini bulk-IN packet size ("
			  << maxPacketSize << ") for " << PKT_BUFSZ
			  << "-byte receive buffer" << std::endl;
		libusb_release_interface(g_tiniDev, 0);
		libusb_close(g_tiniDev);
		g_tiniDev = nullptr;
		return false;
	}
	activeDeviceName = "Appletini SDD Stream";
	return true;
}

static bool tini_write(const uint8_t *buf, uint32_t len)
{
	int sent = 0;
	if (g_tiniDev == nullptr)
		return false;
	int r = libusb_bulk_transfer(g_tiniDev, TINI_PIPE_WRITE,
		const_cast<uint8_t *>(buf), (int)len, &sent, 1000);
	if (r != LIBUSB_SUCCESS) {
		std::cerr << "libusb write failed (" << libusb_error_name(r)
			  << ", sent " << sent << "/" << len << ")" << std::endl;
		return false;
	}
	if (sent != (int)len) {
		std::cerr << "libusb short write (sent " << sent << "/"
			  << len << ")" << std::endl;
		return false;
	}
	return true;
}

// Resynchronize the bus event stream: disable bus events, drain everything
// still in flight (a full read timeout proves the pipeline is empty), bump
// the stream generation, then re-enable. After this the next byte received
// is guaranteed to start a fresh message. Returns false on any transport
// failure, in which case the caller should drop the handle and reconnect.
static bool tini_resync_stream()
{
	static uint8_t drainBuf[PKT_BUFSZ];
	uint32_t ctl_msg_buf[3];
	ctl_msg_buf[0] = 0x00000001; // 1 data field
	ctl_msg_buf[1] = 0x00001000; // address of bus_event_control
	ctl_msg_buf[2] = 0x00000000; // disable bus events
	if (!tini_write((uint8_t *)ctl_msg_buf, 12))
	{
		std::cerr << "Appletini resync: disable write failed" << std::endl;
		return false;
	}
	// Bound the drain in case the device ignores the disable
	auto drain_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
	while (true)
	{
		int got = 0;
		int r = libusb_bulk_transfer(g_tiniDev, TINI_PIPE_READ,
			drainBuf, PKT_BUFSZ, &got, 1000);
		if ((r == LIBUSB_ERROR_TIMEOUT) && (got == 0))
			break;	// drained
		if ((r != LIBUSB_SUCCESS) && (r != LIBUSB_ERROR_TIMEOUT))
		{
			std::cerr << "Appletini resync: drain read failed ("
				  << libusb_error_name(r) << ")" << std::endl;
			return false;
		}
		if (std::chrono::steady_clock::now() > drain_deadline)
		{
			std::cerr << "Appletini resync: stream did not stop on disable" << std::endl;
			return false;
		}
	}
	// Everything received from here on belongs to the new, aligned stream
	streamGeneration.fetch_add(1, std::memory_order_release);
	ctl_msg_buf[2] = 0x00000001; // enable bus events
	if (!tini_write((uint8_t *)ctl_msg_buf, 12))
	{
		std::cerr << "Appletini resync: enable write failed" << std::endl;
		return false;
	}
	return true;
}

int usb_server_thread(std::atomic<bool> *shouldTerminateNetworking)
{
	eventRecorder = EventRecorder::GetInstance();
	clear_queues();
	std::cout << "Starting USB thread (Appletini native libusb)" << std::endl;
	ftStatusPrevious = 0xFFFF;
	bIsConnected = false;
	std::chrono::steady_clock::time_point next_connect_timeout{};
	std::chrono::steady_clock::time_point last_data_time = std::chrono::steady_clock::now();
	std::chrono::steady_clock::time_point next_watchdog_resync{};
	bool bWatchdogAnnounced = false;

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

			activeDeviceName = "NO DEVICE";
			if (!tini_open())
			{
				ftStatus = TINI_ERR_NODEV;
				if (ftStatus != ftStatusPrevious)
					std::cerr << "No Appletini SDD device found" << std::endl;
				ftStatusPrevious = ftStatus;
				continue;
			}

			std::cerr << "Connected to Appletini SDD device" << std::endl;
			ftStatus = TINI_ERR_OK;
			bIsConnected = true;

			// set the no slot clock time
			time_t tt = time(NULL);
			struct tm time_val;
			localtime_r(&tt, &time_val);
			uint32_t set_time_buf[4];
			set_time_buf[0] = 0x80000002; // incr set, 2 data fields;
			set_time_buf[1] = 0x00000014; // address of time set location
			uint8_t* tp = (uint8_t*)(&set_time_buf[2]);
			*tp++ = 0;
			*tp++ = ((time_val.tm_sec / 10) << 4) + (time_val.tm_sec % 10);
			*tp++ = ((time_val.tm_min / 10) << 4) + (time_val.tm_min % 10);
			*tp++ = ((time_val.tm_hour / 10) << 4) + (time_val.tm_hour % 10);
			*tp++ = (((time_val.tm_wday + 1) / 10) << 4) + ((time_val.tm_wday + 1) % 10);
			*tp++ = ((time_val.tm_mday / 10) << 4) + (time_val.tm_mday % 10);
			*tp++ = (((time_val.tm_mon + 1) / 10) << 4) + (time_val.tm_mon % 10);
			*tp++ = (((time_val.tm_year % 100) / 10) << 4) + ((time_val.tm_year % 100) % 10);
			printf("Setting time... ");
			if (!tini_write((uint8_t *)set_time_buf, 16))
				std::cerr << "failed!" << std::endl;
			else
				std::cerr << "done!" << std::endl;

			// Full resync rather than a plain enable: the device FIFO may
			// still hold a partial message from before the (re)connect
			bRequestResyncBusEvents.store(true, std::memory_order_release);
			last_data_time = std::chrono::steady_clock::now();
			bWatchdogAnnounced = false;
		}

		// full stream resync when necessary
		if (bRequestResyncBusEvents.load(std::memory_order_acquire)) {
			if (!tini_resync_stream()) {
				// Leave the request set: the reconnect path requests a
				// resync again once the device is reopened
				tini_close();
				continue;
			}
			bRequestResyncBusEvents.store(false, std::memory_order_release);
			last_data_time = std::chrono::steady_clock::now();
		}

		// enable bus events when necessary
		if (bRequestEnableBusEvents.load(std::memory_order_acquire)) {
			std::cerr << "Enabling Appletini bus events... ";
			uint32_t enable_msg_buf[3];
			enable_msg_buf[0] = 0x00000001; // 1 data field
			enable_msg_buf[1] = 0x00001000; // address of bus_event_control
			enable_msg_buf[2] = 0x00000001; // bit 0 indicates enable bus events
			if (!tini_write((uint8_t *)enable_msg_buf, 12)) {
				// A failed 12-byte control write means the device is gone
				// or wedged: drop the handle and reconnect
				std::cerr << "failed!" << std::endl;
				tini_close();
				continue;
			} else {
				std::cerr << "done!" << std::endl;
				// Only clear the request after the FPGA has received it.  The
				// old code silently disabled retries after a failed macOS write.
				bRequestEnableBusEvents.store(false, std::memory_order_release);
			}
		}

		auto packet = packetFreeQueue.pop();
		int got = 0;
		int r = libusb_bulk_transfer(g_tiniDev, TINI_PIPE_READ,
			packet->data, PKT_BUFSZ, &got, 1000);
		packet->size = (uint32_t)got;

		if ((r == LIBUSB_ERROR_TIMEOUT) && (got == 0)) {
			// The Apple is off / no events flowing. Stay connected.  A
			// timeout with bytes transferred is a valid partial libusb read
			// and is processed below.
			ftStatus = TINI_ERR_TIMEOUT;
			// Watchdog: the Apple bus generates events on every cycle
			// when the machine is running, so prolonged silence is either
			// "Apple off" (a resync is harmless) or a wedged stream
			// (a resync recovers it). Resync periodically until data flows.
			auto now = std::chrono::steady_clock::now();
			if ((now - last_data_time > std::chrono::seconds(TINI_WATCHDOG_SECONDS))
				&& (now >= next_watchdog_resync))
			{
				if (!bWatchdogAnnounced)
				{
					std::cerr << "No Appletini bus events, resyncing stream" << std::endl;
					bWatchdogAnnounced = true;
				}
				bRequestResyncBusEvents.store(true, std::memory_order_release);
				next_watchdog_resync = now + std::chrono::seconds(TINI_WATCHDOG_SECONDS);
			}
			packetFreeQueue.push(std::move(packet));
			continue;
		}
		if ((r != LIBUSB_SUCCESS) && (r != LIBUSB_ERROR_TIMEOUT)) {
			// Do not turn an overflow, stall, or other transport error into
			// an apparent OK merely because libusb returned a partial count.
			// For non-timeout errors that count is not guaranteed reliable.
			ftStatus = TINI_ERR_IO;
			std::cerr << "Appletini read failed (" << libusb_error_name(r)
				  << ", received " << got << "), reconnecting" << std::endl;
			ftStatusPrevious = ftStatus;
			tini_close();
			packetFreeQueue.push(std::move(packet));
			continue;
		}
		ftStatus = TINI_ERR_OK;
		packet->generation = streamGeneration.load(std::memory_order_acquire);
		last_data_time = std::chrono::steady_clock::now();
		bWatchdogAnnounced = false;

		if (!eventRecorder->IsInReplayMode() && packet->size > 0)
		{
			packetInQueue.push(std::move(packet));
		}
		else
		{
			packetFreeQueue.push(std::move(packet));
		}
	}
	std::cout << "ending usb read loop" << std::endl;
	tini_close();
	return 0;
}

#endif /* __NETWORKING_WINDOWS__ */

uint32_t usb_write_register(uint32_t addressStart, const std::vector<uint32_t>* vData, bool setIncrement)
{
	if (!bIsConnected)
		return 0;
	uint32_t msg_buf[256];	// max 256 entries, 254 data fields
	uint32_t vDataSize = (uint32_t)vData->size();
	if (vDataSize > ((sizeof(msg_buf) / sizeof(msg_buf[0])) - 2))
	{
		std::cerr << "ERROR: Too much data sent to usb_write_register!" << std::endl;
		return 0;
	}
	msg_buf[0] = 0x0;
	if (setIncrement)
		msg_buf[0] = 0x80000000;
	msg_buf[0] += vDataSize;
	msg_buf[1] = addressStart;
	for (size_t i = 0; i < vDataSize; ++i)
	{
		msg_buf[2 + i] = vData->at(i);
	}
	uint32_t msg_buf_len = (2 + vDataSize) * (uint32_t)sizeof(msg_buf[0]);
	if (!tini_write((uint8_t *)msg_buf, msg_buf_len))
	{
		ftStatus = TINI_ERR_IO;
		if (ftStatus != ftStatusPrevious)
			std::cerr << "Failed to write to Appletini" << std::endl;
		ftStatusPrevious = ftStatus;
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
			if (ImGui::Button("Resync Bus Stream"))
				bRequestResyncBusEvents.store(true, std::memory_order_release);
			ImGui::SetItemTooltip("Disable, drain and re-enable the Appletini bus event stream. Use if the display stops updating.");
			ImGui::Separator();
			ImGui::Checkbox("Increment", &bUSBImGUiIsIncrement);
			// Only writing to RAM, not registers
			ImGui::DragInt("Apple RAM Address", &iUSBImGUIAddressStart, 1.f, 0, 0xFFFF, "%04X");
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
				// now send to appletini (RAM starts at 0x10000 above the register space
				auto _res = usb_write_register(0x10000 + iUSBImGUIAddressStart, &result, bUSBImGUiIsIncrement);
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
