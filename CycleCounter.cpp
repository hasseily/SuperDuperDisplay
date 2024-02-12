#include "CycleCounter.h"

// below because "The declaration of a static data member in its class definition is not a definition"
CycleCounter* CycleCounter::s_instance;

struct VideoTiming {
	const uint32_t count_scanlines;
	const uint32_t cycles_scanline; // each scanline is 40 bytes, 1 cycle per byte
	const uint32_t cycles_hblank;
	const uint32_t cycles_vblank;
	const uint32_t cycles_screen;	// (cyclesScanline + cyclesHblank) * count_scanlines
	const uint32_t cycles_total;	// cycles_screen + cycles_vblank

	VideoTiming(uint32_t countScanlines, uint32_t cyclesScanline, uint32_t cyclesHblank, uint32_t cyclesVblank)
		: count_scanlines(countScanlines), cycles_scanline(cyclesScanline), cycles_vblank(cyclesVblank),
		cycles_hblank(cyclesHblank), cycles_screen((cyclesScanline + cyclesHblank) * count_scanlines),
		cycles_total(cycles_screen + cycles_vblank) {}
};

void CycleCounter::Initialize()
{
	VideoTiming timingApple2Modes(192, 40, 25, 4550);	// count_scanlines, cycles_scanline, cycles_hblank, cycles_vblank
	VideoTiming timingSHR(200, 80, 25, 4550);
}

