#pragma once

#ifndef CYCLECOUNTER_H
#define CYCLECOUNTER_H

#include <stdint.h>
#include <stddef.h>

constexpr uint32_t SC_TOTAL_NTSC = 262;
constexpr uint32_t SC_TOTAL_PAL = 312;
constexpr uint32_t CYCLES_SC_HBL = 25;			// always 25 cycles
constexpr uint32_t CYCLES_SC_CONTENT = 40;		// each scanline content area is 40 bytes, 1 cycle per byte
constexpr uint32_t CYCLES_SC_TOTAL = CYCLES_SC_CONTENT + CYCLES_SC_HBL;
constexpr uint32_t COUNT_SC_CONTENT = 192;		// EVEN IN SHR with 200 visible lines, the VBL triggers at line 192.
constexpr uint32_t CYCLES_SCREEN = CYCLES_SC_TOTAL * COUNT_SC_CONTENT;
constexpr uint32_t CYCLES_TOTAL_NTSC = 17030;
constexpr uint32_t CYCLES_TOTAL_PAL = 20280;

enum class VideoRegion_e
{
	Unknown = 0,
	NTSC = 1,
	PAL = 2
};

class CycleCounter
{
public:
	void IncrementCycles(int inc, bool isVBL);
	const bool IsVBL();
	const bool IsHBL();
	const bool IsInBlank();
	bool isVideoRegionDynamic = true;		// Video region will reconfigure automatically as necessary
	const VideoRegion_e GetVideoRegion();
	void SetVideoRegion(VideoRegion_e region);
	void Reset();
	
	// Gets the timestamp of the current cycle, in usec since init
	const size_t GetCycleTimestamp() { return m_tstamp_cycle; };
	// Gets the scanline (0-191 or 0-199 for SHR when not VBLANK)
	const uint32_t GetScanline();
	// Gets the Byte's X Position (0-39 or 0-159 for SHR)
	const uint32_t GetByteXPos();
	// Gets the number of cycles for the screen
	const uint32_t GetScreenCycles();
	// Shift the VBL start. It effectively moves the current cycle.
	void SetVBLStart(uint32_t _vblStart);
	// Get cycles since reset
	uint64_t GetCyclesSinceReset() { return m_cycles_since_reset; };
	
	// public singleton code
	static CycleCounter* GetInstance()
	{
		if (NULL == s_instance)
			s_instance = new CycleCounter();
		return s_instance;
	}
private:
//////////////////////////////////////////////////////////////////////////
// Singleton pattern
//////////////////////////////////////////////////////////////////////////
	void Initialize();

	static CycleCounter* s_instance;
	CycleCounter()
	{
		Initialize();
	}
	
	size_t GetCurrentTimeInMicroseconds();
	
	uint32_t m_prev_vbl_start = 0;	// debug to know when we think vbl started previously
	size_t m_tstamp_init = 0;		// tstamp at initalization, as microseconds since epoch
	size_t m_tstamp_cycle = 0;		// current tstamp of cycle, as microseconds since m_tstamp_init
	uint64_t m_cycles_since_reset = 0;	// total cycles since computer reset

};

#endif // CYCLECOUNTER_H
