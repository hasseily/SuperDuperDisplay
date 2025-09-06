#include "MemoryManager.h"
#include "CycleCounter.h"
#include "A2VideoManager.h"
#include <iostream>
#include <sstream>

float Memory_HighlightWriteFunction(const uint8_t* data, size_t offset, uint8_t cutoffSeconds) {
	(void)data;
	// data pointer is the start of memory
	auto usecdelta = CycleCounter::GetInstance()->GetCycleTimestamp() - MemoryManager::GetInstance()->GetMemWriteTimestamp(offset);
	// Return a range between 0 and 1, where 1 is newly updated and 0 is at least 1 second ago
	if (usecdelta >= 1'000'000 * cutoffSeconds)
		return 0.f;
	return (1.f - (static_cast<float>(usecdelta) / (1'000'000 * cutoffSeconds)));
}

// below because "The declaration of a static data member in its class definition is not a definition"
MemoryManager* MemoryManager::s_instance;

MemoryManager::~MemoryManager()
{
	delete[] a2mem;
}

void MemoryManager::Initialize()
{
	// Initialize the Apple 2 memory duplicate
	// Whenever memory is written from the Apple2
	// in any of the 2 banks between $200 and _A2_MEMORY_SHADOW_END it will
	// be sent through the socket and this buffer will be updated
	// memory of both banks is concatenated into one buffer
	memset(a2mem, 0x00, _A2_MEMORY_SHADOW_END * 2);
	a2SoftSwitches = A2SS_TEXT; // default to TEXT1
	switch_c022 = 0b11110000;	// white fg, black bg
	switch_c034 = 0;
	is2gs = false;
}

// Return a pointer to the shadowed apple 2 memory
uint8_t* MemoryManager::GetApple2MemPtr()
{
	return a2mem;
}

// Return a pointer to the shadowed apple 2 aux memory
uint8_t* MemoryManager::GetApple2MemAuxPtr()
{
	return a2mem + _A2_MEMORY_SHADOW_END;
}

void MemoryManager::SetSoftSwitch(A2SoftSwitch_e ss, bool state)
{
	if (state)
		a2SoftSwitches |= ss;
	else
		a2SoftSwitches &= ~ss;
}

void MemoryManager::ProcessSoftSwitch(uint16_t addr, uint8_t val, bool rw, bool is_iigs)
{
	(void)is_iigs;		// mark as unused -- Appletini brings iigs features to the //e
	//std::cerr << "Processing soft switch " << std::hex << (uint32_t)addr << " RW: " << (uint32_t)rw << " 2gs: " << (uint32_t)is_iigs << std::endl;

	/*
	* The below is a hack and just here to remember the code.
	// This is to handle Video-7 cards, but Appletini needs to register as a Video-7 card!
	// Check the Video-7 special mode changes
	// using a 2-bit state register
	// They're activated when the DHIRES (!AN3) switch is toggled exactly 5 times while A2SS_MIXED is OFF:
	// OFF-ON-OFF-ON-OFF
	// At each of the 2 ON, it sets a bit based on the 80COL value

	if ((addr & 0b1) == 0xC05E)		// Case of DHIRES (!AN3) being hit: 0xC05E and 0xC05F
	{
		if (IsSoftSwitch(A2SS_MIXED))	// MIXED should always be OFF for the Video7 modes
		{
			stateAN3Video7 = 0;
			goto AFTERVIDEO7;
		}
		bool setOn = (addr == 0xC05E);
		if ((stateAN3Video7 == 0) && setOn)	// Not a start of sequence, it needs to start set to off
			goto AFTERVIDEO7;
		if ((stateAN3Video7 & 0b1) != setOn)		// toggle
		{
			++stateAN3Video7;
			std::cerr << "toggled AN3 " << stateAN3Video7 << std::endl;
		}
		if (stateAN3Video7 == 2)
		{
			flagsVideo7 = !IsSoftSwitch(A2SS_80COL);
		}
		else if (stateAN3Video7 == 4)
		{
			flagsVideo7 += 2 * !IsSoftSwitch(A2SS_80COL);
		}
		else if (stateAN3Video7 == 5)	// 5 total toggles, Video-7 is switching modes!
		{
			stateAN3Video7 = 0;
			// Video7 special modes:
			// 00 : 140x192
			// 01 : 160x192
			// 10: MIX mode (ie. COL140Mixed mode) 140x192 + 560x192
			// 11 : 560x192 monochrome
			SetSoftSwitch(A2SS_DHGRMONO, false);
			A2VideoManager::GetInstance()->bUseDHGRCOL140Mixed = false;
			A2VideoManager::GetInstance()->bUseDHGR160 = false;
			switch (flagsVideo7)
			{
			case 0b00:		// MIXED ON  80COL ON : 140x192
				// Default
				break;
			case 0b01:		// MIXED OFF 80COL ON : 160x192
				A2VideoManager::GetInstance()->bUseDHGR160 = true;
				break;
			case 0b10:		// MIX mode (ie. COL140Mixed mode) 140x192 + 560x192
				// TODO: Hack. Don't force usage of A2VideoManager. Instead use another SS variable
				// for A2VideoManager to determine what to do, or have a special Video7 module
				A2VideoManager::GetInstance()->bUseDHGRCOL140Mixed = true;
				break;
			case 0b11:		// 560x192 monochrome
				SetSoftSwitch(A2SS_DHGRMONO, true);
				break;
			default:
				break;
			}
		}
	}
	else {
		stateAN3Video7 = 0;			// Reset the toggle state
	}
	*/

AFTERVIDEO7:
	switch (addr)
	{
	case 0xC000:	// 80STOREOFF
		if (!rw)
			a2SoftSwitches &= ~A2SS_80STORE;
		break;
	case 0xC001:	// 80STOREON
		if (!rw)
			a2SoftSwitches |= A2SS_80STORE;
		break;
	case 0xC002:	// RAMRDOFF
		if (!rw)
			a2SoftSwitches &= ~A2SS_RAMRD;
		break;
	case 0xC003:	// RAMRDON
		if (!rw)
			a2SoftSwitches |= A2SS_RAMRD;
		break;
	case 0xC004:	// RAMWRTOFF
		if (!rw)
			a2SoftSwitches &= ~A2SS_RAMWRT;
		break;
	case 0xC005:	// RAMWRTON
		if (!rw)
			a2SoftSwitches |= A2SS_RAMWRT;
		break;
	case 0xC006:	// INTCXROMOFF
		if (!rw)
			a2SoftSwitches &= ~A2SS_INTCXROM;
		break;
	case 0xC007:	// INTCXROMON
		if (!rw)
			a2SoftSwitches |= A2SS_INTCXROM;
		break;
	case 0xC00A:	// SLOTC3ROMOFF
		if (!rw)
			a2SoftSwitches &= ~A2SS_SLOTC3ROM;
		break;
	case 0xC00B:	// SLOTC3ROMOFF
		if (!rw)
			a2SoftSwitches |= A2SS_SLOTC3ROM;
		break;
	case 0xC00C:	// 80COLOFF
		if (!rw)
			a2SoftSwitches &= ~A2SS_80COL;
		break;
	case 0xC00D:	// 80COLON
		if (!rw)
			a2SoftSwitches |= A2SS_80COL;
		break;
	case 0xC00E:	// ALTCHARSETOFF
		if (!rw)
			a2SoftSwitches &= ~A2SS_ALTCHARSET;
		break;
	case 0xC00F:	// ALTCHARSETON
		if (!rw)
			a2SoftSwitches |= A2SS_ALTCHARSET;
		break;
	case 0xC050:	// TEXTOFF
		a2SoftSwitches &= ~A2SS_TEXT;
		break;
	case 0xC051:	// TEXTON
		a2SoftSwitches |= A2SS_TEXT;
		break;
	case 0xC052:	// MIXEDOFF
		a2SoftSwitches &= ~A2SS_MIXED;
		break;
	case 0xC053:	// MIXEDON
		a2SoftSwitches |= A2SS_MIXED;
		break;
	case 0xC054:	// PAGE2OFF
		a2SoftSwitches &= ~A2SS_PAGE2;
		break;
	case 0xC055:	// PAGE2ON
		a2SoftSwitches |= A2SS_PAGE2;
		break;
	case 0xC056:	// HIRESOFF
		a2SoftSwitches &= ~A2SS_HIRES;
		break;
	case 0xC057:	// HIRESON
		a2SoftSwitches |= A2SS_HIRES;
		break;
	case 0xC05E:	// DHIRESON	(AN3 RESET)
		a2SoftSwitches |= A2SS_DHGR;
		break;
	case 0xC05F:	// DHIRESOFF (AN3 SET)
		a2SoftSwitches &= ~A2SS_DHGR;
		break;
	case 0xC021:	// MONOCOLOR
		// bits 0-6 are reserved
		// bit 7 determines color or greyscale. Greyscale is 1
		if (!rw)
		{
			SetSoftSwitch(A2SS_GREYSCALE, (bool)(val >> 7));
		}
		break;
		// $C022   R / W     SCREENCOLOR[IIgs] text foreground and background colors(also VidHD)
	case 0xC022:	// Set screen color
		//            std::cerr << "Processing soft switch " << std::hex << (uint32_t)addr <<
		//            " VAL: " << (uint32_t)val <<
		//            " RW: " << (uint32_t)rw << std::endl;
		if (!rw)
		{
			switch_c022 = val;
		}
		break;
		// $C034   R / W     BORDERCOLOR[IIgs] b3:0 are border color(also VidHD)
	case 0xC034:	// Set border color on bits 3:0
		if (!rw)
		{
			switch_c034 = val;
		}
		break;
		// $C029   R/W     NEWVIDEO        [IIgs] Select new video modes (also VidHD)
	case 0xC029:    // NEWVIDEO (SHR)
		if (rw)		// don't do anything on read
			break;
		// bits 1-4 are reserved
		if (val == 0x21)
		{
			// Return to mode TEXT
			a2SoftSwitches &= ~A2SS_SHR;
			a2SoftSwitches |= A2SS_TEXT;
			break;
		}
		if (val & 0x20)		// bit 5
		{
			// DHGR in Monochrome @ 560x192
			a2SoftSwitches |= A2SS_DHGRMONO;
		}
		else {
			// DHGR in 16 Colors @ 140x192
			a2SoftSwitches &= ~A2SS_DHGRMONO;
		}
		if (val & 0x40)	// bit 6
		{
			// Video buffer is contiguous 0x2000-0x9D00 in bank E1
		}
		else {
			// AUX memory behaves like Apple //e
		}
		if (val & 0x80)	// bit 7
		{
			// SHR video mode. Bit 6 is considered on
			a2SoftSwitches |= A2SS_SHR;
		}
		else {
			// Classic Apple 2 video modes
			a2SoftSwitches &= ~A2SS_SHR;
		}
		break;
	case 0xC013:
		if (rw) {
			if (val & 0x80) {
				a2SoftSwitches |= A2SS_RAMRD;
			}
			else {
				a2SoftSwitches &= ~A2SS_RAMRD;
			}
		}
		break;
	case 0xC014:
		if (rw) {
			if (val & 0x80) {
				a2SoftSwitches |= A2SS_RAMWRT;
			}
			else {
				a2SoftSwitches &= ~A2SS_RAMWRT;
			}
		}
		break;
	case 0xC015:
		if (rw) {
			if (val & 0x80) {
				a2SoftSwitches |= A2SS_INTCXROM;
			}
			else {
				a2SoftSwitches &= ~A2SS_INTCXROM;
			}
		}
		break;
	case 0xC017:
		if (rw) {
			if (val & 0x80) {
				a2SoftSwitches |= A2SS_SLOTC3ROM;
			}
			else {
				a2SoftSwitches &= ~A2SS_SLOTC3ROM;
			}
		}
		break;
	case 0xC018:
		if (rw) {
			if (val & 0x80) {
				a2SoftSwitches |= A2SS_80STORE;
			}
			else {
				a2SoftSwitches &= ~A2SS_80STORE;
			}
		}
		break;
	case 0xC01A:
		if (rw) {
			if (val & 0x80) {
				a2SoftSwitches |= A2SS_TEXT;
			}
			else {
				a2SoftSwitches &= ~A2SS_TEXT;
			}
		}
		break;
	case 0xC01B:
		if (rw) {
			if (val & 0x80) {
				a2SoftSwitches |= A2SS_MIXED;
			}
			else {
				a2SoftSwitches &= ~A2SS_MIXED;
			}
		}
		break;
	case 0xC01C:
		if (rw) {
			if (val & 0x80) {
				a2SoftSwitches |= A2SS_PAGE2;
			}
			else {
				a2SoftSwitches &= ~A2SS_PAGE2;
			}
		}
		break;
	case 0xC01D:
		if (rw) {
			if (val & 0x80) {
				a2SoftSwitches |= A2SS_HIRES;
			}
			else {
				a2SoftSwitches &= ~A2SS_HIRES;
			}
		}
		break;
	case 0xC01E:
		if (rw) {
			if (val & 0x80) {
				a2SoftSwitches |= A2SS_ALTCHARSET;
			}
			else {
				a2SoftSwitches &= ~A2SS_ALTCHARSET;
			}
		}
		break;
	case 0xC01F:
		if (rw) {
			if (val & 0x80) {
				a2SoftSwitches |= A2SS_80COL;
			}
			else {
				a2SoftSwitches &= ~A2SS_80COL;
			}
		}
		break;
	case 0xC07F:
		if (rw) {
			if (val & 0x80) {
				a2SoftSwitches |= A2SS_DHGR;
			}
			else {
				a2SoftSwitches &= ~A2SS_DHGR;
			}
		}
	 break;
	default:
		break;
	}
}

void MemoryManager::WriteToMemory(uint16_t addr, uint8_t val, bool m2b0, bool is_iigs) {
	is2gs = is_iigs;
	uint8_t _sw = 0;	// switches state
	if (IsSoftSwitch(A2SS_80STORE))
		_sw |= 0b001;
	if (IsSoftSwitch(A2SS_RAMWRT))
		_sw |= 0b010;
	if (IsSoftSwitch(A2SS_PAGE2))
		_sw |= 0b100;
	bool bIsAux = false;
	switch (_sw)
	{
		case 0b010:
			// Only writes 0000-01FF to MAIN
			if (addr < 0x200)
				bIsAux = false;
			else
				bIsAux = true;
			break;
		case 0b011:
			// anything not page 1 (including 0000-01FFF goes to AUX
			if ((addr >= 0x400 && addr < 0x800)
				|| (addr >= 0x2000 && addr < 0x4000))
				bIsAux = false;
			else
				bIsAux = true;
			break;
		case 0b101:
			// Page 1 is in AUX
			if ((addr >= 0x400 && addr < 0x800)
				|| (addr >= 0x2000 && addr < 0x4000))
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
	if (is_iigs && m2b0)
	{
		// check if SHR is being accessed
		// SHR is only accessible when the SHR flag is on
		if (!IsSoftSwitch(A2SS_SHR) && (addr >= 0x2000 && addr < 0xA000))
			return;
		bIsAux = true;
	}
	
	if (bIsAux)
	{
		a2mem[_A2_MEMORY_SHADOW_END + addr] = val;
		a2mem_lastUpdate[_A2_MEMORY_SHADOW_END + addr] = CycleCounter::GetInstance()->GetCycleTimestamp();
	}
	else {
		a2mem[addr] = val;
		a2mem_lastUpdate[addr] = CycleCounter::GetInstance()->GetCycleTimestamp();

		// Handle Main ZERO PAGE data changes
		if (addr < 0x100)
		{
			// Check for PR#1 (assuming the tini is in slot 1
			// TODO: Have a way to get the tini to tell us its slot
			// TODO: Have a way to get the tini to tell us to switch between the new modes
			if ((addr == 0x36) || (addr == 0x37))	// PR#x
			{
				if (a2mem[0x36] == 0x00 && a2mem[0x37] == 0xC1)	// PR#1 is called, COUT is going to $C001
				{
					// switch to the new video modes
					A2VideoManager::GetInstance()->vidhdWindowBeam->SetVideoMode(VIDHDMODE_TEXT_40X24);
				}
				else {
					// switch to the standard Apple 2 video modes
					A2VideoManager::GetInstance()->vidhdWindowBeam->SetVideoMode(VIDHDMODE_NONE);
				}
			}
		}
	}
}

std::string MemoryManager::SerializeSwitches() const {
	std::ostringstream out;
	out.write(reinterpret_cast<const char*>(&a2SoftSwitches), sizeof(a2SoftSwitches));
	out.write(reinterpret_cast<const char*>(&switch_c022), sizeof(switch_c022));
	out.write(reinterpret_cast<const char*>(&switch_c034), sizeof(switch_c034));
	out.write(reinterpret_cast<const char*>(&is2gs), sizeof(is2gs));
	return out.str();
}

void MemoryManager::DeserializeSwitches(const std::string& data) {
	std::istringstream in(data);
	in.read(reinterpret_cast<char*>(&a2SoftSwitches), sizeof(a2SoftSwitches));
	in.read(reinterpret_cast<char*>(&switch_c022), sizeof(switch_c022));
	in.read(reinterpret_cast<char*>(&switch_c034), sizeof(switch_c034));
	in.read(reinterpret_cast<char*>(&is2gs), sizeof(is2gs));
}
