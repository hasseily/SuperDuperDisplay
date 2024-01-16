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
	A2VIDEO_LGR1,
	A2VIDEO_LGR2,
	A2VIDEO_HGR1,
	A2VIDEO_HGR2,
	A2VIDEO_DTEXT,
	A2VIDEO_DLGR,
	A2VIDEO_DHGR,
	A2VIDEO_SHR,
	A2VIDEO_TOTAL_COUNT
};

/*
MEMORY MANAGEMENT SOFT SWITCHES
 $C000   W       80STOREOFF      Allow page2 to switch video page1 page2
 $C001   W       80STOREON       Allow page2 to switch main & aux video memory
 $C002   W       RAMRDOFF        Read enable main memory from $0200-$BFFF
 $C003   W       RAMRDON         Read enable aux memory from $0200-$BFFF
 $C004   W       RAMWRTOFF       Write enable main memory from $0200-$BFFF
 $C005   W       RAMWRTON        Write enable aux memory from $0200-$BFFF
 $C006   W       INTCXROMOFF     Enable slot ROM from $C100-$C7FF (but $C800-$CFFF depends on INTC8ROM)
 $C007   W       INTCXROMON      Enable main ROM from $C100-$CFFF
 $C008   W       ALTZPOFF        Enable main memory from $0000-$01FF & avl BSR
 $C009   W       ALTZPON         Enable aux memory from $0000-$01FF & avl BSR
 $C00A   W       SLOTC3ROMOFF    Enable main ROM from $C300-$C3FF
 $C00B   W       SLOTC3ROMON     Enable slot ROM from $C300-$C3FF
 $C07E   W       IOUDIS          [//c] On: disable IOU access for addresses $C058 to $C05F; enable access to DHIRES switch
 $C07F   W       IOUDIS          [//c] Off: enable IOU access for addresses $C058 to $C05F; disable access to DHIRES switch

VIDEO SOFT SWITCHES
 $C00C   W       80COLOFF        Turn off 80 column display
 $C00D   W       80COLON         Turn on 80 column display
 $C00E   W       ALTCHARSETOFF   Turn off alternate characters
 $C00F   W       ALTCHARSETON    Turn on alternate characters
 $C021   R/W     MONOCOLOR       [IIgs] Bit 7 on: Greyscale
 $C022   R/W     SCREENCOLOR     [IIgs] text foreground and background colors (also VidHD)
 $C029   R/W     NEWVIDEO        [IIgs] Select new video modes (also VidHD)
 $C034   R/W     BORDERCOLOR     [IIgs] b3:0 are border color (also VidHD)
 $C035   R/W     SHADOW          [IIgs] auxmem-to-bank-E1 shadowing (also VidHD)
 $C050   R/W     TEXTOFF         Select graphics mode
 $C051   R/W     TEXTON          Select text mode
 $C052   R/W     MIXEDOFF        Use full screen for graphics
 $C053   R/W     MIXEDON         Use graphics with 4 lines of text
 $C054   R/W     PAGE2OFF        Select panel display (or main video memory)
 $C055   R/W     PAGE2ON         Select page2 display (or aux video memory)
 $C056   R/W     HIRESOFF        Select low resolution graphics
 $C057   R/W     HIRESON         Select high resolution graphics
 $C05E   R/W     DHIRESON        Select double (14M) resolution graphics (DLGR or DHGR)
 $C05F   R/W     DHIRESOFF       Select single (7M) resolution graphics
*/
enum A2SoftSwitch_e
{
	A2SS_80STORE	= 0b000000000000001,
	A2SS_RAMRD		= 0b000000000000010,
	A2SS_RAMWRT		= 0b000000000000100,
	A2SS_80COL		= 0b000000000001000,
	A2SS_ALTCHARSET = 0b000000000010000,
	A2SS_INTCXROM	= 0b000000000100000,
	A2SS_SLOTC3ROM	= 0b000000001000000,
	A2SS_TEXT		= 0b000000010000000,
	A2SS_MIXED		= 0b000000100000000,
	A2SS_PAGE2		= 0b000001000000000,
	A2SS_HIRES		= 0b000010000000000,
	A2SS_DHGR		= 0b000100000000000,
	A2SS_DHGRMONO	= 0b001000000000000,
    A2SS_SHR        = 0b010000000000000,
	A2SS_GREYSCALE  = 0b100000000000000,
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

	ImageAsset image_assets[5];
	A2Window windows[A2VIDEO_TOTAL_COUNT];

	bool bShowScanLines = true;

	// Margins when rendering in a window (pixels)
	int windowMargins = 30;

	uint32_t color_border = 0;
	uint32_t color_foreground = UINT32_MAX;
	uint32_t color_background = 0;
    bool bShouldReboot = false;             // When an Appletini reboot packet arrives

	//////////////////////////////////////////////////////////////////////////
	// Methods
	//////////////////////////////////////////////////////////////////////////

	void NotifyA2MemoryDidChange(uint16_t addr);	// Apple 2's memory changed at addr
	void ToggleA2Video(bool value);
	void SelectVideoModes();			// Based on soft switches, decided on video modes
	uXY ScreenSize() { return windows[activeVideoMode].Get_screen_count(); }
	void ProcessSoftSwitch(uint16_t addr, uint8_t val, bool rw, bool is_iigs);

	void Render();	// render whatever mode is active (enabled windows)

	// public singleton code
	static A2VideoManager* GetInstance()
	{
		if (NULL == s_instance)
			s_instance = new A2VideoManager();
		return s_instance;
	}
	~A2VideoManager();

	inline static bool IsSoftSwitch(A2SoftSwitch_e ss) { return (a2SoftSwitches & ss); };
    static void SetSoftSwitch(A2SoftSwitch_e ss, bool state)
    {
        if (state)
            a2SoftSwitches |= ss;
        else
            a2SoftSwitches &= ~ss;
        A2VideoManager::GetInstance()->SelectVideoModes();
    }
    
	void ResetComputer();
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

	// Renders graphics mode depending on mixed mode switch
	void RenderSubMixed(std::vector<uint32_t>*framebuffer);

	void UpdateLoResRGBCell(uint16_t addr, const uint16_t addr_start, std::vector<uint32_t>* framebuffer);
	void UpdateDLoResRGBCell(uint16_t addr, const uint16_t addr_start, std::vector<uint32_t>* framebuffer);
	void UpdateHiResRGBCell(uint16_t addr, const uint16_t addr_start, std::vector<uint32_t>* framebuffer);
	void UpdateDHiResRGBCell(uint16_t addr, const uint16_t addr_start, std::vector<uint32_t>* framebuffer);
	void UpdateSHRLine(uint8_t line_number, std::vector<uint32_t>* framebuffer);
	
	uint32_t ConvertIIgs2RGB(uint16_t color);
	//////////////////////////////////////////////////////////////////////////
	// Internal data
	//////////////////////////////////////////////////////////////////////////
	bool bA2VideoEnabled = true;			// Is standard Apple 2 video enabled?
	bool bShouldInitializeRender = true;	// Used to tell the render method to run initialization
    bool bIsRebooting = false;              // Rebooting semaphore
	static uint16_t a2SoftSwitches;			// Soft switches states
    
	// framebuffers for graphics modes
	std::vector<uint32_t>v_fblgr1;
	std::vector<uint32_t>v_fblgr2;
	std::vector<uint32_t>v_fbdlgr;
	std::vector<uint32_t>v_fbhgr1;
	std::vector<uint32_t>v_fbhgr2;
	std::vector<uint32_t>v_fbdhgr;
	std::vector<uint32_t>v_fbshr;

	A2VideoMode_e activeVideoMode = A2VIDEO_TEXT1;
};
#endif // A2VIDEOMANAGER_H

