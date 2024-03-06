#pragma once

#ifndef A2VIDEOMANAGER_H
#define A2VIDEOMANAGER_H

#include <stdint.h>
#include <stddef.h>
#include <mutex>

#include "common.h"
#include "A2WindowBeam.h"

// Legacy mode VRAM is 4 bytes (main, aux, flags, colors)
// for each "byte" of screen use
// colors are 4-bit each of fg and bg colors as in the Apple 2gs

constexpr uint32_t _BEAM_VRAM_SIZE_LEGACY = 40 * 192 * 4;

// SHR mode VRAM is the standard bytes of screen use ($2000 to $9CFF)
// plus, for each of the 200 lines, at the beginning of the line draw
// (at end of HBLANK) the SCB (1 byte) and palette (32 bytes) of that line
// SHR mode VRAM looks like this:

// SCB        PALETTE              COLOR BYTES
// [0 [(1 2) (3 4) ... (31 32)] [0 ......... 159]]	// line 0
// [0 [(1 2) (3 4) ... (31 32)] [0 ......... 159]]	// line 1
//                         .
//                         .
//                         .
//                         .
// [0 [(1 2) (3 4) ... (31 32)] [0 ......... 159]]	// line 199

constexpr uint32_t _BEAM_VRAM_SIZE_SHR = (1 + 32 + 160) * 200;

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

	ImageAsset image_assets[8];
	A2WindowBeam windowsbeam[A2VIDEOBEAM_TOTAL_COUNT];	// beam racing GPU render

	// Margins when rendering in a window (pixels)
	int windowMargins = 10;

	uint32_t color_border = 0;
	uint32_t color_foreground = UINT32_MAX;
	uint32_t color_background = 0;
    bool bShouldReboot = false;             // When an Appletini reboot packet arrives

	//////////////////////////////////////////////////////////////////////////
	// Methods
	//////////////////////////////////////////////////////////////////////////

	bool IsReady();		// true after full initialization
	void ToggleA2Video(bool value);
	uXY ScreenSize();

	// Methods for the single multipurpose beam racing shader
	void BeamIsAtPosition(uint32_t x, uint32_t y);
	void RequestVRAMUpdates(bool cycleHasLegacy, bool cycleHasSHR);
	void ForceBeamFullScreenRender();
	
	uint8_t* GetLegacyVRAMPtr() { return a2legacy_vram; };
	uint8_t* GetSHRVRAMPtr() { return a2shr_vram; };
	void ActivateBeam();	// The apple 2 is rendering!
	void DeactivateBeam();	// We don't have a connection to the Apple 2!
	bool ShouldRender();
	void Render();	// render whatever mode is active (enabled windows)

	// public singleton code
	static A2VideoManager* GetInstance()
	{
		if (NULL == s_instance)
			s_instance = new A2VideoManager();
		return s_instance;
	}
	~A2VideoManager();
    
	void Initialize();
	void ResetComputer();
private:
	static A2VideoManager* s_instance;
	A2VideoManager()
	{
		a2legacy_vram = new uint8_t[_BEAM_VRAM_SIZE_LEGACY];
		if (a2legacy_vram == NULL)
			std::cerr << "FATAL ERROR: COULD NOT ALLOCATE a2legacy_vram MEMORY" << std::endl;
		a2shr_vram = new uint8_t[_BEAM_VRAM_SIZE_SHR];
		if (a2shr_vram == NULL)
			std::cerr << "FATAL ERROR: COULD NOT ALLOCATE a2shr_vram MEMORY" << std::endl;
		Initialize();
	}

	//////////////////////////////////////////////////////////////////////////
	// Internal data
	//////////////////////////////////////////////////////////////////////////
	bool bIsReady = false;
	bool bA2VideoEnabled = true;			// Is standard Apple 2 video enabled?
	bool bShouldInitializeRender = true;	// Used to tell the render method to run initialization
    bool bIsRebooting = false;              // Rebooting semaphore

	// beam render state variables
	mutable std::mutex a2video_mutex;
	bool bRequestVRAMUpdates = true;		// Requests beam rendering from the main thread
	bool bVBlankHasLegacy = true;			// Does this vblank cycle have some legacy dots?
	bool bVBlankHasSHR = false;				// Does this vblank cycle have some shr dots?
	bool bBeamRenderLegacy = true;			// Should we render legacy of the previous vblank cycle?
	bool bBeamRenderSHR = false;			// Should we render shr of the previous vblank cycle?
	bool bBeamIsActive = false;				// Is the beam active?
	
	// vram for beam renderers
	uint8_t* a2legacy_vram;
	uint8_t* a2shr_vram;

};
#endif // A2VIDEOMANAGER_H

