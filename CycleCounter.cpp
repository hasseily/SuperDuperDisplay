#include "CycleCounter.h"
#include <mutex>
#include <iostream>
#include "A2VideoManager.h"


// below because "The declaration of a static data member in its class definition is not a definition"
CycleCounter* CycleCounter::s_instance;

static uint32_t m_cycle = 0;				// Current cycle
static uint32_t m_cycle_alignments = 0;		// Number of times we aligned the cycles to the VBL seen
static bool bIsHBL = true;
static bool bIsVBL = false;
static VideoRegion_e m_region;	// We default to NTSC values unless we get a VBL in a wrong cycle
std::mutex mtx_cycle;	// protect the cycle counter

static uint32_t dbg_last_vbl_cycle = 0;

constexpr uint32_t COUNT_SCANLINES = 192;		// EVEN IN SHR with 200 visible lines, the VBL triggers at line 192.
constexpr uint32_t CYCLES_SCANLINES = 40;		// each scanline is 40 bytes, 1 cycle per byte
constexpr uint32_t CYCLES_HBLANK = 25;			// always 25 cycles
constexpr uint32_t CYCLES_SCREEN = (CYCLES_SCANLINES + CYCLES_HBLANK) * COUNT_SCANLINES;
constexpr uint32_t CYCLES_TOTAL_NTSC = 17030;
constexpr uint32_t CYCLES_TOTAL_PAL = 20280;

uint32_t cycles_vblank;
uint32_t cycles_total;

void CycleCounter::Initialize()
{
	// So we can render on startup, move the cycle counter to after HBlank
	m_cycle = CYCLES_HBLANK;
	bIsHBL = false;

	m_region = VideoRegion_e::NTSC;
	cycles_total = CYCLES_TOTAL_NTSC;
	cycles_vblank = cycles_total - CYCLES_SCREEN;
}

void CycleCounter::Reset()
{
	Initialize();
}

void CycleCounter::IncrementCycles(int inc, bool isVBL)
{
	m_cycle += inc;
	m_cycle = (m_cycle % cycles_total);
	if (isVBL)
	{
		if (m_cycle < CYCLES_SCREEN)
		{
			// determine the region
			if ((m_cycle_alignments > 0) && (m_cycle < 7000))	// shouldn't happen, so it's the other region
			{
				std::cout << "	VBL in cycle: " << m_cycle << ". " << std::endl;
				if (m_region == VideoRegion_e::NTSC)
					SetVideoRegion(VideoRegion_e::PAL);
				else
					SetVideoRegion(VideoRegion_e::NTSC);
			}
			else {
				m_prev_vbl_start = m_cycle;
				m_cycle = CYCLES_SCREEN;
				m_cycle_alignments++;
				std::cout << "VBL Alignment " << m_cycle_alignments 
					<< ": " << m_prev_vbl_start << " ---> " << m_cycle << std::endl;
			}
		}
	}
	bIsVBL = (m_cycle >= CYCLES_SCREEN);
	bIsHBL = (GetByteXPos() < CYCLES_HBLANK);
	A2VideoManager::GetInstance()->BeamIsAtPosition(GetByteXPos(), GetScanline());
}

const VideoRegion_e CycleCounter::GetVideoRegion()
{
	return m_region;
}

void CycleCounter::SetVideoRegion(VideoRegion_e region)
{
	if (region == m_region)
		return;
	
	switch (region) {
		case VideoRegion_e::PAL:
			m_region = VideoRegion_e::PAL;
			cycles_total = CYCLES_TOTAL_PAL;
			cycles_vblank = cycles_total - CYCLES_SCREEN;
			std::cout << "Switched to PAL." << std::endl;
			break;
		case VideoRegion_e::NTSC:
			m_region = VideoRegion_e::NTSC;
			cycles_total = CYCLES_TOTAL_NTSC;
			cycles_vblank = cycles_total - CYCLES_SCREEN;
			std::cout << "Switched to NTSC." << std::endl;
			break;
		default:
			std::cout << "ERROR: Unknown Region" << std::endl;
			break;
	}
	
	m_prev_vbl_start = CYCLES_SCREEN;
	m_cycle = CYCLES_SCREEN;
	m_cycle_alignments = 0;
}

const bool CycleCounter::IsVBL()
{
	return bIsVBL;
}

const bool CycleCounter::IsHBL()
{
	return bIsHBL;
}

const bool CycleCounter::IsInBlank()
{
	return (bIsVBL || bIsHBL);
}

const uint32_t CycleCounter::GetScanline()
{
	return m_cycle / (CYCLES_SCANLINES + CYCLES_HBLANK);
}

const uint32_t CycleCounter::GetByteXPos()
{
	return m_cycle % (CYCLES_SCANLINES + CYCLES_HBLANK);
}

const uint32_t CycleCounter::GetScreenCycles()
{
	return CYCLES_SCREEN;
}

void CycleCounter::SetVBLStart(uint32_t _vblStart)
{
	// Don't allow a VBL start that's after the screen cycles
	if (_vblStart > CYCLES_SCREEN)
		return;
	m_prev_vbl_start = CYCLES_SCREEN;
	// move the cycle to the requested change to the VBL
	m_cycle = m_cycle + (_vblStart - CYCLES_SCREEN);
	m_cycle_alignments = 0;
	return;
}
