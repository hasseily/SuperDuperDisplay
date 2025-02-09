#pragma once

#ifndef MEMORYMANAGER_H
#define MEMORYMANAGER_H

#include <stdint.h>
#include <stddef.h>

#include "common.h"

/*
MEMORY MANAGEMENT SOFT SWITCHES
 $C000   W       80STOREOFF      Allow page2 to switch video page1 page2
 $C001   W       80STOREON       Allow page2 to switch main & aux video memory
 $C002   W       RAMRDOFF        Read enable main memory from $0200-$BFFF
 $C003   W       RAMRDON         Read enable aux memory from $0200-$BFFF
 $C004   W       RAMWRTOFF       Write enable main memory from $0200-$BFFF
 $C005   W       RAMWRTON        Write enable aux memory from $0200-$BFFF
 $C006   W       INTCXROMOFF     Enable slot ROM from $C100-$C7FF (but $C800-$CFFF depends on INTC8ROM)
 $C007   W       INTCXROMON      Enable main ROM from $C100-$CFFF
 $C008   W       ALTZPOFF        Enable main memory from $0000-$01FF & avl BSR
 $C009   W       ALTZPON         Enable aux memory from $0000-$01FF & avl BSR
 $C00A   W       SLOTC3ROMOFF    Enable main ROM from $C300-$C3FF
 $C00B   W       SLOTC3ROMON     Enable slot ROM from $C300-$C3FF
 $C07E   W       IOUDIS          [//c] On: disable IOU access for addresses $C058 to $C05F; enable access to DHIRES switch
 $C07F   W       IOUDIS          [//c] Off: enable IOU access for addresses $C058 to $C05F; disable access to DHIRES switch

VIDEO SOFT SWITCHES
 $C00C   W       80COLOFF        Turn off 80 column display
 $C00D   W       80COLON         Turn on 80 column display
 $C00E   W       ALTCHARSETOFF   Turn off alternate characters
 $C00F   W       ALTCHARSETON    Turn on alternate characters
 $C019   R7      VERTBLANK       Bit 7 off: During vertical blank in //e. Reversed in IIgs
 $C021   R/W     MONOCOLOR       [IIgs] Bit 7 on: Greyscale
 $C022   R/W     SCREENCOLOR     [IIgs] text foreground and background colors (also VidHD)
 $C029   R/W     NEWVIDEO        [IIgs] Select new video modes (also VidHD)
 $C034   R/W     BORDERCOLOR     [IIgs] b3:0 are border color (also VidHD)
 $C035   R/W     SHADOW          [IIgs] auxmem-to-bank-E1 shadowing (also VidHD)
 $C050   R/W     TEXTOFF         Select graphics mode
 $C051   R/W     TEXTON          Select text mode
 $C052   R/W     MIXEDOFF        Use full screen for graphics
 $C053   R/W     MIXEDON         Use graphics with 4 lines of text
 $C054   R/W     PAGE2OFF        Select page1 display (or main video memory)
 $C055   R/W     PAGE2ON         Select page2 display (or aux video memory)
 $C056   R/W     HIRESOFF        Select low resolution graphics
 $C057   R/W     HIRESON         Select high resolution graphics
 $C05E   R/W     DHIRESON        AN3: Select double (14M) resolution graphics (DLGR or DHGR)
 $C05F   R/W     DHIRESOFF       AN3: Select single (7M) resolution graphics
*/
enum A2SoftSwitch_e
{
	A2SS_80STORE = 0b000000000000001,
	A2SS_RAMRD = 0b000000000000010,
	A2SS_RAMWRT = 0b000000000000100,
	A2SS_80COL = 0b000000000001000,
	A2SS_ALTCHARSET = 0b000000000010000,
	A2SS_INTCXROM = 0b000000000100000,
	A2SS_SLOTC3ROM = 0b000000001000000,
	A2SS_TEXT = 0b000000010000000,
	A2SS_MIXED = 0b000000100000000,
	A2SS_PAGE2 = 0b000001000000000,
	A2SS_HIRES = 0b000010000000000,
	A2SS_DHGR = 0b000100000000000,
	A2SS_DHGRMONO = 0b001000000000000,
	A2SS_SHR = 0b010000000000000,
	A2SS_GREYSCALE = 0b100000000000000,
};

// For highlighting in the UI memory last written to. De-highlights after cutoffSeconds
float Memory_HighlightWriteFunction(const uint8_t* data, size_t offset, uint8_t cutoffSeconds = 1);

class MemoryManager
{
public:

	//////////////////////////////////////////////////////////////////////////
	// Methods
	//////////////////////////////////////////////////////////////////////////

	uint8_t* GetApple2MemPtr();	// Gets the Apple 2 main memory pointer
	uint8_t* GetApple2MemAuxPtr();	// Gets the Apple 2 aux memory pointer
	
	size_t GetMemWriteTimestamp(size_t offset) {
		if (offset >= (_A2_MEMORY_SHADOW_END * 2))
			return 0;
		return a2mem_lastUpdate[offset];
	};
	
	// Use this method to set a byte. It will choose which bank based on current softswitches
	void WriteToMemory(uint16_t addr, uint8_t val, bool m2b0, bool is_iigs);

	inline bool IsSoftSwitch(A2SoftSwitch_e ss) { return (a2SoftSwitches & ss); };
	void SetSoftSwitch(A2SoftSwitch_e ss, bool state);
	void ProcessSoftSwitch(uint16_t addr, uint8_t val, bool rw, bool is_iigs);

	// De/serialization in case one wants to save and restore state
	std::string SerializeSwitches() const;
	void DeserializeSwitches(const std::string& data);

	// public singleton code
	static MemoryManager* GetInstance()
	{
		if (NULL == s_instance)
			s_instance = new MemoryManager();
		return s_instance;
	};
	void Initialize();
	~MemoryManager();

	//////////////////////////////////////////////////////////////////////////
	// Attributes
	//////////////////////////////////////////////////////////////////////////
	int switch_c022;					// Exact value of the switch c022	fg/bg color
	int switch_c034;					// Exact value of the switch c034	border color
	bool is2gs;

private:
	//////////////////////////////////////////////////////////////////////////
	// Singleton pattern
	//////////////////////////////////////////////////////////////////////////

	static MemoryManager* s_instance;
	MemoryManager()
	{
		auto _memsize = _A2_MEMORY_SHADOW_END * 2;		// anything below _A2_MEMORY_SHADOW_BEGIN is unused
		a2mem = new uint8_t[_memsize];
		a2mem_lastUpdate = new size_t[_memsize];
		if (a2mem == NULL || a2mem_lastUpdate == NULL)
		{
			std::cerr << "FATAL ERROR: COULD NOT ALLOCATE Apple 2 MEMORY" << std::endl;
			exit(1);
		}
		
		Initialize();
	}

	//////////////////////////////////////////////////////////////////////////
	// Internal methods
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Internal data
	//////////////////////////////////////////////////////////////////////////

	uint8_t* a2mem;					// The current shadowed Apple 2 memory
	size_t* a2mem_lastUpdate;		// timestamp of last update of each Apple 2 memory byte
	uint16_t a2SoftSwitches;		// Soft switches states
	uint8_t stateAN3Video7 = 0;		// State of the AN3 toggle for Video-7. Needs to toggle 5 times, starting with off
	uint8_t flagsVideo7 = 0;		// 2 bits
};

#endif	// MEMORYMANAGER_H
