#include "ShmProcessor.h"
#include <iostream>

#include "EventRecorder.h"
#include "CycleCounter.h"
#include "MemoryManager.h"
#include "A2VideoManager.h"

ShmProcessor::ShmProcessor()
	: map_base(nullptr)
#ifdef _WIN32
	, mem_fd(NULL)
#else
	, mem_fd(-1)
#endif
{
#ifdef _WIN32
	mem_fd = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 0x10000000, "APPLE2_EVENTS_MMAP");
	if (mem_fd == NULL) {
		std::cerr << "Could not create file mapping object: " << GetLastError() << std::endl;
		throw std::runtime_error("CreateFileMapping failed");
	}

	map_base = MapViewOfFile(mem_fd, FILE_MAP_READ, 0, 0, 0x10000000);
	if (map_base == NULL) {
		std::cerr << "Could not map view of file: " << GetLastError() << std::endl;
		CloseHandle(mem_fd);
		throw std::runtime_error("MapViewOfFile failed");
	}
#else
	mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
	if (mem_fd < 0) {
		perror("open");
		throw std::runtime_error("open failed");
	}

	map_base = mmap(NULL, 0x10000000, PROT_READ, MAP_SHARED, mem_fd, 0x81000000);
	if (map_base == MAP_FAILED) {
		perror("mmap");
		close(mem_fd);
		throw std::runtime_error("mmap failed");
	}
#endif
}

ShmProcessor::~ShmProcessor() {
#ifdef _WIN32
	if (map_base != NULL) {
		UnmapViewOfFile(map_base);
	}
	if (mem_fd != NULL) {
		CloseHandle(mem_fd);
	}
#else
	if (map_base != MAP_FAILED) {
		munmap(map_base, 0x10000000);
	}
	if (mem_fd >= 0) {
		close(mem_fd);
	}
#endif
}

void ShmProcessor::ProcessSHMEvents(bool* bShouldTerminateProcessing) {
	shm_layout* shm = static_cast<shm_layout*>(map_base);
	uint32_t event_ring_end = shm->event_ring_end;
	A2VideoManager::GetInstance()->ActivateBeam();

	while (!(*bShouldTerminateProcessing)) {
		uint32_t new_event_ring_end = shm->event_ring_end;
		while (event_ring_end != new_event_ring_end) {
			bus_event* ev = shm->event_ring + event_ring_end;
			auto shr_ev = SDHREvent((ev->control >> 3) & 1, (ev->control >> 2) & 1,
				(ev->control >> 1) & 1, ev->control & 1, ev->address, ev->data);
			ProcessSingleEvent(shr_ev);
			++event_ring_end;
			if (event_ring_end == 15 * 1024 * 1024) {
				event_ring_end = 0;
			}
		}
	}
	A2VideoManager::GetInstance()->DeactivateBeam();
}

void ShmProcessor::ProcessSingleEvent(SDHREvent& e)
{
	// std::cout << e.is_iigs << " " << e.rw << " " << std::hex << e.addr << " " << (uint32_t)e.data << std::endl;

	static auto eventRecorder = EventRecorder::GetInstance();
	if (eventRecorder->IsRecording())
		eventRecorder->RecordEvent(&e);
	// Update the cycle counting and VBL hit
	bool isVBL = ((e.addr == 0xC019) && e.rw && ((e.data >> 7) == (e.is_iigs ? 1 : 0)));
	CycleCounter::GetInstance()->IncrementCycles(1, isVBL);
	if (e.is_iigs && e.m2sel) {
		// ignore updates from iigs_mode firmware with m2sel high
		return;
	}
	if (e.rw && ((e.addr & 0xF000) != 0xC000)) {
		// ignoring all read events not softswitches
		return;
	}

	auto memMgr = MemoryManager::GetInstance();

	/*
	 *********************************
	 HANDLE SIMPLE MEMORY WRITE EVENTS
	 *********************************
	 */
	if ((e.addr >= _A2_MEMORY_SHADOW_BEGIN) && (e.addr < _A2_MEMORY_SHADOW_END)) {
		memMgr->WriteToMemory(e.addr, e.data, e.m2b0, e.is_iigs);
		return;
	}
	/*
	 *********************************
	 HANDLE SOFT SWITCHES EVENTS
	 *********************************
	 */
	 // TODO: *** SDHR IS DISABLED FOR 2GS ***
	 //		because we're getting spurious 0xC0A0 events from the GS
	if ((e.is_iigs == true) || ((e.addr != CXSDHR_CTRL) && (e.addr != CXSDHR_DATA))) {
		if (e.addr >> 8 == 0xc0)
			memMgr->ProcessSoftSwitch(e.addr, e.data, e.rw, e.is_iigs);
		// ignore non-control
		return;
	}
}