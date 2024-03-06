#pragma once

#ifndef MEMORYMANAGER_H
#define MEMORYMANAGER_H

#include <stdint.h>
#include <stddef.h>
#include <mutex>

#include "common.h"

class MemoryManager
{
public:

	//////////////////////////////////////////////////////////////////////////
	// Methods
	//////////////////////////////////////////////////////////////////////////

	uint8_t* GetApple2MemPtr();	// Gets the Apple 2 main memory pointer
	uint8_t* GetApple2MemAuxPtr();	// Gets the Apple 2 aux memory pointer

	// public singleton code
	static MemoryManager* GetInstance()
	{
		if (NULL == s_instance)
			s_instance = new MemoryManager();
		return s_instance;
	};
	void Initialize();
	~MemoryManager();

private:
	//////////////////////////////////////////////////////////////////////////
	// Singleton pattern
	//////////////////////////////////////////////////////////////////////////

	static MemoryManager* s_instance;
	MemoryManager()
	{
		a2mem = new uint8_t[_A2_MEMORY_SHADOW_END * 2];	// anything below _A2_MEMORY_SHADOW_BEGIN is unused

		if (a2mem == NULL)
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

	uint8_t* a2mem;		// The current shadowed Apple 2 memory

};

#endif	// MEMORYMANAGER_H