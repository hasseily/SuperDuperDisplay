// Extras funbctions to load a binary file into an arbitrary location in memory

#ifndef MEMORYLOADER_H
#define MEMORYLOADER_H

#include <cstdint>
#include <string>

bool MemoryLoadUsingDialog(uint32_t position, bool bAuxBank);
bool MemoryLoad(const std::string &filePath, uint32_t position, bool bAuxBank);
bool MemoryLoadHGR(const std::string &filePath);
bool MemoryLoadDHR(const std::string &filePath);
bool MemoryLoadSHR(const std::string &filePath);

#endif /* MEMORYLOADER_H */
