// Apple 2 Super Duper High Resolution
// List of commands and their structs

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <vector>

#include "common.h"
#include "mosaicmesh.h"

#define _SDHR_WIDTH  640
#define _SDHR_HEIGHT 360

enum THREADCOMM_e
{
	IDLE = 0,			// SDHR data and OpenGL are in sync
	SOCKET_LOCK,		// Socket thread is updating the SDHR data
	COMMAND_PROCESSED,	// Socket thread has processed a command batch, waiting for Main thread to work
	MAIN_LOCK			// Main thread is updating OpenGL textures
};

enum SDHRCtrl_e
{
	SDHR_CTRL_DISABLE = 0,
	SDHR_CTRL_ENABLE,
	SDHR_CTRL_PROCESS,
	SDHR_CTRL_RESET
};

enum SDHRCmd_e {
	SDHR_CMD_UPLOAD_DATA = 1,
	SDHR_CMD_DEFINE_IMAGE_ASSET = 2,
	SDHR_CMD_DEFINE_IMAGE_ASSET_FILENAME = 3,	// NOT RELEVANT, NOT IMPLEMENTED
	SDHR_CMD_DEFINE_TILESET = 4,
	SDHR_CMD_DEFINE_TILESET_IMMEDIATE = 5,
	SDHR_CMD_DEFINE_WINDOW = 6,
	SDHR_CMD_UPDATE_WINDOW_SET_IMMEDIATE = 7,
	// SDHR_CMD_UPDATE_WINDOW_SINGLE_TILESET = 8,
	SDHR_CMD_UPDATE_WINDOW_SHIFT_TILES = 9,
	SDHR_CMD_UPDATE_WINDOW_SET_WINDOW_POSITION = 10,
	SDHR_CMD_UPDATE_WINDOW_ADJUST_WINDOW_VIEW = 11,
	SDHR_CMD_UPDATE_WINDOW_SET_BITMASKS = 12,
	SDHR_CMD_UPDATE_WINDOW_ENABLE = 13,
	SDHR_CMD_READY = 14,
	SDHR_CMD_UPLOAD_DATA_FILENAME = 15,			// NOT RELEVANT, NOT IMPLEMENTED
	SDHR_CMD_UPDATE_WINDOW_SET_UPLOAD = 16,
};

struct bgra_t
{
	uint8_t b;
	uint8_t g;
	uint8_t r;
	uint8_t a;
};

class SDHRManager
{
public:

//////////////////////////////////////////////////////////////////////////
// state structs
//////////////////////////////////////////////////////////////////////////

	// An image asset is a texture with its metadata (width, height)
	// The actual texture data is in the GPU memory
	struct ImageAsset {
		void AssignByFilename(SDHRManager* owner, const char* filename);
		void AssignByMemory(SDHRManager* owner, const uint8_t* buffer, uint64_t size);

		// image assets are full 32-bit bitmap files, uploaded from PNG
		uint64_t image_xcount = 0;	// width and height of asset
		uint64_t image_ycount = 0;
		GLuint tex_id = UINT_MAX;	// Texture ID on the GPU that holds the image data
		ImageAsset()
			: image_xcount(0)
			, image_ycount(0)
			, tex_id(UINT_MAX)
		{}	// Do nothing in constructor
	};

	struct TilesetRecord {			// A single tileset
		GLuint tex_id;					// texture id of the image asset
		uint64_t xdim;					// Width of tiles in this tileset
		uint64_t ydim;					// Height of tiles in this tileset
		uint64_t num_entries;
		TileTex* tile_data = NULL;		// list of tile texture starting coordinates
		TilesetRecord()
			: tex_id(UINT_MAX)
			, xdim(0)
			, ydim(0)
			, num_entries(0)
			, tile_data()
		{}
	};

	struct Window {
		uint8_t enabled;
		bool black_or_wrap;      // false: viewport is black outside of tile range, true: viewport wraps
		uint64_t screen_xcount;  // width in pixels of visible screen area of window
		uint64_t screen_ycount;
		int64_t screen_xbegin;   // pixel xy coordinate where window begins
		int64_t screen_ybegin;
		int64_t tile_xbegin;     // pixel xy coordinate on backing tile array where aperture begins
		int64_t tile_ybegin;
		uint64_t tile_xdim;      // xy dimension, in pixels, of tiles in the window.
		uint64_t tile_ydim;
		uint64_t tile_xcount;    // xy dimension, in tiles, of the tile array
		uint64_t tile_ycount;
		uint8_t* tileset_indexes = NULL;
		uint8_t* tile_indexes = NULL;
		Window()
			: enabled(0), black_or_wrap(false)
			, screen_xcount(0), screen_ycount(0)
			, screen_xbegin(0), screen_ybegin(0)
			, tile_xbegin(0), tile_ybegin(0)
			, tile_xdim(0), tile_ydim(0)
			, tile_xcount(0), tile_ycount(0)
			, tileset_indexes(), tile_indexes()
		{}
	};

	//////////////////////////////////////////////////////////////////////////
	// Attributes
	//////////////////////////////////////////////////////////////////////////

	uint32_t* cpubuffer;
	bool shouldUseCpuBuffer = false;	// Disables the temp cpu buffer
	bool shouldUseSubImage2D = true;	// Disables the pixel-by-pixel subimage2d upload
	THREADCOMM_e threadState;
	// NOTE:	Maximum of 16 image assets.
	//			They're always concomitantly available as textures in the GPU
	ImageAsset image_assets[_SDHR_MAX_TEXTURES];
	TilesetRecord tileset_records[256];
	Window windows[256];

	//////////////////////////////////////////////////////////////////////////
	// Methods
	//////////////////////////////////////////////////////////////////////////

	void AddPacketDataToBuffer(uint8_t data);
	void ClearBuffer();
	bool ProcessCommands(void);
	void DrawWindowsIntoScreenImage(GLuint textureid);
	uint8_t* GetApple2MemPtr();	// Gets the Apple 2 memory pointer

	uint8_t* GetTilesetRecordData(uint8_t index) { return reinterpret_cast<uint8_t*>(tileset_records[index].tile_data); };

	void ToggleSdhr(bool value) {
		m_bEnabled = value;
	}

	bool IsSdhrEnabled(void) {
		return m_bEnabled;
	}

	void ResetSdhr() {
		Initialize();
	}
	// public singleton code
	static SDHRManager* GetInstance()
	{
		if (NULL == s_instance)
			s_instance = new SDHRManager();
		return s_instance;
	}
	~SDHRManager();
private:
//////////////////////////////////////////////////////////////////////////
// Singleton pattern
//////////////////////////////////////////////////////////////////////////
	void Initialize();

	static SDHRManager* s_instance;
	SDHRManager()
	{
		Initialize();
	}

	//////////////////////////////////////////////////////////////////////////
	// Internal methods
	//////////////////////////////////////////////////////////////////////////
	void CommandError(const char* err);
	bool CheckCommandLength(uint8_t* p, uint8_t* e, size_t sz);
	uint64_t DataOffset(uint8_t low, uint8_t med, uint8_t high) {
		return (uint64_t)high * 256 * 256 + (uint64_t)med * 256 + low;
	}
	bool DataSizeCheck(uint64_t offset, uint64_t data_size) {
		if (offset + data_size >= sizeof(uploaded_data_region)) {
			CommandError("data not bounded by uploaded data region");
			return false;
		}
		return true;
	}

	void DefineTileset(uint8_t tileset_index, uint16_t num_entries, uint8_t xdim, uint8_t ydim,
		ImageAsset* asset, uint8_t* offsets);


//////////////////////////////////////////////////////////////////////////
// Internal data
//////////////////////////////////////////////////////////////////////////
	uint8_t* a2mem;	// The current state of the Apple 2 memory ($0200-$BFFF)
	
	bool m_bEnabled;

	static const uint16_t screen_xcount = _SDHR_WIDTH;
	static const uint16_t screen_ycount = _SDHR_HEIGHT;

	std::vector<uint8_t> command_buffer;
	bool error_flag;
	char error_str[256];
	uint8_t uploaded_data_region[256 * 256 * 256];
};
