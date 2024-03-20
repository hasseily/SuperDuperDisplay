#pragma once

#ifndef CYCLECOUNTER_H
#define CYCLECOUNTER_H

#include <stdint.h>
#include <stddef.h>

constexpr uint8_t _COLORBYTESOFFSET = 1 + 32;	// the color bytes are offset every line by 33 (after SCBs and palette)
constexpr uint32_t SCANLINES_TOTAL_NTSC = 262;
constexpr uint32_t SCANLINES_TOTAL_PAL = 312;
constexpr uint32_t CYCLES_HBLANK = 25;			// always 25 cycles
constexpr uint32_t COUNT_SCANLINES = 192;		// EVEN IN SHR with 200 visible lines, the VBL triggers at line 192.
constexpr uint32_t CYCLES_SCANLINES = 40;		// each scanline is 40 bytes, 1 cycle per byte
constexpr uint32_t CYCLES_SCREEN = (CYCLES_SCANLINES + CYCLES_HBLANK) * COUNT_SCANLINES;
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
	const VideoRegion_e GetVideoRegion();
	void SetVideoRegion(VideoRegion_e region);
	void Reset();
	
	// Gets the scanline (0-191 or 0-199 for SHR when not VBLANK)
	const uint32_t GetScanline();
	// Gets the Byte's X Position (0-39 or 0-159 for SHR)
	const uint32_t GetByteXPos();
	// Gets the number of cycles for the screen
	const uint32_t GetScreenCycles();
	// Shift the VBL start. It effectively moves the current cycle.
	void SetVBLStart(uint32_t _vblStart);
	
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
	
	uint32_t m_prev_vbl_start = 0;	// debug to know when we think vbl started previously

};

#endif // CYCLECOUNTER_H
