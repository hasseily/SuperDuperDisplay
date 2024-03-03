#pragma once

#ifndef CYCLECOUNTER_H
#define CYCLECOUNTER_H

#include <stdint.h>
#include <stddef.h>

enum class VideoRegion_e
{
	Unknown = 0,
	NTSC = 1,
	PAL = 2
};

struct BeamCycle {
	uint16_t x;
	uint16_t y;
	BeamCycle(uint16_t _x = 0, uint16_t _y = 0) : x(_x), y(_y) {}
};

class CycleCounter
{
public:
	BeamCycle IncrementCycles(int inc, bool isVBL);	// returns the new cycle
	void SetCycle(BeamCycle _cycle);
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
	
	bool isSHR = false;
	uint32_t frameIndex = 0;

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
