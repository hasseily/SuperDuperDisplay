// Extras functions to load a binary file into an arbitrary location in memory
// Also functions to save a binary file into a location on the filesystem

#ifndef MEMORYLOADER_H
#define MEMORYLOADER_H

#include <cstdint>
#include <string>

enum class SHRFileContent_e
{
	UNKNOWN = 0,			// Unknown, can't parse
	SHR,					// 0x8000: Standard SHR
	SHR4,					// 0x8000: SHR4
	SHR3200,				// 0x9900: SHR3200 (Brooks) with 200 palettes
	SHR_BYTES,				// 0x7D00: Raw SHR bytes (no SCB or palettes)
	TOTAL_COUNT
};

// Optional interpretation for files selected in the memory loader. AUTO keeps
// extension-based loading and falls back to the user-entered raw address. Every
// other entry forces both the video mode and its conventional load address.
enum class MemoryLoadFormat_e
{
	AUTO = 0,
	TEXT,
	DTEXT,
	LGR,
	DLGR,
	HGR,
	HGR_SPEC1,
	HGR_SPEC2,
	DHGR,
	DHGR_MONO,
	DHGR_COL140_MIXED,
	DHGR160,
	SHR,
	SHR3200,
	SHR4_SHR,
	SHR4_RGGB,
	SHR4_PAL256,
	SHR4_PAL256I,
	SHR4_R4G4B4,
	TOTAL_COUNT
};

uint32_t GetMemoryLoadStart(MemoryLoadFormat_e format);
bool MemoryLoadUsingDialog(uint32_t position, bool bAuxBank, std::string& path,
	MemoryLoadFormat_e format = MemoryLoadFormat_e::AUTO);
bool MemoryLoad(const std::string &filePath, uint32_t position, bool bAuxBank, size_t fileSize = 0);
bool MemoryLoadWithFormat(const std::string& filePath, MemoryLoadFormat_e format,
	uint32_t rawPosition = 0, bool rawAuxBank = false);
bool MemoryLoadLGR(const std::string &filePath);
bool MemoryLoadDGR(const std::string &filePath);
bool MemoryLoadHGR(const std::string &filePath);
bool MemoryLoadDHR(const std::string &filePath);
bool MemoryLoadSHR(const std::string &filePath);
// Loads an SHR file starting at offset. Returns if parsed bytes, and the content loaded in E1 (aux) and E0 (main)
uint32_t ParseSHRData(std::ifstream& file, uint32_t offset, SHRFileContent_e* typeE1, SHRFileContent_e* typeE0);
// Returns a fully formatted file path for saving, no suffix
std::string GetMemorySaveFilePath();
// The save methods append the specified legacy suffix, using the *i form for page pairs.
bool MemorySaveLGR(const std::string& filePath, size_t fileSize = 0x400);	// 0x800 for interlace/pageflip
bool MemorySaveDGR(const std::string& filePath, size_t fileSize = 0x800);	// .dlr, or .dlri for 0x1000
bool MemorySaveHGR(const std::string& filePath, size_t fileSize = 0x2000);	// 0x4000 for interlace/pageflip
bool MemorySaveDHR(const std::string& filePath, size_t fileSize = 0x4000);	// 0x8000 for interlace/pageflip
bool MemorySaveSHR(const std::string& filePath, size_t fileSize = 0x8000);	// 0x10000 for interlace/pageflip

#endif /* MEMORYLOADER_H */
