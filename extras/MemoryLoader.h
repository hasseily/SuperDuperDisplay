// Extras class to load a binary file into an arbitrary location in memory

#ifndef MEMORYLOADER_H
#define MEMORYLOADER_H

#include <cstdint>
#include <string>

bool MemoryLoadUsingDialog(uint32_t position, bool bAuxBank);
bool MemoryLoadHGR(std::string filePath);
bool MemoryLoadDHR(std::string filePath);
bool MemoryLoadSHR(std::string filePath);

#endif /* MEMORYLOADER_H */
