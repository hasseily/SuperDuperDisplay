#pragma once

#ifndef A2VIDEOMANAGER_H
#define A2VIDEOMANAGER_H

#include <stdint.h>
#include <stddef.h>

#include "common.h"
#include "A2Window.h"

#define _A2_TEXT40_CHAR_WIDTH 7
#define _A2_TEXT40_CHAR_HEIGHT 8

enum A2VideoMode_e
{
	A2VIDEO_TEXT1 = 0,
	A2VIDEO_TEXT2,
	A2VIDEO_LORES,
	A2VIDEO_HGR1,
	A2VIDEO_HGR2,
	A2VIDEO_DTEXT,
	A2VIDEO_DLORES,
	A2VIDEO_DHGR,
	A2VIDEO_TOTAL_COUNT
};

class A2VideoManager
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
		void AssignByFilename(A2VideoManager* owner, const char* filename);

		// image assets are full 32-bit bitmap files, uploaded from PNG
		uint32_t image_xcount = 0;	// width and height of asset in pixels
		uint32_t image_ycount = 0;
		GLuint tex_id = 0;	// Texture ID on the GPU that holds the image data
	};

	struct TileTex {				// Tile texture starting coordinates
		uint32_t upos;					// U (x) starting position
		uint32_t vpos;					// V (y) starting position
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

	// Won't need more than 3 image assets for the Apple 2 video modes
	// Probably only 1 just for the text
	ImageAsset image_assets[3];
	A2Window windows[A2VIDEO_TOTAL_COUNT];

	// Actual screen rendered output dimensions
	int rendererOutputWidth = 40 * _A2_TEXT40_CHAR_WIDTH;
	int rendererOutputHeight = 24 * _A2_TEXT40_CHAR_HEIGHT;

	// Debugging attributes
	bool bUsePerspective = false;		// see bIsUsingPerspective

	//////////////////////////////////////////////////////////////////////////
	// Methods
	//////////////////////////////////////////////////////////////////////////

	bool GetDidChangeResolution() { return bDidChangeResolution; };

	void SelectVideoMode(A2VideoMode_e mode);
	void ToggleMixedMode();
	A2VideoMode_e ActiveVideoMode();

	void Render();	// render whatever mode is active (enabled windows)

	// public singleton code
	static A2VideoManager* GetInstance()
	{
		if (NULL == s_instance)
			s_instance = new A2VideoManager();
		return s_instance;
	}
	~A2VideoManager();
private:
	//////////////////////////////////////////////////////////////////////////
	// Singleton pattern
	//////////////////////////////////////////////////////////////////////////
	void Initialize();

	static A2VideoManager* s_instance;
	A2VideoManager()
	{
		Initialize();
	}

	//////////////////////////////////////////////////////////////////////////
	// Internal methods
	//////////////////////////////////////////////////////////////////////////



	//////////////////////////////////////////////////////////////////////////
	// Internal data
	//////////////////////////////////////////////////////////////////////////
	bool bIsMixedMode = false;				// Mixed graphics and text mode
	bool bShouldInitializeRender = true;	// Used to tell the render method to run initialization
	// routines like clearing out the image assets

	bool bIsUsingPerspective = false;	// is it currently using perspective?
	bool bDidChangeResolution = false;	// did the resolution change?

	A2VideoMode_e activeVideoMode;
	GLint texSamplers[3];
};
#endif // A2VIDEOMANAGER_H

