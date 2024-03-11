#pragma once

#ifndef A2VIDEOMANAGER_H
#define A2VIDEOMANAGER_H

#include <stdint.h>
#include <stddef.h>

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

	// We'll create 2 BeamRenderVRAMs objects, for double buffering
	struct BeamRenderVRAMs {
		uint64_t frame_idx = 0;
		bool use_legacy = true;
		bool use_shr = false;
		uint8_t vram_legacy[_BEAM_VRAM_SIZE_LEGACY];
		uint8_t vram_shr[_BEAM_VRAM_SIZE_SHR];
	};

	//////////////////////////////////////////////////////////////////////////
	// Attributes
	//////////////////////////////////////////////////////////////////////////

	ImageAsset image_assets[8];
	A2WindowBeam windowsbeam[A2VIDEOBEAM_TOTAL_COUNT];	// beam racing GPU render

	// Margins when rendering in a window (pixels)
	int windowMargins = 10;

	uint64_t current_frame_idx = 0;
	uint64_t rendered_frame_idx = UINT64_MAX;

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
	void ForceBeamFullScreenRender();
	
	const uint8_t* GetLegacyVRAMReadPtr() { return vrams_read->vram_legacy; };
	const uint8_t* GetSHRVRAMReadPtr() { return vrams_read->vram_shr; };
	uint8_t* GetLegacyVRAMWritePtr() { return vrams_write->vram_legacy; };
	uint8_t* GetSHRVRAMWritePtr() { return vrams_write->vram_shr; };
	void ActivateBeam();	// The apple 2 is rendering!
	void DeactivateBeam();	// We don't have a connection to the Apple 2!
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
		vrams_array = new BeamRenderVRAMs[2];
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
	bool bBeamIsActive = false;				// Is the beam active?

	// Double-buffered vrams
	BeamRenderVRAMs* vrams_array;	// 2 buffers of legacy+shr vrams
	BeamRenderVRAMs* vrams_write;	// the write buffer
	BeamRenderVRAMs* vrams_read;	// the read buffer
};
#endif // A2VIDEOMANAGER_H

