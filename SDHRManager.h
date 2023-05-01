// Apple 2 Super Duper High Resolution
// List of commands and their structs

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <vector>

#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <SDL_opengles2.h>
#else
#include <SDL_opengl.h>
#endif

#define _SDHR_WIDTH  640
#define _SDHR_HEIGHT 360

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

struct sdhr_image
{
	int width = _SDHR_WIDTH;
	int height = _SDHR_HEIGHT;
	GLuint texture_id = 0;
};

class SDHRManager
{
public:
	void AddPacketDataToBuffer(uint8_t data);
	void ClearBuffer();
	bool ProcessCommands(void);
	void DrawWindowsIntoScreenImage();
	bool GetIsUpdatingCpuBuffer() { return isUpdatingCpuBuffer; };
	uint32_t ARGB555_to_ARGB888(uint16_t argb555);
	uint8_t* GetApple2MemPtr();	// Gets the Apple 2 memory pointer
	uint32_t* cpubuffer;

	void SetSDHRImage(sdhr_image simage) { screen_image = simage; };
	sdhr_image GetSDHRImage() { return screen_image; };

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
// Internal state structs
//////////////////////////////////////////////////////////////////////////

	struct ImageAsset {
		void AssignByFilename(const char* filename);	// currently unused
		void AssignByMemory(SDHRManager* owner, const uint8_t* buffer, uint64_t size);
		void ExtractTile(SDHRManager* owner, uint32_t* tile_p,
			uint16_t tile_xdim, uint16_t tile_ydim, 
			uint64_t xsource, uint64_t ysource);

		// image assets are full 32-bit bitmap files, uploaded from PNG
		uint64_t image_xcount = 0;
		uint64_t image_ycount = 0;
		uint8_t* data = NULL;
		ImageAsset()
			: image_xcount(0)
			, image_ycount(0)
			, data()
		{}	// Do nothing in constructor
	};

	struct TilesetRecord {
		uint64_t xdim;
		uint64_t ydim;
		uint64_t num_entries;
		uint32_t* tile_data = NULL;  // tiledata is 32-bit RGBA
		TilesetRecord()
			: xdim(0)
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
		uint8_t* tilesets = NULL;
		uint8_t* tile_indexes = NULL;
		Window()
			: enabled(0), black_or_wrap(false)
			, screen_xcount(0), screen_ycount(0)
			, screen_xbegin(0), screen_ybegin(0)
			, tile_xbegin(0), tile_ybegin(0)
			, tile_xdim(0), tile_ydim(0)
			, tile_xcount(0), tile_ycount(0)
			, tilesets(), tile_indexes()
		{}
	};

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

	sdhr_image screen_image;

	std::vector<uint8_t> command_buffer;
	bool error_flag;
	char error_str[256];
	uint8_t uploaded_data_region[256 * 256 * 256];
	ImageAsset image_assets[256];
	TilesetRecord tileset_records[256];
	Window windows[256];
	bool isUpdatingCpuBuffer = false;	// set to true when the thread is updating cpubuffer
};
