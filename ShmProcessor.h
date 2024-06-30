#pragma once

#ifndef SHM_PROCESSOR_H
#define SHM_PROCESSOR_H

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#endif
#include "SDHRNetworking.h"

class ShmProcessor {
public:
	ShmProcessor(const char* shm_name = "/dev/mem");
	~ShmProcessor();
	void ProcessSHMEvents(bool* bShouldTerminateProcessing);
	void ProcessSingleEvent(SDHREvent& e);
private:
	struct bus_event {
		uint32_t address;
		uint32_t data;
		uint32_t control;
		uint32_t pad1;
	};

	struct shm_layout {
		volatile uint32_t event_ring_end;
		uint32_t pad2[7];
		struct bus_event event_ring[15 * 1024 * 1024];
	};

	void* map_base;
#ifdef _WIN32
	HANDLE mem_fd;
#else
	int mem_fd;
#endif
};

#endif // SHM_PROCESSOR_H
