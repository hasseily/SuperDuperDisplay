#pragma once

#ifndef A2VIDEOMANAGER_H
#define A2VIDEOMANAGER_H

#include <stdint.h>
#include <stddef.h>

#include "common.h"
#include "A2Window.h"

enum A2VideoMode_e
{
	A2VIDEO_TEXT1 = 0,
	A2VIDEO_TEXT2,
	A2VIDEO_LORES1,
	A2VIDEO_LORES2,
	A2VIDEO_HGR1,
	A2VIDEO_HGR2,
	A2VIDEO_DTEXT,
	A2VIDEO_DLORES,
	A2VIDEO_DHGR,
	A2VIDEO_TOTAL_COUNT
};

enum A2SoftSwitch_e
{
	A2SS_80STORE	= 0b000000001,
	A2SS_RAMRD		= 0b000000010,
	A2SS_RAMWRT		= 0b000000100,
	A2SS_80COL		= 0b000001000,
	A2SS_ALTCHARSET = 0b000010000,
	A2SS_TEXT		= 0b000100000,
	A2SS_MIXED		= 0b001000000,
	A2SS_PAGE2		= 0b010000000,
	A2SS_HIRES		= 0b100000000,
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

	//////////////////////////////////////////////////////////////////////////
	// Attributes
	//////////////////////////////////////////////////////////////////////////

	// Won't need more than 3 image assets for the Apple 2 video modes
	// Probably only 1 just for the text
	ImageAsset image_assets[3];
	A2Window windows[A2VIDEO_TOTAL_COUNT];

	// Margins when rendering in a window (pixels)
	int windowMargins = 30;

	//////////////////////////////////////////////////////////////////////////
	// Methods
	//////////////////////////////////////////////////////////////////////////

	void NotifyA2MemoryDidChange(uint16_t addr);	// Apple 2's memory changed at addr
	void ToggleA2Video(bool value);
	void SelectVideoMode(A2VideoMode_e mode);
	void ToggleMixedMode();
	A2VideoMode_e ActiveVideoMode();
	uXY ScreenSize() { return windows[activeVideoMode].Get_screen_count(); }
	void ProcessSoftSwitch(uint16_t addr);
	bool IsSoftSwitch(A2SoftSwitch_e ss);

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
	bool bA2VideoEnabled = true;			// Is standard Apple 2 video enabled?
	bool bIsMixedMode = false;				// Mixed graphics and text mode
	bool bShouldInitializeRender = true;	// Used to tell the render method to run initialization

	uint16_t a2SoftSwitches = 0;			// Soft switches states

	A2VideoMode_e activeVideoMode = A2VIDEO_TEXT1;
};
#endif // A2VIDEOMANAGER_H

