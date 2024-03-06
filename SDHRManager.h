// Apple 2 Super Duper High Resolution
// List of commands and their structs

#pragma once
#ifndef SDHRMANAGER_H
#define SDHRMANAGER_H

#include <stdint.h>
#include <stddef.h>
#include <queue>

#include "common.h"
#include "OpenGLHelper.h"
#include "SDHRWindow.h"

enum DATASTATE_e
{
	DATA_IDLE = 0,			// CPU and GPU are in sync
	DATA_UPDATED			// Updated data in memory. Needs sync to GPU
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
	SDHR_CMD_UPDATE_WINDOW_ENABLE = 13,
	SDHR_CMD_UPLOAD_DATA_FILENAME = 15,			// NOT RELEVANT, NOT IMPLEMENTED
	SDHR_CMD_UPDATE_WINDOW_SET_UPLOAD = 16,

	SDHR_CMD_CHANGE_RESOLUTION = 50,
	SDHR_CMD_UPDATE_WINDOW_SET_SIZE = 51,
	SDHR_CMD_UPDATE_WINDOW_DISPLAY_IMAGE = 52,
};

class SDHRManager
{
public:

//////////////////////////////////////////////////////////////////////////
// SDHR state structs
//////////////////////////////////////////////////////////////////////////

	// NOTE:	Anything labled "id" is an internal identifier by the GPU
	//			Anything labled "index" is an actual array or vector index used by the code
	
	// An image asset is a texture with its metadata (width, height)
	// The actual texture data is in the GPU memory
	struct ImageAsset {
		void AssignByFilename(SDHRManager* owner, const char* filename);
		void AssignByMemory(SDHRManager* owner, const uint8_t* buffer, int size);
		// This method to be called by the main thread only
		void LoadIntoGPU();

		// image assets are full 32-bit bitmap files, uploaded from PNG
		int image_xcount = 0;	// width and height of asset in pixels
		int image_ycount = 0;
		GLuint tex_id = 0;	// Texture ID on the GPU that holds the image data
		int channels = 3;
		unsigned char* data = nullptr;
	};

	struct TileTex {				// Tile texture starting coordinates
		uint32_t upos;					// U (x) starting position
		uint32_t vpos;					// V (y) starting position
		// TODO: Add flags (inverted, mirrored, ...)
	};

	struct TilesetRecord {			// A single tileset
		uint8_t asset_index;			// index of the image asset
		uint16_t xdim;					// Width of tiles in this tileset
		uint16_t ydim;					// Height of tiles in this tileset
		uint32_t num_entries;
		TileTex* tile_data = NULL;		// list of tile texture starting coordinates
		TilesetRecord()
			: asset_index(0)
			, xdim(0)
			, ydim(0)
			, num_entries(0)
			, tile_data()
		{}
	};

	//////////////////////////////////////////////////////////////////////////
	// Attributes
	//////////////////////////////////////////////////////////////////////////

	volatile DATASTATE_e dataState;
	// NOTE:	Maximum of 16 image assets.
	//			They're always concomitantly available as textures in the GPU
	ImageAsset image_assets[_SDHR_MAX_TEXTURES];
	TilesetRecord tileset_records[_SDHR_MAX_WINDOWS];
	SDHRWindow windows[_SDHR_MAX_WINDOWS];

	// Margins when rendering in a window (pixels)
	int windowMargins = 0;


	//////////////////////////////////////////////////////////////////////////
	// Methods
	//////////////////////////////////////////////////////////////////////////

	void AddPacketDataToBuffer(uint8_t data);
	void ClearBuffer();
	bool ProcessCommands(void);

	uint8_t* GetUploadRegionPtr();

	void Render();	// render everything SDHR related

	TileTex* GetTilesetRecordData(uint8_t tileset_index) { return tileset_records[tileset_index].tile_data; };
	TileTex GetTilesetTileTex(uint8_t tileset_index, uint8_t tile_index) { return tileset_records[tileset_index].tile_data[tile_index]; };

	void ToggleSdhr(bool value) {
		bSDHREnabled = value;
		if (bSDHREnabled)
			bShouldInitializeRender = true;
		else
			ResetSdhr();
	}

	bool IsSdhrEnabled(void) {
		return bSDHREnabled;
	}

	void ResetSdhr();

	// public singleton code
	static SDHRManager* GetInstance()
	{
		if (NULL == s_instance)
			s_instance = new SDHRManager();
		return s_instance;
	}
	void Initialize();
	~SDHRManager();
private:
//////////////////////////////////////////////////////////////////////////
// Singleton pattern
//////////////////////////////////////////////////////////////////////////

	static SDHRManager* s_instance;
	SDHRManager()
	{
		uploaded_data_region = new uint8_t[_SDHR_UPLOAD_REGION_SIZE];

		if (uploaded_data_region == NULL)
			std::cerr << "FATAL ERROR: COULD NOT ALLOCATE uploaded_data_region MEMORY" << std::endl;
		Initialize();
	}

	//////////////////////////////////////////////////////////////////////////
	// Internal methods
	//////////////////////////////////////////////////////////////////////////
	void CommandError(const char* err);
	bool CheckCommandLength(uint8_t* p, uint8_t* e, size_t sz);
	uint32_t DataOffset(uint8_t low, uint8_t med, uint8_t high) {
		return (uint32_t)high * 256 * 256 + (uint32_t)med * 256 + low;
	}
	bool DataSizeCheck(uint32_t offset, uint32_t data_size) {
		if (offset + data_size >= _SDHR_UPLOAD_REGION_SIZE) {
			CommandError("data not bounded by uploaded data region");
			return false;
		}
		return true;
	}

	void DefineTileset(uint8_t tileset_index, uint16_t num_entries, uint16_t xdim, uint16_t ydim,
		uint8_t asset_index, uint8_t* offsets);


//////////////////////////////////////////////////////////////////////////
// Internal data
//////////////////////////////////////////////////////////////////////////
	bool bShouldInitializeRender = true;	// Used to tell the render method to run initialization
											// routines like clearing out the image assets

	bool bSDHREnabled = false;	// is SDHR enabled?

	std::vector<uint8_t> command_buffer;
	bool error_flag = false;
	char error_str[256];
	uint8_t* uploaded_data_region = NULL;	// A region of 256 * 256 * 256 bytes (16MB)

	// This is a FIFO queue where the network thread tells the main thread that
	// there's image data that needs to be uploaded to the GPU
	// It's only FIFO in name because we don't allow more than one entry in there
	// The queue not being empty acts as a semaphore.
	struct UploadImageData {
		uint8_t asset_index;
		uint32_t upload_start_addr;
		int upload_data_size;
	};
	std::queue<UploadImageData>fifo_upload_image_data;
};
#endif // SDHRMANAGER_H
