// Extras functions to load a binary file into an arbitrary location in memory
// Also functions to save a binary file into a location on the filesystem

#ifndef MEMORYLOADER_H
#define MEMORYLOADER_H

#include <cstdint>
#include <string>

bool MemoryLoadUsingDialog(uint32_t position, bool bAuxBank, std::string& path);
bool MemoryLoad(const std::string &filePath, uint32_t position, bool bAuxBank, size_t fileSize = 0);
bool MemoryLoadLGR(const std::string &filePath);
bool MemoryLoadDGR(const std::string &filePath);
bool MemoryLoadHGR(const std::string &filePath);
bool MemoryLoadDHR(const std::string &filePath);
bool MemoryLoadSHR(const std::string &filePath);
std::string GetMemorySaveFilePath();	// Returns a fully formatted file path for saving, no suffix
// the save methods auto-append the correct suffix (.hgr, .dgr, .shr)
bool MemorySaveLGR(const std::string& filePath, size_t fileSize = 0x400);	// 0x800 for interlace/pageflip
bool MemorySaveDGR(const std::string& filePath, size_t fileSize = 0x800);	// 0x1000 for interlace/pageflip
bool MemorySaveHGR(const std::string& filePath, size_t fileSize = 0x2000);	// 0x4000 for interlace/pageflip
bool MemorySaveDHR(const std::string& filePath, size_t fileSize = 0x4000);	// 0x8000 for interlace/pageflip
bool MemorySaveSHR(const std::string& filePath, size_t fileSize = 0x80000);	// 0x10000 for interlace/pageflip

#endif /* MEMORYLOADER_H */
