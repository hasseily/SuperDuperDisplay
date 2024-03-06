#include "MemoryManager.h"

// below because "The declaration of a static data member in its class definition is not a definition"
MemoryManager* MemoryManager::s_instance;

MemoryManager::~MemoryManager()
{
	delete[] a2mem;
}

void MemoryManager::Initialize()
{
	// Initialize the Apple 2 memory duplicate
	// Whenever memory is written from the Apple2
	// in any of the 2 banks between $200 and _A2_MEMORY_SHADOW_END it will
	// be sent through the socket and this buffer will be updated
	// memory of both banks is concatenated into one buffer
	memset(a2mem, 0, _A2_MEMORY_SHADOW_END * 2);
}

// Return a pointer to the shadowed apple 2 memory
uint8_t* MemoryManager::GetApple2MemPtr()
{
	return a2mem;
}

// Return a pointer to the shadowed apple 2 aux memory
uint8_t* MemoryManager::GetApple2MemAuxPtr()
{
	return a2mem + _A2_MEMORY_SHADOW_END;
}