#include "CycleCounter.h"
#include <mutex>
#include <iostream>

// below because "The declaration of a static data member in its class definition is not a definition"
CycleCounter* CycleCounter::s_instance;

static uint32_t m_cycle;		// Current cycle
std::mutex mtx_cycle;	// protect the cycle counter

const uint32_t cycles_scanline = 40; // each scanline is 40 bytes, 1 cycle per byte
const uint32_t cycles_hblank = 25;   // always 25 cycles
const uint32_t cycles_total = 17030;

struct VideoTiming {
	const uint32_t count_scanlines;
	const uint32_t cycles_screen;
	const uint32_t cycles_vblank;

	VideoTiming(uint32_t countScanlines)
		: count_scanlines(countScanlines),
		cycles_screen(countScanlines* (cycles_scanline + cycles_hblank)),
		cycles_vblank(262 * (cycles_scanline + cycles_hblank) - cycles_screen) {}
};

static VideoTiming timingApple2Modes(192); // number of visible scanlines
static VideoTiming timingSHR(200);

void CycleCounter::Initialize()
{
	// So we can render on startup, move the cycle counter to after HBlank
	std::lock_guard<std::mutex> lock(mtx_cycle);	// unlocked when goes out of scope
	m_cycle = cycles_hblank;
}

void CycleCounter::IncrementCycles(int inc, bool isVBL)
{
	std::lock_guard<std::mutex> lock(mtx_cycle);
	m_cycle += inc;
	m_cycle = (m_cycle % cycles_total);
	if (isVBL)
	{
		auto t = (isSHR ? timingSHR : timingApple2Modes);
		if (m_cycle < t.cycles_screen)
		{
			std::cout << "	VBL in cycle: " << m_cycle << std::endl;
			m_vbl_start = m_cycle;
			m_cycle = t.cycles_screen;
		}
	}
}

bool CycleCounter::IsVBL()
{
	return (m_cycle >= (isSHR ? timingSHR.cycles_screen : timingApple2Modes.cycles_screen));
}

bool CycleCounter::IsHBL()
{
	if (IsVBL())
		return false;
	auto lineCycle = 0;
	lineCycle = m_cycle % (cycles_scanline + cycles_hblank);
	return (lineCycle < cycles_hblank);
}

bool CycleCounter::IsInBlank()
{
	return (IsVBL() || IsHBL());
}

uint32_t CycleCounter::GetScanline()
{
	return m_cycle / (cycles_scanline + cycles_hblank);
}

uint32_t CycleCounter::GetByteYPos()
{
	return m_cycle % (cycles_scanline + cycles_hblank);
}

