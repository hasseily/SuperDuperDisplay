#include "CycleCounter.h"
#include <mutex>
#include <iostream>
#include <chrono>
#include "A2VideoManager.h"
#include "SoundManager.h"


// below because "The declaration of a static data member in its class definition is not a definition"
CycleCounter* CycleCounter::s_instance;

static uint32_t m_cycle = 0;				// Current cycle
static uint32_t m_cycle_alignments = 0;		// Number of times we aligned the cycles to the VBL seen
static bool bIsHBL = true;
static bool bIsVBL = false;
static VideoRegion_e m_region;	// We default to NTSC values unless we get a VBL in a wrong cycle
std::mutex mtx_cycle;	// protect the cycle counter

uint32_t cycles_vblank;
uint32_t cycles_total;

size_t CycleCounter::GetCurrentTimeInMicroseconds() {
	// Get the current time point from the high-resolution clock
	auto now = std::chrono::high_resolution_clock::now();
	
	// Convert the time point to a duration since epoch in microseconds
	auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch());
	
	// Return the count of microseconds
	return duration.count();
}

void CycleCounter::Initialize()
{
	m_tstamp_init = GetCurrentTimeInMicroseconds();
	// So we can render on startup, move the cycle counter to after HBlank
	m_cycle = CYCLES_SC_HBL;
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
	m_tstamp_cycle = GetCurrentTimeInMicroseconds() - m_tstamp_init;
	m_cycle += inc;
	m_cycle = (m_cycle % cycles_total);

	// Update VBL and region automatically with 0xC019
	if (isVideoRegionDynamic && isVBL)
	{
		if (m_cycle < CYCLES_SCREEN)
		{
			m_cycle_alignments++;
			// determine the region
			if ((m_cycle_alignments > 1) && (m_cycle < (CYCLES_SCREEN - cycles_vblank)))
			{
				// If we've already done one cycle alignment at least, then if we're in the
				// correct region we're certain that the VBL is beyond (CYCLES_SCREEN - cycles_vblank).
				// Hence here we've demonstrated that we're in the wrong region and should switch region.
				std::cout << "	VBL in cycle: " << m_cycle << ". " << std::endl;
				if (m_region == VideoRegion_e::NTSC)
					SetVideoRegion(VideoRegion_e::PAL);
				else
					SetVideoRegion(VideoRegion_e::NTSC);
			}

			m_prev_vbl_start = m_cycle;
			while (m_cycle < CYCLES_SCREEN)	// move m_cycle to VBLANK start
			{
				bIsVBL = false;
				bIsHBL = (GetByteXPos() < CYCLES_SC_HBL);
				A2VideoManager::GetInstance()->BeamIsAtPosition(GetByteXPos(), GetScanline());
				++m_cycle;
			}
			std::cout << "VBL Alignment " << m_cycle_alignments
				<< ": " << m_prev_vbl_start << " ---> " << m_cycle << std::endl;
		}
	}

	bIsVBL = (m_cycle >= CYCLES_SCREEN);
	bIsHBL = (GetByteXPos() < CYCLES_SC_HBL);
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
			SoundManager::GetInstance()->SetPAL(true);
			std::cout << "Switched to PAL." << std::endl;
			break;
		case VideoRegion_e::NTSC:
			m_region = VideoRegion_e::NTSC;
			cycles_total = CYCLES_TOTAL_NTSC;
			cycles_vblank = cycles_total - CYCLES_SCREEN;
			SoundManager::GetInstance()->SetPAL(false);
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
	return m_cycle / (CYCLES_SC_CONTENT + CYCLES_SC_HBL);
}

const uint32_t CycleCounter::GetByteXPos()
{
	return m_cycle % (CYCLES_SC_CONTENT + CYCLES_SC_HBL);
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
