// Extras class to load a binary file into an arbitrary location in memory

#ifndef MEMORYLOADER_H
#define MEMORYLOADER_H

#include <cstdint>

bool MemoryLoad(uint32_t position, bool bAuxBank);

#endif /* MEMORYLOADER_H */
