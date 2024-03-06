#include "A2VideoManager.h"
#include <cstring>
#include <zlib.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <algorithm>
#include <map>
#include "SDL.h"
#ifdef _DEBUGTIMINGS
#include <chrono>
#endif

#include "OpenGLHelper.h"
#include "MemoryManager.h"
#include "CycleCounter.h"
#include "EventRecorder.h"
#include "GRAddr2XY.h"

static inline uint32_t SETRGBCOLOR(uint8_t r, uint8_t g, uint8_t b)
{
	return ((0xFF << 24) | (b << 16) | (g << 8) | r);
}

static uint32_t gPaletteRGB[] =
{
	// HiRes
	SETRGBCOLOR(/*HGR_BLACK, */ 0x00,0x00,0x00),
	SETRGBCOLOR(/*HGR_WHITE, */ 0xFF,0xFF,0xFF),
	SETRGBCOLOR(/*BLUE,      */ 0x00,0x8A,0xB5),
	SETRGBCOLOR(/*ORANGE,    */ 0xFF,0x72,0x47),
	SETRGBCOLOR(/*GREEN,     */ 0x6F,0xE6,0x2C),
	SETRGBCOLOR(/*MAGENTA,   */ 0xAA,0x1A,0xD1),

	// TV emu
	SETRGBCOLOR(/*HGR_GREY1, */ 0x80,0x80,0x80),
	SETRGBCOLOR(/*HGR_GREY2, */ 0x80,0x80,0x80),
	SETRGBCOLOR(/*HGR_YELLOW,*/ 0x9E,0x9E,0x00),
	SETRGBCOLOR(/*HGR_AQUA,  */ 0x00,0xCD,0x4A),
	SETRGBCOLOR(/*HGR_PURPLE,*/ 0x61,0x61,0xFF),
	SETRGBCOLOR(/*HGR_PINK,  */ 0xFF,0x32,0xB5),

	// LoRes
	SETRGBCOLOR(/*BLACK,*/      0x00,0x00,0x00),
	SETRGBCOLOR(/*DEEP_RED,*/   0xAC,0x12,0x4C),
	SETRGBCOLOR(/*DARK_BLUE,*/  0x00,0x07,0x83),
	SETRGBCOLOR(/*MAGENTA,*/    0xAA,0x1A,0xD1),
	SETRGBCOLOR(/*DARK_GREEN,*/ 0x00,0x83,0x2F),
	SETRGBCOLOR(/*DARK_GRAY,*/  0x9F,0x97,0x7E),
	SETRGBCOLOR(/*BLUE,*/       0x00,0x8A,0xB5),
	SETRGBCOLOR(/*LIGHT_BLUE,*/ 0x9F,0x9E,0xFF),
	SETRGBCOLOR(/*BROWN,*/      0x7A,0x5F,0x00),
	SETRGBCOLOR(/*ORANGE,*/     0xFF,0x72,0x47),
	SETRGBCOLOR(/*LIGHT_GRAY,*/ 0x78,0x68,0x7F),
	SETRGBCOLOR(/*PINK,*/       0xFF,0x7A,0xCF),
	SETRGBCOLOR(/*GREEN,*/      0x6F,0xE6,0x2C),
	SETRGBCOLOR(/*YELLOW,*/     0xFF,0xF6,0x7B),
	SETRGBCOLOR(/*AQUA,*/       0x6C,0xEE,0xB2),
	SETRGBCOLOR(/*WHITE,*/      0xFF,0xFF,0xFF),
};

// Memory offsets for TEXT/D/LGR and D/HGR modes
// The rows aren't contiguous in Apple 2 RAM.
// They're interlaced because WOZ chip optimization.

// Apple 2 TEXT row offset interlacing in RAM
static uint16_t g_RAM_TEXTOffsets[] =
{
	0x0000, 0x0080, 0x0100, 0x0180, 0x0200, 0x0280, 0x0300, 0x0380,
	0x0028, 0x00A8, 0x0128, 0x01A8, 0x0228, 0x02A8, 0x0328, 0x03A8,
	0x0050, 0x00D0, 0x0150, 0x01D0, 0x0250, 0x02D0, 0x0350, 0x03D0
};
// Apple 2 HGR row offsets interlacing in RAM
static uint16_t g_RAM_HGROffsets[] = {
	0x0000, 0x0400, 0x0800, 0x0C00, 0x1000, 0x1400, 0x1800, 0x1C00,
	0x0080, 0x0480, 0x0880, 0x0C80, 0x1080, 0x1480, 0x1880, 0x1C80,
	0x0100, 0x0500, 0x0900, 0x0D00, 0x1100, 0x1500, 0x1900, 0x1D00,
	0x0180, 0x0580, 0x0980, 0x0D80, 0x1180, 0x1580, 0x1980, 0x1D80,
	0x0200, 0x0600, 0x0A00, 0x0E00, 0x1200, 0x1600, 0x1A00, 0x1E00,
	0x0280, 0x0680, 0x0A80, 0x0E80, 0x1280, 0x1680, 0x1A80, 0x1E80,
	0x0300, 0x0700, 0x0B00, 0x0F00, 0x1300, 0x1700, 0x1B00, 0x1F00,
	0x0380, 0x0780, 0x0B80, 0x0F80, 0x1380, 0x1780, 0x1B80, 0x1F80,
	0x0028, 0x0428, 0x0828, 0x0C28, 0x1028, 0x1428, 0x1828, 0x1C28,
	0x00A8, 0x04A8, 0x08A8, 0x0CA8, 0x10A8, 0x14A8, 0x18A8, 0x1CA8,
	0x0128, 0x0528, 0x0928, 0x0D28, 0x1128, 0x1528, 0x1928, 0x1D28,
	0x01A8, 0x05A8, 0x09A8, 0x0DA8, 0x11A8, 0x15A8, 0x19A8, 0x1DA8,
	0x0228, 0x0628, 0x0A28, 0x0E28, 0x1228, 0x1628, 0x1A28, 0x1E28,
	0x02A8, 0x06A8, 0x0AA8, 0x0EA8, 0x12A8, 0x16A8, 0x1AA8, 0x1EA8,
	0x0328, 0x0728, 0x0B28, 0x0F28, 0x1328, 0x1728, 0x1B28, 0x1F28,
	0x03A8, 0x07A8, 0x0BA8, 0x0FA8, 0x13A8, 0x17A8, 0x1BA8, 0x1FA8,
	0x0050, 0x0450, 0x0850, 0x0C50, 0x1050, 0x1450, 0x1850, 0x1C50,
	0x00D0, 0x04D0, 0x08D0, 0x0CD0, 0x10D0, 0x14D0, 0x18D0, 0x1CD0,
	0x0150, 0x0550, 0x0950, 0x0D50, 0x1150, 0x1550, 0x1950, 0x1D50,
	0x01D0, 0x05D0, 0x09D0, 0x0DD0, 0x11D0, 0x15D0, 0x19D0, 0x1DD0,
	0x0250, 0x0650, 0x0A50, 0x0E50, 0x1250, 0x1650, 0x1A50, 0x1E50,
	0x02D0, 0x06D0, 0x0AD0, 0x0ED0, 0x12D0, 0x16D0, 0x1AD0, 0x1ED0,
	0x0350, 0x0750, 0x0B50, 0x0F50, 0x1350, 0x1750, 0x1B50, 0x1F50,
	0x03D0, 0x07D0, 0x0BD0, 0x0FD0, 0x13D0, 0x17D0, 0x1BD0, 0x1FD0
};

// Gotta have a transparency global just in case
// static uint32_t gRGBTransparent = 0;

// below because "The declaration of a static data member in its class definition is not a definition"
A2VideoManager* A2VideoManager::s_instance;

constexpr uint32_t CYCLES_HBLANK = 25;			// always 25 cycles
constexpr uint8_t _COLORBYTESOFFSET = 1 + 32;	// the color bytes are offset every line by 33 (after SCBs and palette)

static OpenGLHelper* oglHelper = OpenGLHelper::GetInstance();

static Shader shader_beam_legacy = Shader();
static Shader shader_beam_shr = Shader();

//////////////////////////////////////////////////////////////////////////
// Image Asset Methods
//////////////////////////////////////////////////////////////////////////

// NOTE:	Both the below image asset methods use OpenGL 
//			so they _must_ be called from the main thread
void A2VideoManager::ImageAsset::AssignByFilename(A2VideoManager* owner, const char* filename) {
	int width;
	int height;
	int channels;
	unsigned char* data = stbi_load(filename, &width, &height, &channels, 4);
	if (data == NULL) {
		std::cerr << "ERROR: STBI load failure" << stbi_failure_reason() << std::endl;
		return;
	}
	if (tex_id != UINT_MAX)
	{
		oglHelper->load_texture(data, width, height, channels, tex_id);
		stbi_image_free(data);
	}
	else {
		std::cerr << "ERROR: Could not bind texture, all slots filled!" << std::endl;
		return;
	}
	GLenum glerr;
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "ImageAsset::AssignByFilename error: " << glerr << std::endl;
	}
	image_xcount = width;
	image_ycount = height;
}

//////////////////////////////////////////////////////////////////////////
// Manager Methods
//////////////////////////////////////////////////////////////////////////

void A2VideoManager::Initialize()
{
	bIsReady = false;
	memset(a2legacy_vram, 0, _BEAM_VRAM_SIZE_LEGACY);
	memset(a2shr_vram, 0, _BEAM_VRAM_SIZE_SHR);
	bVBlankHasLegacy = true;
	bVBlankHasSHR = false;
	bBeamIsActive = false;
	
	auto memMgr = MemoryManager::GetInstance();
	color_background = gPaletteRGB[12 + (memMgr->switch_c022 & 0x0F)];
	color_foreground = gPaletteRGB[12 + ((memMgr->switch_c022 & 0xF0) >> 4)];
	color_border = gPaletteRGB[12 + (memMgr->switch_c034 & 0x0F)];

	// Set up the image assets (textures)
	// Assign them their respective GPU texture id
	*image_assets = {};
	for (size_t i = 0; i < (sizeof(image_assets) / sizeof(ImageAsset)); i++)
	{
		image_assets[i].tex_id = oglHelper->get_texture_id_at_slot(i);
	}

	// Generate shaders
	shader_beam_legacy.build(_SHADER_A2_VERTEX_DEFAULT, _SHADER_BEAM_LEGACY_FRAGMENT);
	shader_beam_shr.build(_SHADER_A2_VERTEX_DEFAULT, _SHADER_BEAM_SHR_FRAGMENT);

	// Initialize windows
	windowsbeam[A2VIDEOBEAM_LEGACY].Define(A2VIDEOBEAM_LEGACY, &shader_beam_legacy);
	windowsbeam[A2VIDEOBEAM_SHR].Define(A2VIDEOBEAM_SHR, &shader_beam_shr);

	// tell the next Render() call to run initialization routines
	bShouldInitializeRender = true;
	
	CycleCounter::GetInstance()->Reset();
	bIsReady = true;
}

A2VideoManager::~A2VideoManager()
{
	delete[] a2legacy_vram;
	delete[] a2shr_vram;
}

void A2VideoManager::ResetComputer()
{
    if (bIsRebooting == true)
        return;
    bIsRebooting = true;
	MemoryManager::GetInstance()->Initialize();
	this->Initialize();
    bIsRebooting = false;
}

bool A2VideoManager::IsReady()
{
	return bIsReady;
}

void A2VideoManager::ToggleA2Video(bool value)
{
	bA2VideoEnabled = value;
	if (bA2VideoEnabled)
	{
		auto scrSz = ScreenSize();
		if (oglHelper->request_framebuffer_resize(scrSz.x, scrSz.y))
			bShouldInitializeRender = true;
	}
	ForceBeamFullScreenRender();
}

void A2VideoManager::BeamIsAtPosition(uint32_t _x, uint32_t y)
{
	// Theoretically at y==192 (start of VBLANK) we can render for legacy
	// but SHR goes to 200 so let's wait until 200 anyway. We're in VBLANK still.
	if (_x == CYCLES_HBLANK && y == 200)	// Start of VBLANK
	{
		RequestVRAMUpdates(bVBlankHasLegacy, bVBlankHasSHR);
		// reset the legacy and shr flags at each vblank
		bVBlankHasLegacy = false;
		bVBlankHasSHR = false;
		return;
	}

	if (y >= 200)	// in VBLANK, nothing to do
		return;

	// Anything here below means the VRAMs are getting modified
	// No rendering should be done while the VRAMs are being modified
	
	std::lock_guard<std::mutex> lock(a2video_mutex);
	if (_x == 0)
	{
		// Always at the start of the row, set the SHR SCB to 0x10
		// Because we check bit 4 of the SCB to know if that line is drawn as SHR
		// The 2gs will always set bit 4 to 0 when sending it over
		a2shr_vram[(_COLORBYTESOFFSET + 160) * y] = 0x10;
	}
	if (_x < CYCLES_HBLANK)	// in HBLANK, nothing to do
		return;
	
	auto memMgr = MemoryManager::GetInstance();

	// Get the colors
	color_background = gPaletteRGB[12 + (memMgr->switch_c022 & 0x0F)];
	color_foreground = gPaletteRGB[12 + ((memMgr->switch_c022 & 0xF0) >> 4)];
	color_border = gPaletteRGB[12 + (memMgr->switch_c034 & 0x0F)];

	// Set xx to 0 when after HBLANK. HBLANK is always at the start of the line
	// However, VBLANK is at the end of the screen so we can use y as is
	uint32_t xx = _x - CYCLES_HBLANK;	// xx is always positive here
	if (memMgr->IsSoftSwitch(A2SS_SHR))
	{
		bVBlankHasSHR = true;		// at least 1 byte in this vblank cycle is in SHR
		uint8_t* lineStartPtr = a2shr_vram + (_COLORBYTESOFFSET + 160) * y;
		auto memPtr = memMgr->GetApple2MemAuxPtr();
		if (xx == 0)
		{
			// it's the beginning of the line
			// Get the SCB
			lineStartPtr[0] = *(memPtr + _A2VIDEO_SHR_SCB_START + y);
			// Get the palette
			memcpy(lineStartPtr + 1,	// palette starts at byte 1 in our a2shr_vram
				   memPtr + _A2VIDEO_SHR_PALETTE_START + ((uint32_t)(lineStartPtr[0] & 0xFu) * 32),
				   32);					// palette length is 32 bytes
		}
		// Get the color info for the 4 bytes where the beam is
		auto xfb = xx * 4;	// the x first byte, given that every beam x renders 4 bytes
		auto scb = lineStartPtr[0];
		for (uint8_t i = 0; i < 4; i++)
		{
			lineStartPtr[_COLORBYTESOFFSET + xfb + i] = *(memPtr + _A2VIDEO_SHR_START + y * _A2VIDEO_SHR_BYTES_PER_LINE + xfb + i);
			// Pre-calculate colorfill, so that the shader doesn't have to do it
			// It's completely wasted on the shader. Here it's much more efficient
			if (!(scb & 0x80u) && (scb & 0x20u))	// 320 mode and colorfill
			{
				auto byteColor = lineStartPtr[_COLORBYTESOFFSET + xfb + i];
				// if the first color of the byte is 0, give it the last color of the previous byte
				// assuming this is not the first byte of the line
				if (((byteColor & 0xF0) == 0) && ((xfb + i) != 0))
					byteColor |= (lineStartPtr[_COLORBYTESOFFSET + xfb + i - 1] & 0b1111) << 4;
				// if the second color of the byte is 0, give it the first color of the byte
				if ((byteColor & 0x0F) == 0)
					byteColor |= (byteColor >> 4);
				lineStartPtr[_COLORBYTESOFFSET + xfb + i] = byteColor;
			}
		}
		return;
	}
	
	// The byte isn't SHR, it's legacy
	bVBlankHasLegacy = true;	// at least 1 byte in this vblank cycle is not SHR
	auto byteStartPtr = a2legacy_vram + ((40 * y) + xx) * 4;	// 4 bytes in VRAM for each byte on screen
	// the flags byte is:
	// bits 0-2: mode (TEXT, DTEXT, LGR, DLGR, HGR, DHGR, DHGRMONO)
	// bit 3: ALT charset for TEXT
	// bits 4-7: border color (like in the 2gs)
	uint8_t flags = 0;
	// the colors byte is:
	// bits 0-3: background color
	// bits 4-7: foreground color
	uint8_t colors = 0;
	
	// now set the mode, and depending on the mode, grab the bytes
	if (!memMgr->IsSoftSwitch(A2SS_TEXT))
	{
		if (memMgr->IsSoftSwitch(A2SS_MIXED) && y > 159)	// check mixed mode
		{
			if (memMgr->IsSoftSwitch(A2SS_80COL))
				flags = 1;	// DTEXT
			else
				flags = 0;	// TEXT
		}
		else if (memMgr->IsSoftSwitch(A2SS_80COL) && memMgr->IsSoftSwitch(A2SS_DHGR))	// double resolution
		{
			if (memMgr->IsSoftSwitch(A2SS_HIRES))
			{
				if (memMgr->IsSoftSwitch(A2SS_DHGRMONO))
					flags = 6;	// DHGRMONO
				else
					flags = 5;	// DHGR
			}
			else
				flags = 3;	// DLGR
		}
		else if (memMgr->IsSoftSwitch(A2SS_HIRES))	// standard hires
		{
			flags = 4;	// HGR
		}
		else {	// standard lores
			flags = 2;	// LGR
		}
	} else { 	// Now check the text modes
		if (memMgr->IsSoftSwitch(A2SS_80COL))
			flags = 1;	// DTEXT
		else
		{
			flags = 0;	// TEXT
		}
	}
		
	// Fill in the rest of the flags. We already use bits 0-2 for the modes
	flags |= ((memMgr->IsSoftSwitch(A2SS_ALTCHARSET) ? 1 : 0) << 3);	// bit 3 is alt charset
	flags |= ((memMgr->switch_c034 & 0b111) << 4);						// bits 4-7 are border color
																		// and the colors
	colors = memMgr->switch_c022;
	// Check for page 2
	bool isPage2 = false;
	// Careful: it's only page 2 if 80STORE is off
	if (memMgr->IsSoftSwitch(A2SS_PAGE2) && !memMgr->IsSoftSwitch(A2SS_80STORE))
		isPage2 = true;
	
	// Finally set the 4 VRAM bytes
	// Determine where in memory we should get the data from, and get it
	if ((flags & 0b111) < 4)	// D/TEXT AND D/LGR
	{
		uint32_t startMem = _A2VIDEO_TEXT1_START;
		if (((flags & 0b111) < 3) && isPage2)		// check for page 2 (DLGR doesn't have it)
			startMem = _A2VIDEO_TEXT2_START;
		byteStartPtr[0] = *(memMgr->GetApple2MemPtr() + startMem + g_RAM_TEXTOffsets[y / 8] + xx);
		byteStartPtr[1] = *(memMgr->GetApple2MemAuxPtr() + startMem + g_RAM_TEXTOffsets[y / 8] + xx);
	} else {		// D/HIRES
		uint32_t startMem = _A2VIDEO_HGR1_START;
		if (isPage2)
			startMem = _A2VIDEO_HGR2_START;
		byteStartPtr[0] = *(memMgr->GetApple2MemPtr() + startMem + g_RAM_HGROffsets[y] + xx);
		byteStartPtr[1] = *(memMgr->GetApple2MemAuxPtr() + startMem + g_RAM_HGROffsets[y] + xx);
	}
	byteStartPtr[2] = flags;
	byteStartPtr[3] = colors;

}

void A2VideoManager::RequestVRAMUpdates(bool cycleHasLegacy, bool cycleHasSHR)
{
	bBeamRenderLegacy = cycleHasLegacy;
	bBeamRenderSHR = cycleHasSHR;
	bRequestVRAMUpdates = true;
}

void A2VideoManager::ForceBeamFullScreenRender()
{
	// Forces a full screen render for the beam renderer
	// Move the beam over the whole screen
	for (uint32_t y = 0; y < 201; y++)	// renders at 200 (VBL)
	{
		for (uint32_t x = 0; x < 65; x++)	// HBL is the first 25
		{
			this->BeamIsAtPosition(x, y);
		}
	}
}

uXY A2VideoManager::ScreenSize()
{
	uXY maxSize = uXY({ _A2VIDEO_MIN_WIDTH, _A2VIDEO_MIN_HEIGHT});
	uXY s = maxSize;
	if (this->windowsbeam[A2VIDEOBEAM_SHR].IsEnabled())
	{
		s = uXY({ _A2VIDEO_SHR_WIDTH, _A2VIDEO_SHR_HEIGHT});
	}
	maxSize.x = (s.x > maxSize.x ? s.x : maxSize.x);
	maxSize.y = (s.y > maxSize.y ? s.y : maxSize.y);
	return maxSize;
}

void A2VideoManager::Render()
{
	if (!bA2VideoEnabled)
		return;

	if ((!bShouldInitializeRender) && (!bRequestVRAMUpdates))
		return;
	
	GLenum glerr;
	auto oglh = OpenGLHelper::GetInstance();

	oglh->setup_render();

	// Initialization routine runs only once on init (or re-init)
	// We do that here because we know the framebuffer is bound, and everything
	// for drawing the SDHR stuff is active
	if (bShouldInitializeRender) {
		bShouldInitializeRender = false;
		
		// image asset 0: The apple 2e US font
		glActiveTexture(_SDHR_START_TEXTURES);
		image_assets[0].AssignByFilename(this, "assets/Apple2eFont14x16 - Regular.png");
		// image asset 1: The alternate font
		glActiveTexture(_SDHR_START_TEXTURES + 1);
		image_assets[1].AssignByFilename(this, "assets/Apple2eFont14x16 - Alternate.png");
		// image asset 2: The apple 2e US font 80COL
		glActiveTexture(_SDHR_START_TEXTURES + 2);
		image_assets[2].AssignByFilename(this, "assets/Apple2eFont7x16 - Regular.png");
		// image asset 3: The alternate font 80COL
		glActiveTexture(_SDHR_START_TEXTURES + 3);
		image_assets[3].AssignByFilename(this, "assets/Apple2eFont7x16 - Alternate.png");
		// image asset 4: LGR texture (overkill for color, useful for dithered b/w)
		glActiveTexture(_SDHR_START_TEXTURES + 4);
		image_assets[4].AssignByFilename(this, "assets/Texture_composite_lgr.png");
		// image asset 5: HGR texture
		glActiveTexture(_SDHR_START_TEXTURES + 5);
		image_assets[5].AssignByFilename(this, "assets/Texture_composite_hgr.png");
		// image asset 6: DHGR texture
		glActiveTexture(_SDHR_START_TEXTURES + 6);
		image_assets[6].AssignByFilename(this, "assets/Texture_composite_dhgr.png");
		// image asset 7: The bezel for postprocessing
		glActiveTexture(_SDHR_START_TEXTURES + 7);
		image_assets[7].AssignByFilename(this, "assets/Bezel.png");
		if ((glerr = glGetError()) != GL_NO_ERROR) {
			std::cerr << "OpenGL AssignByFilename error: " 
				<< 0 << " - " << glerr << std::endl;
		}
		glActiveTexture(GL_TEXTURE0);

		// Make sure the beam renderer has gone through one pass of the VRAM updates
		// Unless we want SDD to display a "splash screen", in which case this is where
		// we'd do it.
		ForceBeamFullScreenRender();
	}

	std::lock_guard<std::mutex> lock(a2video_mutex);

	// At line 200 the cycle counter flags to update the VRAM in the GPU
	if (bRequestVRAMUpdates)
	{
		windowsbeam[A2VIDEOBEAM_LEGACY].SetEnabled(bBeamRenderLegacy);
		windowsbeam[A2VIDEOBEAM_SHR].SetEnabled(bBeamRenderSHR);
	}
	windowsbeam[A2VIDEOBEAM_LEGACY].Render(bRequestVRAMUpdates);
	windowsbeam[A2VIDEOBEAM_SHR].Render(bRequestVRAMUpdates);
	bRequestVRAMUpdates = false;


	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL draw error: " << glerr << std::endl;
	}
	oglh->finalize_render();
}

bool A2VideoManager::ShouldRender()
{
	// Only render when VRAM needs updating, in VBLANK
	return (bRequestVRAMUpdates || (!bBeamIsActive));
}

void A2VideoManager::ActivateBeam()
{
	bBeamIsActive = true;
}

void A2VideoManager::DeactivateBeam()
{
	bBeamIsActive = false;
}
