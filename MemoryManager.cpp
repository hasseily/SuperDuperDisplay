#include "MemoryManager.h"

// below because "The declaration of a static data member in its class definition is not a definition"
MemoryManager* MemoryManager::s_instance;
uint16_t a2SoftSwitches = A2SS_TEXT;
uint8_t switch_c022 = 0b11110000;	// white fg, black bg
uint8_t switch_c034 = 0;

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
	memset(a2mem, 0, _A2_MEMORY_SHADOW_END * 2);
	a2SoftSwitches = A2SS_TEXT; // default to TEXT1
	switch_c022 = 0b11110000;	// white fg, black bg
	switch_c034 = 0;
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
	//std::cerr << "Processing soft switch " << std::hex << (uint32_t)addr << " RW: " << (uint32_t)rw << " 2gs: " << (uint32_t)is_iigs << std::endl;
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
	case 0xC05E:	// DHIRESON
		a2SoftSwitches |= A2SS_DHGR;
		break;
	case 0xC05F:	// DHIRESOFF
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
	default:
		break;
	}
}