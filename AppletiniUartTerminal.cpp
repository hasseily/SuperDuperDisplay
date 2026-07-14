#include "AppletiniUartTerminal.h"

#include "imgui.h"
#include "imgui_internal.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#if defined(_WIN32) || defined(_WIN64)
#define APPLETINI_UART_WINDOWS
#elif defined(__APPLE__) || defined(__linux__)
#define APPLETINI_UART_POSIX
#endif

#if defined(APPLETINI_UART_WINDOWS)
#include <windows.h>
#include <devguid.h>
#include <setupapi.h>
#pragma comment(lib, "setupapi.lib")
#elif defined(APPLETINI_UART_POSIX)
#include <dirent.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <cerrno>
#include <fstream>
#if defined(__APPLE__)
#include <IOKit/serial/ioss.h>
#endif
#endif

namespace {

constexpr int APPLETINI_UART_BAUD = 921600;
constexpr size_t UART_LOG_LIMIT = 1024 * 1024;
constexpr size_t COMMAND_HISTORY_LIMIT = 100;

struct SerialPortInfo {
	std::string port_name;
	std::string friendly_name;
	std::string hardware_id;
	bool appletini_candidate = false;
	int port_number = 0;
};

static std::string to_lower_ascii(std::string s)
{
	for (char& c : s)
		c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	return s;
}

static bool contains_ascii_ci(const std::string& haystack, const char* needle)
{
	return to_lower_ascii(haystack).find(to_lower_ascii(needle)) != std::string::npos;
}

static void sort_ports(std::vector<SerialPortInfo>& ports)
{
	std::sort(ports.begin(), ports.end(), [](const SerialPortInfo& a, const SerialPortInfo& b) {
		if (a.appletini_candidate != b.appletini_candidate)
			return a.appletini_candidate > b.appletini_candidate;
		if (a.port_number != b.port_number)
			return a.port_number < b.port_number;
		return a.port_name < b.port_name;
	});
}

#if defined(APPLETINI_UART_WINDOWS)
static int serial_port_number(const std::string& port_name)
{
	if (port_name.size() < 4)
		return 0;
	if ((port_name[0] != 'C') && (port_name[0] != 'c'))
		return 0;
	if ((port_name[1] != 'O') && (port_name[1] != 'o'))
		return 0;
	if ((port_name[2] != 'M') && (port_name[2] != 'm'))
		return 0;
	return std::atoi(port_name.c_str() + 3);
}

static std::string format_win32_error(DWORD err)
{
	char* msg = nullptr;
	DWORD len = FormatMessageA(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr, err, 0, reinterpret_cast<LPSTR>(&msg), 0, nullptr);
	std::string result;
	if (len > 0 && msg != nullptr) {
		result.assign(msg, len);
		while (!result.empty() && (result.back() == '\r' || result.back() == '\n' || result.back() == '.'))
			result.pop_back();
		LocalFree(msg);
	} else {
		char buf[64];
		snprintf(buf, sizeof(buf), "Win32 error %lu", static_cast<unsigned long>(err));
		result = buf;
	}
	return result;
}

static std::string read_device_property(HDEVINFO devs, SP_DEVINFO_DATA* dev_info, DWORD property)
{
	char buf[2048] = {};
	DWORD type = 0;
	DWORD required = 0;
	if (!SetupDiGetDeviceRegistryPropertyA(devs, dev_info, property, &type,
			reinterpret_cast<PBYTE>(buf), sizeof(buf), &required)) {
		return {};
	}
	if (type == REG_MULTI_SZ) {
		std::string result;
		const char* p = buf;
		while (*p != '\0') {
			if (!result.empty())
				result += " ";
			result += p;
			p += std::strlen(p) + 1;
		}
		return result;
	}
	return std::string(buf);
}

static std::string read_port_name(HDEVINFO devs, SP_DEVINFO_DATA* dev_info)
{
	HKEY key = SetupDiOpenDevRegKey(devs, dev_info, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
	if (key == INVALID_HANDLE_VALUE)
		return {};

	char buf[128] = {};
	DWORD type = 0;
	DWORD size = sizeof(buf);
	std::string port_name;
	if (RegQueryValueExA(key, "PortName", nullptr, &type, reinterpret_cast<LPBYTE>(buf), &size) == ERROR_SUCCESS &&
		type == REG_SZ) {
		port_name = buf;
	}
	RegCloseKey(key);
	return port_name;
}

static std::vector<SerialPortInfo> enumerate_serial_ports()
{
	std::vector<SerialPortInfo> ports;
	HDEVINFO devs = SetupDiGetClassDevsA(&GUID_DEVCLASS_PORTS, nullptr, nullptr, DIGCF_PRESENT);
	if (devs == INVALID_HANDLE_VALUE)
		return ports;

	for (DWORD i = 0;; ++i) {
		SP_DEVINFO_DATA dev_info;
		dev_info.cbSize = sizeof(dev_info);
		if (!SetupDiEnumDeviceInfo(devs, i, &dev_info))
			break;

		SerialPortInfo port;
		port.port_name = read_port_name(devs, &dev_info);
		if (port.port_name.empty() || port.port_name.rfind("COM", 0) != 0)
			continue;

		port.friendly_name = read_device_property(devs, &dev_info, SPDRP_FRIENDLYNAME);
		if (port.friendly_name.empty())
			port.friendly_name = read_device_property(devs, &dev_info, SPDRP_DEVICEDESC);
		port.hardware_id = read_device_property(devs, &dev_info, SPDRP_HARDWAREID);
		port.port_number = serial_port_number(port.port_name);

		const bool is_cp2105 =
			contains_ascii_ci(port.hardware_id, "VID_10C4") &&
			contains_ascii_ci(port.hardware_id, "PID_EA70");
		const bool name_matches =
			contains_ascii_ci(port.friendly_name, "CP2105") ||
			contains_ascii_ci(port.friendly_name, "Appletini");
		port.appletini_candidate = is_cp2105 || name_matches;
		ports.push_back(port);
	}
	SetupDiDestroyDeviceInfoList(devs);

	sort_ports(ports);
	return ports;
}
#elif defined(__linux__)
static std::string errno_string()
{
	return std::string(strerror(errno));
}

static std::string read_text_file(const std::string& path)
{
	std::ifstream f(path);
	std::string s;
	if (f.is_open())
		std::getline(f, s);
	return s;
}

static bool sysfs_is_cp2105(const std::string& tty_name)
{
	// The USB device directory (holding idVendor/idProduct) sits one or two
	// levels above the interface directory /sys/class/tty/<tty>/device links to.
	for (const char* up : { "/device/..", "/device/../.." }) {
		const std::string base = "/sys/class/tty/" + tty_name + up;
		const std::string vendor = read_text_file(base + "/idVendor");
		if (vendor.empty())
			continue;
		return contains_ascii_ci(vendor, "10c4") &&
			contains_ascii_ci(read_text_file(base + "/idProduct"), "ea70");
	}
	return false;
}

static std::vector<SerialPortInfo> enumerate_serial_ports()
{
	std::vector<SerialPortInfo> ports;

	// /dev/serial/by-id encodes the USB product name in the link name.
	if (DIR* dir = opendir("/dev/serial/by-id")) {
		while (dirent* entry = readdir(dir)) {
			if (entry->d_name[0] == '.')
				continue;
			const std::string link = std::string("/dev/serial/by-id/") + entry->d_name;
			char* resolved = realpath(link.c_str(), nullptr);
			if (resolved == nullptr)
				continue;
			SerialPortInfo port;
			port.port_name = resolved;
			free(resolved);
			port.friendly_name = entry->d_name;
			port.hardware_id = entry->d_name;
			port.appletini_candidate =
				contains_ascii_ci(port.friendly_name, "CP2105") ||
				contains_ascii_ci(port.friendly_name, "Appletini");
			ports.push_back(port);
		}
		closedir(dir);
	}

	// Pick up USB serial devices that have no by-id entry.
	if (DIR* dir = opendir("/dev")) {
		while (dirent* entry = readdir(dir)) {
			const std::string name = entry->d_name;
			if (name.rfind("ttyUSB", 0) != 0 && name.rfind("ttyACM", 0) != 0)
				continue;
			const std::string path = "/dev/" + name;
			bool known = false;
			for (const SerialPortInfo& existing : ports)
				known = known || (existing.port_name == path);
			if (known)
				continue;
			SerialPortInfo port;
			port.port_name = path;
			port.friendly_name = name;
			port.appletini_candidate = sysfs_is_cp2105(name);
			ports.push_back(port);
		}
		closedir(dir);
	}

	sort_ports(ports);
	return ports;
}
#elif defined(__APPLE__)
static std::string errno_string()
{
	return std::string(strerror(errno));
}

static std::vector<SerialPortInfo> enumerate_serial_ports()
{
	std::vector<SerialPortInfo> ports;

	// Use the call-out (cu.*) devices, which open without waiting for carrier.
	if (DIR* dir = opendir("/dev")) {
		while (dirent* entry = readdir(dir)) {
			const std::string name = entry->d_name;
			if (name.rfind("cu.", 0) != 0)
				continue;
			SerialPortInfo port;
			port.port_name = "/dev/" + name;
			port.friendly_name = name;
			port.appletini_candidate =
				contains_ascii_ci(name, "CP2105") ||
				contains_ascii_ci(name, "SLAB") ||
				contains_ascii_ci(name, "usbserial") ||
				contains_ascii_ci(name, "Appletini");
			ports.push_back(port);
		}
		closedir(dir);
	}

	sort_ports(ports);
	return ports;
}
#else
static std::vector<SerialPortInfo> enumerate_serial_ports()
{
	return {};
}
#endif

class SerialTerminal {
public:
	~SerialTerminal()
	{
		close();
	}

	bool open(const SerialPortInfo& port, bool writable, int baud)
	{
		close();
		clear();
		port_name_ = port.port_name;
		writable_ = writable;

#if defined(APPLETINI_UART_WINDOWS)
		std::string path = "\\\\.\\" + port.port_name;
		DWORD access = GENERIC_READ | (writable ? GENERIC_WRITE : 0);
		HANDLE h = CreateFileA(path.c_str(), access, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE) {
			append_host_line("open failed: " + format_win32_error(GetLastError()));
			return false;
		}

		SetupComm(h, 8192, 8192);
		DCB dcb = {};
		dcb.DCBlength = sizeof(dcb);
		if (!GetCommState(h, &dcb)) {
			append_host_line("GetCommState failed: " + format_win32_error(GetLastError()));
			CloseHandle(h);
			return false;
		}
		dcb.BaudRate = static_cast<DWORD>(baud);
		dcb.ByteSize = 8;
		dcb.Parity = NOPARITY;
		dcb.StopBits = ONESTOPBIT;
		dcb.fBinary = TRUE;
		dcb.fParity = FALSE;
		dcb.fDtrControl = DTR_CONTROL_ENABLE;
		dcb.fRtsControl = RTS_CONTROL_ENABLE;
		if (!SetCommState(h, &dcb)) {
			append_host_line("SetCommState failed: " + format_win32_error(GetLastError()));
			CloseHandle(h);
			return false;
		}

		COMMTIMEOUTS timeouts = {};
		timeouts.ReadIntervalTimeout = 50;
		timeouts.ReadTotalTimeoutConstant = 50;
		timeouts.ReadTotalTimeoutMultiplier = 0;
		timeouts.WriteTotalTimeoutConstant = 1000;
		timeouts.WriteTotalTimeoutMultiplier = 0;
		SetCommTimeouts(h, &timeouts);
		PurgeComm(h, PURGE_RXCLEAR | PURGE_TXCLEAR);

		{
			std::lock_guard<std::mutex> lock(handle_mutex_);
			handle_ = h;
		}
		running_.store(true, std::memory_order_release);
		connected_.store(true, std::memory_order_release);
		append_host_line("opened " + port.port_name + " at " + std::to_string(baud) + " 8N1");
		reader_ = std::thread(&SerialTerminal::reader_loop, this);
		return true;
#elif defined(APPLETINI_UART_POSIX)
		int fd = ::open(port.port_name.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
		if (fd < 0) {
			append_host_line("open failed: " + errno_string());
			return false;
		}
		ioctl(fd, TIOCEXCL);
		fcntl(fd, F_SETFL, 0);	// back to blocking IO; reads time out via VMIN/VTIME

		termios tio = {};
		if (tcgetattr(fd, &tio) != 0) {
			append_host_line("tcgetattr failed: " + errno_string());
			::close(fd);
			return false;
		}
		cfmakeraw(&tio);
		tio.c_cflag |= CLOCAL | CREAD;
		tio.c_cflag &= ~(PARENB | CSTOPB | CSIZE);
		tio.c_cflag |= CS8;
#if defined(CRTSCTS)
		tio.c_cflag &= ~CRTSCTS;
#endif
		tio.c_cc[VMIN] = 0;
		tio.c_cc[VTIME] = 1;	// 100ms read timeout so close() can stop the reader

#if defined(__APPLE__)
		// Try the requested speed directly; fall back to IOSSIOSPEED for
		// drivers that reject non-standard termios speeds.
		cfsetspeed(&tio, static_cast<speed_t>(baud));
		if (tcsetattr(fd, TCSANOW, &tio) != 0) {
			cfsetspeed(&tio, B230400);
			speed_t custom = static_cast<speed_t>(baud);
			if (tcsetattr(fd, TCSANOW, &tio) != 0 || ioctl(fd, IOSSIOSPEED, &custom) == -1) {
				append_host_line("failed to set baud rate: " + errno_string());
				::close(fd);
				return false;
			}
		}
#else
		speed_t speed = B115200;
		switch (baud) {
			case 230400: speed = B230400; break;
			case 460800: speed = B460800; break;
			case 921600: speed = B921600; break;
			default: break;
		}
		cfsetispeed(&tio, speed);
		cfsetospeed(&tio, speed);
		if (tcsetattr(fd, TCSANOW, &tio) != 0) {
			append_host_line("tcsetattr failed: " + errno_string());
			::close(fd);
			return false;
		}
#endif
		tcflush(fd, TCIOFLUSH);

		{
			std::lock_guard<std::mutex> lock(handle_mutex_);
			fd_ = fd;
		}
		running_.store(true, std::memory_order_release);
		connected_.store(true, std::memory_order_release);
		append_host_line("opened " + port.port_name + " at " + std::to_string(baud) + " 8N1");
		reader_ = std::thread(&SerialTerminal::reader_loop, this);
		return true;
#else
		(void)port;
		(void)baud;
		append_host_line("serial UARTs are not implemented on this platform");
		return false;
#endif
	}

	void close()
	{
#if defined(APPLETINI_UART_WINDOWS)
		running_.store(false, std::memory_order_release);
		{
			std::lock_guard<std::mutex> lock(handle_mutex_);
			if (handle_ != INVALID_HANDLE_VALUE)
				CancelIo(handle_);
		}
		if (reader_.joinable())
			reader_.join();
		{
			std::lock_guard<std::mutex> lock(handle_mutex_);
			if (handle_ != INVALID_HANDLE_VALUE) {
				CloseHandle(handle_);
				handle_ = INVALID_HANDLE_VALUE;
			}
		}
#elif defined(APPLETINI_UART_POSIX)
		running_.store(false, std::memory_order_release);
		// The reader wakes within the 100ms VTIME timeout and sees running_ off.
		if (reader_.joinable())
			reader_.join();
		{
			std::lock_guard<std::mutex> lock(handle_mutex_);
			if (fd_ >= 0) {
				::close(fd_);
				fd_ = -1;
			}
		}
#endif
		connected_.store(false, std::memory_order_release);
	}

	bool write_line(const std::string& line)
	{
		std::string payload = line;
		payload += "\r";
		return write_bytes(payload.data(), payload.size());
	}

	bool is_open() const
	{
		return connected_.load(std::memory_order_acquire);
	}

	bool can_write() const
	{
		return writable_ && is_open();
	}

	std::string port_name() const
	{
		return port_name_;
	}

	std::string snapshot_text() const
	{
		std::lock_guard<std::mutex> lock(log_mutex_);
		return log_;
	}

	void clear()
	{
		std::lock_guard<std::mutex> lock(log_mutex_);
		log_.clear();
		last_was_cr_ = false;
	}

private:
	bool write_bytes(const char* data, size_t len)
	{
#if defined(APPLETINI_UART_WINDOWS)
		if (!can_write())
			return false;
		HANDLE h = INVALID_HANDLE_VALUE;
		{
			std::lock_guard<std::mutex> lock(handle_mutex_);
			h = handle_;
		}
		if (h == INVALID_HANDLE_VALUE)
			return false;

		DWORD written = 0;
		if (!WriteFile(h, data, static_cast<DWORD>(len), &written, nullptr)) {
			append_host_line("write failed: " + format_win32_error(GetLastError()));
			return false;
		}
		if (written != len) {
			append_host_line("short write");
			return false;
		}
		FlushFileBuffers(h);
		return true;
#elif defined(APPLETINI_UART_POSIX)
		if (!can_write())
			return false;
		int fd = -1;
		{
			std::lock_guard<std::mutex> lock(handle_mutex_);
			fd = fd_;
		}
		if (fd < 0)
			return false;

		size_t sent = 0;
		while (sent < len) {
			const ssize_t n = ::write(fd, data + sent, len - sent);
			if (n < 0) {
				if (errno == EINTR)
					continue;
				append_host_line("write failed: " + errno_string());
				return false;
			}
			sent += static_cast<size_t>(n);
		}
		tcdrain(fd);
		return true;
#else
		(void)data;
		(void)len;
		return false;
#endif
	}

	void reader_loop()
	{
#if defined(APPLETINI_UART_WINDOWS)
		char buf[512];
		while (running_.load(std::memory_order_acquire)) {
			HANDLE h = INVALID_HANDLE_VALUE;
			{
				std::lock_guard<std::mutex> lock(handle_mutex_);
				h = handle_;
			}
			if (h == INVALID_HANDLE_VALUE)
				break;

			DWORD got = 0;
			if (!ReadFile(h, buf, sizeof(buf), &got, nullptr)) {
				if (running_.load(std::memory_order_acquire))
					append_host_line("read failed: " + format_win32_error(GetLastError()));
				break;
			}
			if (got > 0)
				append_bytes(buf, got);
		}
		connected_.store(false, std::memory_order_release);
#elif defined(APPLETINI_UART_POSIX)
		char buf[512];
		while (running_.load(std::memory_order_acquire)) {
			int fd = -1;
			{
				std::lock_guard<std::mutex> lock(handle_mutex_);
				fd = fd_;
			}
			if (fd < 0)
				break;

			const ssize_t got = ::read(fd, buf, sizeof(buf));
			if (got < 0) {
				if (errno == EINTR || errno == EAGAIN)
					continue;
				if (running_.load(std::memory_order_acquire))
					append_host_line("read failed: " + errno_string());
				break;
			}
			if (got > 0)
				append_bytes(buf, static_cast<size_t>(got));
		}
		connected_.store(false, std::memory_order_release);
#endif
	}

	void append_host_line(const std::string& s)
	{
		std::string line = "[host] " + s + "\n";
		append_bytes(line.data(), line.size());
	}

	void append_bytes(const char* data, size_t len)
	{
		std::lock_guard<std::mutex> lock(log_mutex_);
		for (size_t i = 0; i < len; ++i) {
			const unsigned char c = static_cast<unsigned char>(data[i]);
			if (c == '\r') {
				log_.push_back('\n');
				last_was_cr_ = true;
			} else if (c == '\n') {
				if (!last_was_cr_)
					log_.push_back('\n');
				last_was_cr_ = false;
			} else if (c == '\t' || (c >= 0x20 && c <= 0x7e)) {
				log_.push_back(static_cast<char>(c));
				last_was_cr_ = false;
			} else {
				char escaped[5];
				snprintf(escaped, sizeof(escaped), "\\x%02X", c);
				log_ += escaped;
				last_was_cr_ = false;
			}
		}

		if (log_.size() > UART_LOG_LIMIT) {
			size_t erase_len = log_.size() - UART_LOG_LIMIT;
			size_t newline = log_.find('\n', erase_len);
			if (newline != std::string::npos && newline < erase_len + 4096)
				erase_len = newline + 1;
			log_.erase(0, erase_len);
		}
	}

#if defined(APPLETINI_UART_WINDOWS)
	HANDLE handle_ = INVALID_HANDLE_VALUE;
#elif defined(APPLETINI_UART_POSIX)
	int fd_ = -1;
#endif
	std::thread reader_;
	std::atomic<bool> running_ = false;
	std::atomic<bool> connected_ = false;
	bool writable_ = false;
	std::string port_name_;
	mutable std::mutex handle_mutex_;
	mutable std::mutex log_mutex_;
	std::string log_;
	bool last_was_cr_ = false;
};

class AppletiniUartWindow {
public:
	nlohmann::json SerializeState()
	{
		return {
			{"command input height", command_input_height_},
			{"auto scroll", auto_scroll_},
		};
	}

	void DeserializeState(const nlohmann::json& jsonState)
	{
		command_input_height_ = jsonState.value("command input height", command_input_height_);
		auto_scroll_ = jsonState.value("auto scroll", auto_scroll_);
		if (command_input_height_ < 0.0f)
			command_input_height_ = 0.0f;
	}

	void draw(bool* p_open)
	{
		if (!initialized_) {
			refresh_ports();
			initialized_ = true;
		}

		ImGui::SetNextWindowSizeConstraints(ImVec2(640, 420), ImVec2(FLT_MAX, FLT_MAX));
		if (!ImGui::Begin("Appletini UARTs", p_open)) {
			ImGui::End();
			return;
		}

		draw_toolbar();
		ImGui::Separator();

		if (ImGui::BeginTabBar("##uart_tabs")) {
			if (ImGui::BeginTabItem("Control UART")) {
				draw_terminal("##control_log", control_, true);
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("Debug UART")) {
				draw_terminal("##debug_log", debug_, false);
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}

		ImGui::End();
	}

private:
	void refresh_ports()
	{
		ports_ = enumerate_serial_ports();
		control_port_idx_ = -1;
		debug_port_idx_ = -1;
		for (int i = 0; i < static_cast<int>(ports_.size()); ++i) {
			if (control_port_idx_ < 0) {
				control_port_idx_ = i;
			} else if (debug_port_idx_ < 0) {
				debug_port_idx_ = i;
				break;
			}
		}
	}

	const char* selected_port_label(int idx) const
	{
		static std::string label;
		if (idx < 0 || idx >= static_cast<int>(ports_.size())) {
			label = "No port";
			return label.c_str();
		}
		label = ports_[idx].port_name;
		if (!ports_[idx].friendly_name.empty()) {
			label += " - ";
			label += ports_[idx].friendly_name;
		}
		return label.c_str();
	}

	void draw_port_combo(const char* label, int* idx)
	{
		ImGui::SetNextItemWidth(260.0f);
		if (ImGui::BeginCombo(label, selected_port_label(*idx))) {
			for (int i = 0; i < static_cast<int>(ports_.size()); ++i) {
				std::string item = ports_[i].port_name;
				if (!ports_[i].friendly_name.empty()) {
					item += " - ";
					item += ports_[i].friendly_name;
				}
				if (ports_[i].appletini_candidate)
					item += " [Appletini]";
				if (ImGui::Selectable(item.c_str(), i == *idx))
					*idx = i;
			}
			ImGui::EndCombo();
		}
	}

	void draw_toolbar()
	{
		draw_port_combo("Control", &control_port_idx_);
		ImGui::SameLine();
		draw_port_combo("Debug", &debug_port_idx_);

		if (ImGui::Button("Refresh"))
			refresh_ports();
		ImGui::SameLine();
		const bool any_open = control_.is_open() || debug_.is_open();
		if (!any_open) {
			if (ImGui::Button("Open")) {
				open_selected();
			}
		} else {
			if (ImGui::Button("Close")) {
				control_.close();
				debug_.close();
			}
		}
		ImGui::SameLine();
		ImGui::Checkbox("Auto-scroll", &auto_scroll_);

		if (!status_.empty()) {
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.9f, 0.2f, 0.2f, 1.0f), "%s", status_.c_str());
		}
	}

	void open_selected()
	{
		status_.clear();
		if (control_port_idx_ < 0 || debug_port_idx_ < 0 ||
			control_port_idx_ >= static_cast<int>(ports_.size()) ||
			debug_port_idx_ >= static_cast<int>(ports_.size())) {
			status_ = "select both UART ports";
			return;
		}
		if (control_port_idx_ == debug_port_idx_) {
			status_ = "control/debug ports must differ";
			return;
		}
		bool control_ok = control_.open(ports_[control_port_idx_], true, APPLETINI_UART_BAUD);
		bool debug_ok = debug_.open(ports_[debug_port_idx_], false, APPLETINI_UART_BAUD);
		if (!control_ok || !debug_ok) {
			status_ = "one or more UARTs failed to open";
		} else {
			focus_command_next_frame_ = true;
		}
	}

	void add_command_history(const std::string& line)
	{
		if (line.empty())
			return;
		if (!command_history_.empty() && command_history_.back() == line)
			return;
		command_history_.push_back(line);
		if (command_history_.size() > COMMAND_HISTORY_LIMIT)
			command_history_.erase(command_history_.begin());
	}

	void reset_command_history_navigation()
	{
		command_history_index_ = -1;
		command_history_draft_.clear();
	}

	void replace_command_input(ImGuiInputTextCallbackData* data, const std::string& text)
	{
		data->DeleteChars(0, data->BufTextLen);
		data->InsertChars(0, text.c_str());
		data->CursorPos = data->BufTextLen;
		data->SelectionStart = data->BufTextLen;
		data->SelectionEnd = data->BufTextLen;
	}

	bool navigate_command_history(ImGuiInputTextCallbackData* data)
	{
		ImGuiKey key = ImGuiKey_None;
		if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))
			key = ImGuiKey_UpArrow;
		else if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))
			key = ImGuiKey_DownArrow;
		else
			return false;

		if (command_history_.empty())
			return false;

		if (key == ImGuiKey_UpArrow) {
			if (command_history_index_ < 0) {
				command_history_draft_.assign(data->Buf, data->BufTextLen);
				command_history_index_ = static_cast<int>(command_history_.size()) - 1;
			} else if (command_history_index_ > 0) {
				--command_history_index_;
			}
			replace_command_input(data, command_history_[static_cast<size_t>(command_history_index_)]);
			return true;
		} else if (key == ImGuiKey_DownArrow && command_history_index_ >= 0) {
			if (command_history_index_ + 1 < static_cast<int>(command_history_.size())) {
				++command_history_index_;
				replace_command_input(data, command_history_[static_cast<size_t>(command_history_index_)]);
			} else {
				replace_command_input(data, command_history_draft_);
				reset_command_history_navigation();
			}
			return true;
		}
		return false;
	}

	bool send_command_line(SerialTerminal& terminal, const std::string& line)
	{
		if (!terminal.write_line(line)) {
			status_ = "control UART write failed";
			return false;
		}
		add_command_history(line);
		reset_command_history_navigation();
		focus_command_next_frame_ = true;
		return true;
	}

	// Sends each line of `input` as its own command; returns the unsent tail
	// (empty on full success) so a failed write keeps the remaining text.
	std::string send_lines(SerialTerminal& terminal, const std::string& input)
	{
		size_t line_start = 0;
		while (true) {
			const size_t eol = input.find_first_of("\r\n", line_start);
			const size_t line_end = (eol == std::string::npos) ? input.size() : eol;
			if (!send_command_line(terminal, input.substr(line_start, line_end - line_start)))
				return input.substr(line_start);
			if (eol == std::string::npos)
				break;
			line_start = eol + 1;
			if (input[eol] == '\r' && line_start < input.size() && input[line_start] == '\n')
				++line_start;
			if (line_start >= input.size())
				break;
		}
		return {};
	}

	bool send_completed_command_lines(SerialTerminal& terminal, ImGuiInputTextCallbackData* data)
	{
		std::string input(data->Buf, data->BufTextLen);
		size_t line_start = 0;
		size_t scan = 0;
		bool sent_any = false;

		while (scan < input.size()) {
			if (input[scan] != '\r' && input[scan] != '\n') {
				++scan;
				continue;
			}

			if (!send_command_line(terminal, input.substr(line_start, scan - line_start)))
				break;
			sent_any = true;

			const char delimiter = input[scan++];
			if (delimiter == '\r' && scan < input.size() && input[scan] == '\n')
				++scan;
			line_start = scan;
		}

		if (sent_any) {
			const std::string remaining = input.substr(line_start);
			const size_t max_copy_len = static_cast<size_t>(data->BufSize - 1);
			const size_t copy_len = (remaining.size() < max_copy_len) ? remaining.size() : max_copy_len;
			std::memmove(data->Buf, remaining.data(), copy_len);
			data->Buf[copy_len] = '\0';
			data->BufTextLen = static_cast<int>(copy_len);
			data->CursorPos = static_cast<int>(copy_len);
			data->SelectionStart = static_cast<int>(copy_len);
			data->SelectionEnd = static_cast<int>(copy_len);
			data->BufDirty = true;
		}
		return sent_any;
	}

	void process_command_input(SerialTerminal& terminal, ImGuiInputTextCallbackData* data)
	{
		const int prev_len = command_last_len_;
		command_last_len_ = data->BufTextLen;

		if (navigate_command_history(data)) {
			command_last_len_ = data->BufTextLen;
			return;
		}

		if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)) {
			// Shift+Enter keeps the newline the widget inserted, to compose a
			// multi-line command.
			if (ImGui::GetIO().KeyShift)
				return;

			// Plain Enter: drop the newline just inserted at the caret so a
			// mid-line caret doesn't split the command, then send every line.
			const int newline_pos = data->CursorPos - 1;
			if (newline_pos >= 0 && newline_pos < data->BufTextLen && data->Buf[newline_pos] == '\n')
				data->DeleteChars(newline_pos, 1);
			const std::string unsent = send_lines(terminal, std::string(data->Buf, data->BufTextLen));
			data->DeleteChars(0, data->BufTextLen);
			if (!unsent.empty())
				data->InsertChars(0, unsent.c_str());
			command_last_len_ = data->BufTextLen;
			return;
		}

		// A jump of more than one character is a paste; send its completed
		// lines immediately, terminal-style. Single keystrokes wait for Enter.
		if (data->BufTextLen > prev_len + 1)
			send_completed_command_lines(terminal, data);
		command_last_len_ = data->BufTextLen;
	}

	struct CommandInputCallbackContext {
		AppletiniUartWindow* window = nullptr;
		SerialTerminal* terminal = nullptr;
	};

	static int command_input_callback(ImGuiInputTextCallbackData* data)
	{
		auto* ctx = static_cast<CommandInputCallbackContext*>(data->UserData);
		if (ctx != nullptr && ctx->window != nullptr && ctx->terminal != nullptr)
			ctx->window->process_command_input(*ctx->terminal, data);
		return 0;
	}

	bool send_command_buffer(SerialTerminal& terminal)
	{
		if (command_buf_[0] == '\0')
			return false;
		const std::string unsent = send_lines(terminal, command_buf_);
		const size_t copy_len = (unsent.size() < sizeof(command_buf_) - 1) ? unsent.size() : sizeof(command_buf_) - 1;
		std::memmove(command_buf_, unsent.data(), copy_len);
		command_buf_[copy_len] = '\0';
		return unsent.empty();
	}

	void draw_terminal(const char* id, SerialTerminal& terminal, bool writable)
	{
		std::string text = terminal.snapshot_text();

		if (ImGui::Button("Copy"))
			ImGui::SetClipboardText(text.c_str());
		ImGui::SameLine();
		if (ImGui::Button("Clear"))
			terminal.clear();
		ImGui::SameLine();
		const char* state = terminal.is_open() ? "open" : "closed";
		ImGui::Text("%s %s", state, terminal.port_name().c_str());

		const ImGuiStyle& style = ImGui::GetStyle();
		const float splitter_height = 6.0f;
		if (command_input_height_ <= 0.0f)
			command_input_height_ = ImGui::GetFrameHeight();

		const float footer_height = writable
			? splitter_height + command_input_height_ + style.ItemSpacing.y * 2.0f
			: 0.0f;
		float log_height = ImGui::GetContentRegionAvail().y - footer_height;
		const float min_log_height = ImGui::GetTextLineHeightWithSpacing() * 6.0f;
		if (log_height < min_log_height)
			log_height = min_log_height;

		std::vector<char> text_buf(text.begin(), text.end());
		text_buf.push_back('\0');
		ImGui::InputTextMultiline(
			id,
			text_buf.data(),
			text_buf.size(),
			ImVec2(-FLT_MIN, log_height),
			ImGuiInputTextFlags_ReadOnly);

		// While the scrollbar sits at the bottom, keep the log pinned there as
		// new output arrives. Leave it alone when the user has scrolled up or
		// is selecting text in the log. The log lives in the child window that
		// InputTextMultiline creates, named "<parent>/<label>_<id>".
		const ImGuiID log_id = ImGui::GetID(id);
		char log_child_name[256];
		snprintf(log_child_name, sizeof(log_child_name), "%s/%s_%08X",
			ImGui::GetCurrentWindow()->Name, id, static_cast<unsigned int>(log_id));
		ImGuiWindow* log_child = ImGui::FindWindowByName(log_child_name);
		if (log_child != nullptr && auto_scroll_ && ImGui::GetActiveID() != log_id &&
			log_child->Scroll.y >= log_child->ScrollMax.y - 1.0f) {
			ImGui::SetScrollY(log_child, 1.0e9f);
		}

		if (!writable)
			return;

		ImGui::InvisibleButton("##command_splitter",
			ImVec2(ImMax(ImGui::GetContentRegionAvail().x, 1.0f), splitter_height));
		const bool splitter_active = ImGui::IsItemActive();
		const bool splitter_hovered = ImGui::IsItemHovered();
		if (splitter_active)
			command_input_height_ -= ImGui::GetIO().MouseDelta.y;
		const float max_input_height = ImMax(ImGui::GetWindowHeight() * 0.5f, ImGui::GetFrameHeight());
		command_input_height_ = ImClamp(command_input_height_, ImGui::GetFrameHeight(), max_input_height);
		if (splitter_active || splitter_hovered)
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
		const ImVec2 grip_min = ImGui::GetItemRectMin();
		const ImVec2 grip_max = ImGui::GetItemRectMax();
		const float grip_y = (grip_min.y + grip_max.y) * 0.5f;
		const ImU32 grip_color = ImGui::GetColorU32(
			splitter_active ? ImGuiCol_SeparatorActive :
			splitter_hovered ? ImGuiCol_SeparatorHovered : ImGuiCol_Separator);
		ImGui::GetWindowDrawList()->AddRectFilled(
			ImVec2(grip_min.x, grip_y - 1.0f), ImVec2(grip_max.x, grip_y + 1.0f), grip_color);

		bool send_clicked = false;
		if (focus_command_next_frame_) {
			ImGui::SetKeyboardFocusHere();
			focus_command_next_frame_ = false;
		}
		CommandInputCallbackContext command_callback_ctx{ this, &terminal };
		ImGui::InputTextMultiline(
			"##control_command",
			command_buf_,
			sizeof(command_buf_),
			ImVec2(-90.0f, command_input_height_),
			ImGuiInputTextFlags_CallbackAlways,
			command_input_callback,
			&command_callback_ctx);
		ImGui::SameLine();
		if (ImGui::Button("Send"))
			send_clicked = true;
		if (send_clicked)
			send_command_buffer(terminal);
	}

	std::vector<SerialPortInfo> ports_;
	int control_port_idx_ = -1;
	int debug_port_idx_ = -1;
	bool initialized_ = false;
	bool auto_scroll_ = true;
	bool focus_command_next_frame_ = false;
	float command_input_height_ = 0.0f;
	int command_last_len_ = 0;
	char command_buf_[4096] = {};
	std::vector<std::string> command_history_;
	std::string command_history_draft_;
	int command_history_index_ = -1;
	std::string status_;
	SerialTerminal control_;
	SerialTerminal debug_;
};

static AppletiniUartWindow& get_uart_window()
{
	static AppletiniUartWindow window;
	return window;
}

} // namespace

void appletini_uart_terminal_imgui_window(bool* p_open)
{
	get_uart_window().draw(p_open);
}

nlohmann::json appletini_uart_terminal_serialize()
{
	return get_uart_window().SerializeState();
}

void appletini_uart_terminal_deserialize(const nlohmann::json& jsonState)
{
	get_uart_window().DeserializeState(jsonState);
}
