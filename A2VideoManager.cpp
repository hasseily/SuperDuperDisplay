#include "A2VideoManager.h"
#include <cstring>
#include <zlib.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>   // for std::memcmp
#include <string>
#include <algorithm>
#include <system_error>
#include <map>
#include "SDL.h"
#include <SDL_opengl.h>
#ifdef _DEBUGTIMINGS
#include <chrono>
#endif
#include <iostream>
#include <iomanip>
#include "OpenGLHelper.h"
#include "MemoryManager.h"
#include "extras/MemoryLoader.h"
#include "SoundManager.h"
#include "MockingboardManager.h"
#include "LogTextManager.h"
#include "EventRecorder.h"
#include "GRAddr2XY.h"
#include "imgui.h"
#include "SDL_rect.h"
#include "BasicQuad.h"

// In main.cpp
extern uint32_t Main_GetFPSLimit();

// Aspect constraint callback to enforce a fixed aspect ratio for ImGui windows.
// The aspect ratio is provided via the UserData pointer
static inline void AspectConstraintCallback(ImGuiSizeCallbackData* data)
{
	float aspect_ratio = *(float*)data->UserData;
	data->DesiredSize.y = data->DesiredSize.x / aspect_ratio;
}

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

static std::string fontpath = "assets/Apple2eFont14x16";

// below because "The declaration of a static data member in its class definition is not a definition"
A2VideoManager* A2VideoManager::s_instance;

//////////////////////////////////////////////////////////////////////////
// Overlay String Methods
//////////////////////////////////////////////////////////////////////////

void A2VideoManager::DrawOverlayString(const std::string& text, uint8_t colors, uint32_t x, uint32_t y)
{
	for (uint8_t i = 0; i < text.length(); ++i)
	{
		overlay_text[_OVERLAY_CHAR_WIDTH * y + x + i] = text.c_str()[i] + 0x80;
		overlay_colors[_OVERLAY_CHAR_WIDTH * y + x + i] = colors;
	}
	overlay_lines[y] = 1;
}

void A2VideoManager::DrawOverlayString(const char* text, uint8_t len, uint8_t colors, uint32_t x, uint32_t y)
{
	(void)len;
	uint8_t i = 0;
	while (text[i] != '\0')
	{
		overlay_text[_OVERLAY_CHAR_WIDTH * y + x + i] = text[i] + 0x80;
		overlay_colors[_OVERLAY_CHAR_WIDTH * y+ + x + i] = colors;
		++i;
	}
	overlay_lines[y] = 1;
}

void A2VideoManager::DrawOverlayCharacter(const char c, uint8_t colors, uint32_t x, uint32_t y)
{
	overlay_text[_OVERLAY_CHAR_WIDTH * y + x] = c + 0x80;
	overlay_colors[_OVERLAY_CHAR_WIDTH * y + x] = colors;
	overlay_lines[y] = 1;
}

void A2VideoManager::EraseOverlayRange(uint8_t len, uint32_t x, uint32_t y)
{
	memset(overlay_text + (_OVERLAY_CHAR_WIDTH * y + x), 0, len);
	UpdateOverlayLine(y);
}

void A2VideoManager::EraseOverlayCharacter(uint32_t x, uint32_t y)
{
	overlay_text[_OVERLAY_CHAR_WIDTH * y + x] = 0;
	UpdateOverlayLine(y);
}

inline void A2VideoManager::UpdateOverlayLine(uint32_t y)
{
	for (uint8_t i=0; i< _OVERLAY_CHAR_WIDTH; ++i) {
		if (overlay_text[_OVERLAY_CHAR_WIDTH * y + i] > 0)
		{
			overlay_lines[y] = 1;
			return;
		}
	}
	overlay_lines[y] = 0;
}

//////////////////////////////////////////////////////////////////////////
// Manager Methods
//////////////////////////////////////////////////////////////////////////

A2VideoManager::~A2VideoManager()
{
	for (int i = 0; i < 2; i++)
	{
		if (vrams_array[i].vram_legacy != nullptr)
			delete[] vrams_array[i].vram_legacy;
		if (vrams_array[i].vram_shr != nullptr)
			delete[] vrams_array[i].vram_shr;
		if (vrams_array[i].vram_pal256 != nullptr)
			delete[] vrams_array[i].vram_pal256;
		if (vrams_array[i].offset_buffer != nullptr)
			delete[] vrams_array[i].offset_buffer;
		
		if (vrams_array[i].vram_forced_text1 != nullptr)
			delete[] vrams_array[i].vram_forced_text1;
		if (vrams_array[i].vram_forced_text2 != nullptr)
			delete[] vrams_array[i].vram_forced_text2;
		if (vrams_array[i].vram_forced_hgr1 != nullptr)
			delete[] vrams_array[i].vram_forced_hgr1;
		if (vrams_array[i].vram_forced_hgr2 != nullptr)
			delete[] vrams_array[i].vram_forced_hgr2;
	}
	delete[] vrams_array;

	ResetGLData();

	if (APPLE2MEMORYTEX != UINT_MAX)
		glDeleteTextures(1, &APPLE2MEMORYTEX);
}

void A2VideoManager::Initialize()
{
	// Here do not reinitialize bBeamIsActive. It could still be active from earlier.
	// The initialization process can be triggered from a ctrl-reset on the Apple 2.
	bIsReady = false;

	v_debug_rgb_windows.reserve(_MAX_DEBUG_RGB_WINDOWS);

	border_w_slider_val = (int)this->GetBordersWidthCycles();
	border_h_slider_val = (int)this->GetBordersHeightScanlines() / 8;

	SDL_memset(&event_newframe, 0, sizeof(event_newframe));
	event_newframe.type = SDL_USEREVENT;
	event_newframe.user.code = SDLUSEREVENT_A2NEWFRAME;
	event_newframe.user.data1 = nullptr;
	event_newframe.user.data2 = nullptr;

	ResetGLData();

	auto oglHelper = OpenGLHelper::GetInstance();
	for (int i = 0; i < 2; i++)
	{
		vrams_array[i].id = i;
		vrams_array[i].frame_idx = current_frame_idx + i;
		vrams_array[i].bWasRendered = true;		// otherwise it won't render the first frame
		vrams_array[i].mode = A2Mode_e::NONE;
		if (vrams_array[i].vram_legacy != nullptr)
		{
			delete[] vrams_array[i].vram_legacy;
			vrams_array[i].vram_legacy = nullptr;
		}
		if (vrams_array[i].vram_shr != nullptr)
		{
			delete[] vrams_array[i].vram_shr;
			vrams_array[i].vram_shr = nullptr;
		}
		if (vrams_array[i].vram_pal256 != nullptr)
		{
			delete[] vrams_array[i].vram_pal256;
			vrams_array[i].vram_pal256 = nullptr;
		}
		if (vrams_array[i].offset_buffer != nullptr)
		{
			delete[] vrams_array[i].offset_buffer;
			vrams_array[i].offset_buffer = nullptr;
		}
		vrams_array[i].vram_legacy = new uint8_t[GetVramSizeLegacy()];
		vrams_array[i].vram_shr = new uint8_t[GetVramSizeSHR()];
		vrams_array[i].vram_pal256 = new uint8_t[_A2VIDEO_SHR_BYTES_PER_LINE*2*_A2VIDEO_SHR_SCANLINES*_INTERLACE_MULTIPLIER];
		vrams_array[i].offset_buffer = new GLfloat[GetVramHeightSHR()];
		memset(vrams_array[i].vram_legacy, 0, GetVramSizeLegacy());
		memset(vrams_array[i].vram_shr, 0, GetVramSizeSHR());
		memset(vrams_array[i].vram_pal256, 0, _A2VIDEO_SHR_BYTES_PER_LINE*2*_A2VIDEO_SHR_SCANLINES*_INTERLACE_MULTIPLIER);
		memset(vrams_array[i].offset_buffer, 0, GetVramHeightSHR() * sizeof(GLfloat));

		// the debugging special vrams
		if (vrams_array[i].vram_forced_text1 != nullptr)
		{
			delete[] vrams_array[i].vram_forced_text1;
			vrams_array[i].vram_forced_text1 = nullptr;
		}
		if (vrams_array[i].vram_forced_text2 != nullptr)
		{
			delete[] vrams_array[i].vram_forced_text2;
			vrams_array[i].vram_forced_text2 = nullptr;
		}
		if (vrams_array[i].vram_forced_hgr1 != nullptr)
		{
			delete[] vrams_array[i].vram_forced_hgr1;
			vrams_array[i].vram_forced_hgr1 = nullptr;
		}
		if (vrams_array[i].vram_forced_hgr2 != nullptr)
		{
			delete[] vrams_array[i].vram_forced_hgr2;
			vrams_array[i].vram_forced_hgr2 = nullptr;
		}
		vrams_array[i].vram_forced_text1 = new uint8_t[40 * 192 * 4];
		vrams_array[i].vram_forced_text2 = new uint8_t[40 * 192 * 4];
		vrams_array[i].vram_forced_hgr1 = new uint8_t[40 * 192 * 4];
		vrams_array[i].vram_forced_hgr2 = new uint8_t[40 * 192 * 4];
		memset(vrams_array[i].vram_forced_text1, 0, 40 * 192 * 4);
		memset(vrams_array[i].vram_forced_text2, 0, 40 * 192 * 4);
		memset(vrams_array[i].vram_forced_hgr1, 0, 40 * 192 * 4);
		memset(vrams_array[i].vram_forced_hgr2, 0, 40 * 192 * 4);
	}
	vrams_write = &vrams_array[0];
	vrams_read = &vrams_array[1];

	// Set up the image assets (textures)
	// Assign them their respective GPU texture id
	*image_assets = {};
	for (uint8_t i = 0; i < (sizeof(image_assets) / sizeof(OpenGLHelper::ImageAsset)); i++)
	{
		image_assets[i].tex_id = oglHelper->get_texture_id_at_slot(i);
	}

	// Get the font roms
	try {
		font_roms_array.clear();
		for (const auto & entry : std::filesystem::directory_iterator(fontpath)) {
			if (entry.is_regular_file()) {
				if ((entry.path().extension() == ".png") || (entry.path().extension() == ".PNG"))
					font_roms_array.push_back(entry.path().filename().string());
			}
		}
		if (font_roms_array.empty()) {
			throw std::filesystem::filesystem_error(
													"No Font ROM textures found!",
													std::make_error_code(std::errc::no_such_file_or_directory)
													);
		}
		std::sort(font_roms_array.begin(), font_roms_array.end());
	} catch (const std::filesystem::filesystem_error& e) {
		std::cerr << "Error accessing directory: " << e.what() << std::endl;
		exit(1);
	}

	// Initialize windows
	windowsbeam[A2VIDEOBEAM_LEGACY] = std::make_unique<A2WindowBeam>(A2VIDEOBEAM_LEGACY, _SHADER_A2_VERTEX_DEFAULT, _SHADER_BEAM_LEGACY_FRAGMENT);
	windowsbeam[A2VIDEOBEAM_SHR] = std::make_unique<A2WindowBeam>(A2VIDEOBEAM_SHR, _SHADER_A2_VERTEX_DEFAULT, _SHADER_BEAM_SHR_FRAGMENT);
	windowsbeam[A2VIDEOBEAM_LEGACY]->SetBorder(borders_w_cycles, borders_h_scanlines);
	windowsbeam[A2VIDEOBEAM_SHR]->SetBorder(borders_w_cycles, borders_h_scanlines);

	windowsbeam[A2VIDEOBEAM_FORCED_TEXT1] = std::make_unique<A2WindowBeam>(A2VIDEOBEAM_FORCED_TEXT1, _SHADER_A2_VERTEX_DEFAULT, _SHADER_BEAM_LEGACY_FRAGMENT);
	windowsbeam[A2VIDEOBEAM_FORCED_TEXT2] = std::make_unique<A2WindowBeam>(A2VIDEOBEAM_FORCED_TEXT2, _SHADER_A2_VERTEX_DEFAULT, _SHADER_BEAM_LEGACY_FRAGMENT);
	windowsbeam[A2VIDEOBEAM_FORCED_HGR1] = std::make_unique<A2WindowBeam>(A2VIDEOBEAM_FORCED_HGR1, _SHADER_A2_VERTEX_DEFAULT, _SHADER_BEAM_LEGACY_FRAGMENT);
	windowsbeam[A2VIDEOBEAM_FORCED_HGR2] = std::make_unique<A2WindowBeam>(A2VIDEOBEAM_FORCED_HGR2, _SHADER_A2_VERTEX_DEFAULT, _SHADER_BEAM_LEGACY_FRAGMENT);
	windowsbeam[A2VIDEOBEAM_FORCED_TEXT1]->SetBorder(0, 0);
	windowsbeam[A2VIDEOBEAM_FORCED_TEXT2]->SetBorder(0, 0);
	windowsbeam[A2VIDEOBEAM_FORCED_HGR1]->SetBorder(0, 0);
	windowsbeam[A2VIDEOBEAM_FORCED_HGR2]->SetBorder(0, 0);

	vidhdWindowBeam = std::make_unique<VidHdWindowBeam>(VIDHDMODE_NONE);

	legacyNTSCQuad = std::make_unique<BasicQuad>(_SHADER_VERTEX_BASIC, "shaders/ntsc_sik.frag");

	// The framebuffer width. That will change depending on the layers that are rendered:
	// If a VidHD text modes > 80x24 is active, then 1920x1080
	// Otherwise if SHR is active in any scanline, then SHR size
	// Otherwise, Legacy size
	fb_width = windowsbeam[A2VIDEOBEAM_LEGACY]->GetWidth();
	fb_height = windowsbeam[A2VIDEOBEAM_LEGACY]->GetHeight();

	beamState = BeamState_e::NBVBLANK;
	merge_last_change_mode = A2Mode_e::NONE;
	merge_last_change_y = UINT_MAX;
	
	offsetTextureExists = false;

	mem_edit_vram_legacy.Open = false;
	mem_edit_vram_shr.Open = false;
	mem_edit_offset_buffer.Open = false;
	
	// tell the next Render() call to run initialization routines
	bShouldInitializeRender = true;
	
	// clear the text overlay
	std::memset(overlay_text, 0, sizeof(overlay_text));
	std::memset(overlay_colors, 0, sizeof(overlay_colors));
	std::memset(overlay_lines, 0, sizeof(overlay_lines));

	// Set default border color
	MemoryManager::GetInstance()->switch_c034 = 13;

	bIsReady = true;
}

void A2VideoManager::ResetGLData() {
	if (OFFSETTEX != UINT_MAX)
		glDeleteTextures(1, &OFFSETTEX);

	if (FBO_A2Video != UINT_MAX)
	{
		glDeleteFramebuffers(1, &FBO_A2Video);
		glDeleteTextures(1, &a2video_texture_id);
		glDeleteFramebuffers(1, &FBO_NTSC);
		glDeleteTextures(1, &ntsc_texture_id);
	}
	FBO_A2Video = UINT_MAX;
	FBO_NTSC = UINT_MAX;

	for (int i = 0; i < 4; i++)
	{
		if (FBO_debug[i] != UINT_MAX)
		{
			glDeleteFramebuffers(1, &FBO_debug[i]);
			glDeleteTextures(1, &debug_texture_id[i]);
		}
		FBO_debug[i] = UINT_MAX;
	}

	OFFSETTEX = UINT_MAX;
}

void A2VideoManager::ResetComputer()
{
    if (bIsRebooting == true)
        return;
    bIsRebooting = true;
	Initialize();
	MemoryManager::GetInstance()->Initialize();
	SoundManager::GetInstance()->Initialize();
	MockingboardManager::GetInstance()->Initialize();
    bIsRebooting = false;
}

bool A2VideoManager::IsReady()
{
	return bIsReady;
}

void A2VideoManager::ToggleA2Video(bool value)
{
	// If true, the A2 Video reinitializes fully
	// Only call this method when reinit is necessary,
	// like changing mode from SDHR to A2 Video
	bA2VideoEnabled = value;
	if (bA2VideoEnabled)
		bShouldInitializeRender = true;
}

void A2VideoManager::CheckSetBordersWithReinit()
{
	if (border_w_slider_val > _BORDER_WIDTH_MAX_CYCLES)
		border_w_slider_val = _BORDER_WIDTH_MAX_CYCLES;
	if (border_h_slider_val > _BORDER_WIDTH_MAX_CYCLES)
		border_h_slider_val = _BORDER_WIDTH_MAX_CYCLES;
	if ((border_w_slider_val == borders_w_cycles)
		&& (border_h_slider_val == (borders_h_scanlines / 8)))
		return;
	bIsReady = false;
	borders_w_cycles = border_w_slider_val;
	borders_h_scanlines = border_h_slider_val * 8;	// Must be multiple of 8s
	auto _mms = MemoryManager::GetInstance()->SerializeSwitches();
	this->Initialize();
	MemoryManager::GetInstance()->DeserializeSwitches(_mms);
	this->ForceBeamFullScreenRender();
}

// The input is x,y,width,height where x,y are top left origin. The output is SDL style inverted Y
// The full screen quad output would be { -1.f, 1.f, 2.f, -2.f }
SDL_FRect A2VideoManager::NormalizePixelQuad(const SDL_FRect& pixelQuad)
{
	SDL_FRect normalized;
	normalized.x = (2.0f * pixelQuad.x) / fb_width - 1.0f;
	normalized.y = 1.0f - (2.0f * pixelQuad.y) / fb_height;
	normalized.w = (2.0f * pixelQuad.w) / fb_width;
	normalized.h = (-2.f * pixelQuad.h) / fb_height;
	return normalized;
}

SDL_FRect A2VideoManager::CenteredQuadInFramebuffer(const SDL_FRect& quad)
{
	SDL_FRect centeredQuad;
	centeredQuad.w = quad.w;
	centeredQuad.h = quad.h;
	centeredQuad.x = (fb_width - quad.w) / 2.0f;
	centeredQuad.y = (fb_height - quad.h) / 2.0f;
	return centeredQuad;
}

SDL_FRect A2VideoManager::CenteredQuadInFramebufferWithOffset(const SDL_FRect& quad, const SDL_FPoint& offset)
{
	auto resQuad = CenteredQuadInFramebuffer(quad);
	resQuad.x += offset.x;
	resQuad.y += offset.y;
	return resQuad;
}

void A2VideoManager::StartNextFrame()
{
	// start the next frame
	// set the frame index for the buffer we'll move to reading
	vrams_write->frame_idx = ++current_frame_idx;
	// std::cerr << "starting next frame at current index: " << current_frame_idx << std::endl;

	// Flip the double buffers only if the read buffer was rendered
	// Otherwise it means the renderer is too slow and hasn't finished rendering
	// We just overwrite the current buffer
	if (vrams_read->bWasRendered == true)
	{
		vrams_read->bWasRendered = false;
		auto _vtmp = vrams_write;
		vrams_write = vrams_read;
		vrams_read = _vtmp;
	}
//	memset(vrams_write->vram_legacy, 0, GetVramSizeLegacy());
//	memset(vrams_write->vram_shr, 0, GetVramSizeSHR());
//	memset(vrams_write->offset_buffer, 0, GetVramHeightSHR() * sizeof(GLfloat));

	// At each vblank reset the mode
	vrams_write->mode = A2Mode_e::NONE;
	// Update the current region info
	current_region = CycleCounter::GetInstance()->GetVideoRegion();
	region_scanlines = (current_region == VideoRegion_e::NTSC ? SC_TOTAL_NTSC : SC_TOTAL_PAL);
	// For the merged mode
	merge_last_change_y = UINT_MAX;
	// Additional frame data resets
	vrams_write->frameSHRModes = 0;
	vrams_write->pagedMode = 0;

	// And finally send an event to the main loop saying that the frame was updated
	// This is necessary when synching to the Apple 2 VSYNC. Don't create a new event if there are 4
	// events already in the queue.
	int _numEvents = SDL_PeepEvents(user_events_active, MAX_USEREVENTS_IN_QUEUE, 
		SDL_PEEKEVENT, SDL_USEREVENT, SDL_USEREVENT);
	if (_numEvents < 4)
	{
		auto _everr = SDL_PushEvent(&event_newframe);
		if (_everr == 0)
			std::cerr << "Event queue is full, could not push event_newframe!" << std::endl;
		else if (_everr < 0) {
			std::cerr << "event_newframe error: " << SDL_GetError() << std::endl;
		}
	}
#ifdef DEBUG
	else {
		std::cerr << "Max # of A2 frames already in event queue!" << std::endl;
	}
#endif
}

void A2VideoManager::BeamIsAtPosition(uint32_t _x, uint32_t _y)
{
	/*
		@: Frame flip and start of next frame
		&: Start next frame scanlines
	 ||H|        |H||----------------------------------------------------------------------------|
	 ||B|        |B||                                                                      		 |
	 ||o|        |o||                                                                      		 |
	 ||r| HBLANK |r||                                    Content                                 |
	 ||d|        |d||                     CYCLES_SC_CONTENT x mode_scanlines                  	 |
	 ||e|        |e||                                                                      		 |
	 ||r|        |r||                                                                      		 |
	 |		        |----------------------         Vertical border       -----------------------|
	 |		        |--------------------------- (borders_h_scanlines) --------------------------|
	 |@		        |............................. vertical blanking ............................|
	 |		        |............................. vertical blanking ............................|
	 |		        |............................. vertical blanking ............................|
	 |		        |............................. vertical blanking ............................|
	 |		        |............................. vertical blanking ............................|
	 |		        |............................. vertical blanking ............................|
	 |		        |............................. vertical blanking ............................|
	 |&             |-----------------      Vertical border  (next frame)       -----------------|
	 |		        |--------------------------- (borders_h_scanlines) --------------------------|

	 In order to achieve a "correct" top, left, right, bottom border around the content, with
	 the origin being at the start of the top border, we translate each border area's x & w
	 with the below #defines for each of Border L, R, T, B.
	 */

#define _TR_ANY_X ((_x + borders_w_cycles + CYCLES_SC_CONTENT) % CYCLES_SC_TOTAL)
#define _TR_ANY_Y ((_y + borders_h_scanlines) % region_scanlines)

	if (!bIsReady || bIsRebooting)
		return;

	auto memMgr = MemoryManager::GetInstance();
	uint32_t mode_scanlines = (memMgr->IsSoftSwitch(A2SS_SHR) ? 200 : 192);

	// The Apple 2gs drawing is shifted 6 scanlines down
	// Let's realign it to match the 2e
	if (memMgr->is2gs)
	{
		_y = (_y + region_scanlines - 6) % region_scanlines;
	}

	// Do not bother with the beam state until we get a frame start
	// Then we can start doing work
	if (_y == _SCANLINE_START_FRAME && _x == 0)	// frame start
	{
		beamState = BeamState_e::NBVBLANK;
	}

	// Now determine the actual beam state
	// And flip the frame when switching from BORDER_BOTTOM to NBVBLANK
	// keep updating the beam state until it reaches steady state
	auto _oldBeamState = beamState;

	while (true)
	{
		switch (beamState)
		{
		case BeamState_e::UNKNOWN:
			break;
		case BeamState_e::NBHBLANK:
			if (_x == (CYCLES_SC_HBL - borders_w_cycles))
				beamState = BeamState_e::BORDER_LEFT;
			break;
		case BeamState_e::NBVBLANK:
			// if there are no vertical borders then _y never gets to region_scanlines
			// and we need to handle this special case
			if (_y == (region_scanlines - borders_h_scanlines))
				beamState = BeamState_e::BORDER_TOP;
			else if (_y == 0 && borders_h_scanlines == 0)
				beamState = BeamState_e::BORDER_RIGHT;
			if (_y == _SCANLINE_START_FRAME && _x == 0)
			{
				// Start of NBVBLANK at which we flip the double buffering
				StartNextFrame();
			}
			break;
		case BeamState_e::BORDER_LEFT:
			if (_x == CYCLES_SC_HBL)
				beamState = BeamState_e::CONTENT;
			break;
		case BeamState_e::BORDER_RIGHT:
			if (_x == borders_w_cycles)
				beamState = BeamState_e::NBHBLANK;
			break;
		case BeamState_e::BORDER_TOP:
			if (_y == 0)
				beamState = BeamState_e::BORDER_RIGHT;
			break;
		case BeamState_e::BORDER_BOTTOM:
			if (_y == (mode_scanlines + borders_h_scanlines))
			{
				beamState = BeamState_e::NBVBLANK;
			}
			break;
		case BeamState_e::CONTENT:
			if (_x == 0)
			{
				if (_y == mode_scanlines)
					beamState = BeamState_e::BORDER_BOTTOM;
				else
					beamState = BeamState_e::BORDER_RIGHT;
			}
			break;
		default:
			break;
			break;
		}
		if (_oldBeamState == beamState)
			break;
		// std::cerr << "switched " << BeamStateToString(_oldBeamState) << " --> " << BeamStateToString(beamState) << std::endl;
		_oldBeamState = beamState;
	}

	// Check for text overlay in this position
	if ((_y < COUNT_SC_CONTENT) && (overlay_lines[_y / 8] == 1))
	{
		if (_x == 0)
		{
			bWasSHRBeforeOverlay = memMgr->IsSoftSwitch(A2SS_SHR);
			memMgr->SetSoftSwitch(A2SS_SHR, false);
		} else if (_x == (CYCLES_SC_TOTAL - 1))
		{
			memMgr->SetSoftSwitch(A2SS_SHR, bWasSHRBeforeOverlay);
		}
		if (beamState == BeamState_e::CONTENT)
		{
			if (_x < CYCLES_SC_HBL || _y >= mode_scanlines)		// bounds check if mode changes midway
				return;

			uint32_t _toff = _OVERLAY_CHAR_WIDTH * (_y/8) + (_x - CYCLES_SC_HBL);
			// Override when the byte is an overlay
			if (overlay_text[_toff] > 0)
			{
				if (vrams_write->mode == A2Mode_e::SHR)
					vrams_write->mode = A2Mode_e::MERGED;
				for (uint8_t ii=0; ii<8; ++ii) {
					uint8_t* byteStartPtr = vrams_write->vram_legacy + (GetVramWidthLegacy() * (_TR_ANY_Y + ii) + _TR_ANY_X) * 4;
					byteStartPtr[0] = overlay_text[_toff];		// main
					byteStartPtr[2] = 0b1000;
					byteStartPtr[3] = overlay_colors[_toff];
				}
				return;
			}
		}
	}

	auto vramSHRInterlaceOffset = GetVramSizeSHR() / _INTERLACE_MULTIPLIER;	// Offset to 2nd half of the vram

	// Always at the start of the row, set the SHR SCB to 0x10
	// Because we check bit 4 of the SCB to know if that line is drawn as SHR
	// The 2gs will always set bit 4 to 0 when sending it over
	// Also check if the mode has switched in the middle of the frame
	if (_x == 0)
	{
		if (_TR_ANY_Y < (_A2VIDEO_SHR_SCANLINES + 2 * borders_h_scanlines))
		{
			// Set both regular and interlaced areas SCBs
			vrams_write->vram_shr[GetVramWidthSHR() * _TR_ANY_Y] = 0x10;
			vrams_write->vram_shr[vramSHRInterlaceOffset + GetVramWidthSHR() * _TR_ANY_Y] = 0x10;

			if (vrams_write->mode == A2Mode_e::MERGED)
			{
				// Merge mode calculations
				// determine the mode switch and update merge_last_change_mode and merge_last_change_y
				auto _curr_mode = (memMgr->IsSoftSwitch(A2SS_SHR) ? A2Mode_e::SHR : A2Mode_e::LEGACY);
				if ((merge_last_change_mode == A2Mode_e::LEGACY) && (_curr_mode == A2Mode_e::SHR))
				{
					// 14 -> 16MHz
					merge_last_change_y = _TR_ANY_Y;
					// std::cerr << "merge to 16 " << merge_last_change_y << std::endl;
				}
				else if ((merge_last_change_mode == A2Mode_e::SHR) && (_curr_mode == A2Mode_e::LEGACY))
				{
					// 16 -> 14MHz
					merge_last_change_y = _TR_ANY_Y;
					// std::cerr << "merge to 14 " << merge_last_change_y << std::endl;
				}
				merge_last_change_mode = _curr_mode;

				// Finally set the offset
				// NOTE: We add 10.f to the offset so that the shader can know which mode to apply
				//		 If it's negative, it's Legacy. Positive, SHR.
				if (bNoMergedModeWobble)
				{
					vrams_write->offset_buffer[_TR_ANY_Y] = (_curr_mode == A2Mode_e::LEGACY ? -10.f : 10.f);
				}
				else
				{
					if (_TR_ANY_Y < merge_last_change_y					// the switch happened last frame
						|| (_TR_ANY_Y - merge_last_change_y) > 15)		// the switch has been recovered
					{
						vrams_write->offset_buffer[_TR_ANY_Y] = (_curr_mode == A2Mode_e::LEGACY ? -10.f : 10.f);
					}
					else
					{
						// If the change is to 28 MHz, shift negative (left). Otherwise, shift positive (right)
						float pixelShift = (GLfloat)glm::pow(glm::exp(merge_last_change_y - _TR_ANY_Y + 15), bWobblePower) - 1.0;
						if (_curr_mode == A2Mode_e::LEGACY)
							vrams_write->offset_buffer[_TR_ANY_Y] = -(10.f + pixelShift);
						else
							vrams_write->offset_buffer[_TR_ANY_Y] = 10.f + pixelShift;
					}
					// std::cerr << "Offset: " << vrams_write->offset_buffer[_TR_ANY_Y] << " y: " << _TR_ANY_Y << std::endl;
				}
			}
		}

	}

	// Now we get rid of all the non-border BLANK areas to avoid an overflow on the vertical border areas.
	// We never want to process vertical borders that are in non-border HBLANK
	// Otherwise we'd need the vertical border states to know when they're in HBLANK as well,
	// complicating the state machine.
	
	if (_x >= borders_w_cycles && _x < (CYCLES_SC_HBL - borders_w_cycles))
		return;
	if (_y >= (mode_scanlines + borders_h_scanlines) && _y < (region_scanlines - borders_h_scanlines))
		return;
	
	// Now generate the VRAMs themselves

	if (memMgr->IsSoftSwitch(A2SS_SHR))
	{
		// at least 1 byte in this vblank cycle is in SHR
		switch (vrams_write->mode)
		{
		case A2Mode_e::NONE:
			vrams_write->mode = A2Mode_e::SHR;
			break;
		case A2Mode_e::LEGACY:
			SwitchToMergedMode(_y);
			break;
		default:
			break;
		}
		auto bHasDoneDouble = false;	// flag to ensure we don't repeat the double/interlace work
		auto memPtr = memMgr->GetApple2MemAuxPtr();
		uint8_t* lineStartPtr = vrams_write->vram_shr + GetVramWidthSHR() * _TR_ANY_Y;

DRAW_VRAM:
		// get the SCB and palettes if we're starting a line
		// and it's part of the content area. The top & bottom border areas don't care about SCB
		// We may or may not have a border, so at this point the beamstate is either BORDER_LEFT or CONTENT
		if ((_TR_ANY_X == 0) && (_y < mode_scanlines))
		{
			lineStartPtr[0] = memPtr[_A2VIDEO_SHR_SCB_START + _y];
			// Get the palette (might be overwritten if it's a SHR3200 image
			memcpy(lineStartPtr + 1,	// palette starts at byte 1 in our a2shr_vram
				   memPtr + _A2VIDEO_SHR_PALETTE_START + ((uint32_t)(lineStartPtr[0] & 0xFu) * 32),
				   32);					// palette length is 32 bytes

			// Also here check all the palette reserved nibble values (high nibble of byte 2) to see
			// what SHR4 modes are used in this line, if SHR4 is enabled via the magic bytes
			
			uint32_t magicBytes = reinterpret_cast<uint32_t*>(memPtr + _A2VIDEO_SHR_MAGIC_BYTES)[0];
			if ((magicBytes == _A2VIDEO_SHR4_MAGIC_STRING) || ((overrideSHRMode & A2_VSM_SHR4SHR) != 0))	// SHR4 mode is enabled
			{
				// Modes are 0,1,2,3 on the high nibble of the 2-byte palette. We need to switch to bits as per A2VideoSpecialMode_e
				scanlineSHR4Modes = A2_VSM_SHR4SHR;	// Default SHR enabled for SHR4
				for (uint8_t i = 0; i < 16; ++i) {
					scanlineSHR4Modes |= (1 << ((lineStartPtr[2 + 2*i] >> 4) + 4));	// second byte of each palette color (skip SCB byte 1)
					// But if we're overriding the mode, let's change the palette in real time to match the overridden mode
					// This way the shader (and WindowBeam) doesn't need to know anything about overrides,
					// it's just given the correcly overridden data. The original modes remain in scanlineSHR4Modes so we can show that
					// to the user in the UI
					if ((overrideSHRMode & A2_VSM_SHR4SHR) != 0) {
						auto _lowNibble = (lineStartPtr[2 + 2*i] & 0xF);
						switch (overrideSHRMode) {
							case A2_VSM_SHR4SHR:
								lineStartPtr[2 + 2*i] = _lowNibble;
								break;
							case A2_VSM_SHR4RGGB:
								lineStartPtr[2 + 2*i] = _lowNibble + (1 << 4);
								break;
							case A2_VSM_SHR4PAL256:
								lineStartPtr[2 + 2*i] = _lowNibble + (2 << 4);
								break;
							case A2_VSM_SHR4R4G4B4:
								lineStartPtr[2 + 2*i] = _lowNibble + (3 << 4);
								break;
							default:
								break;
						}
					}
				}
				vrams_write->frameSHRModes |= scanlineSHR4Modes;	// Add to the frame's SHR4 modes the new modes found on this line
				// page mode is the first byte of the 4 control bytes which come just before the magic bytes
				vrams_write->pagedMode = (memPtr + _A2VIDEO_SHR_CTRL_BYTES)[0];
			} else if (
				((magicBytes == _A2VIDEO_3200_MAGIC_STRING) || (overrideSHRMode == A2_VSM_3200SHR))
				&& ((overrideSHRMode & A2_VSM_SHR4SHR) == 0)
					)	// i.e. if not overridden by any other SHR mode, and it's a 3200 image or we force it to be
			{
				// 3200 mode is enabled
				// There's a pointer to the start of the 200 palettes (1 per line) right before the magic bytes
				// The palettes could exist anywhere in memory so the developer tells us where they are
				// The 4 bytes are, from most to least significant:
				// - Paged Mode (00: no, 01: Interlace, 02: Pageflip) - defined in DoubleMode_e
				// - Bank: 00 for Main (E0), 01 for Aux (E1)
				// - Low byte of memory
				// - High byte of memory
				// Example: 00 00 80 22 means the palettes start at 0x2280 in main memory
				uint8_t* pCtrlStart = memPtr + _A2VIDEO_SHR_CTRL_BYTES;
				vrams_write->pagedMode = pCtrlStart[0];	// page mode is the first control byte
				uint8_t* bankPtr = (pCtrlStart[1] == 1 ? memMgr->GetApple2MemAuxPtr() : memMgr->GetApple2MemPtr());
				uint16_t palStart = (((uint16_t)pCtrlStart[3]) << 8) | pCtrlStart[2];
				if (palStart < (_A2_MEMORY_SHADOW_END - 200 * 32)) {
					// we may not be shadowing the whole memory
					memcpy(lineStartPtr + 1,	// palette starts at byte 1 in our a2shr_vram
						bankPtr + palStart + (_y * 32),
						32);					// palette length is 32 bytes
					vrams_write->frameSHRModes = A2_VSM_3200SHR;
				}
			}

			// Do the SCB and palettes for interlacing if requested
			if (!bHasDoneDouble)
			{
				bHasDoneDouble = true;
				bShouldPageDouble = (overrideDoubleSHR > 0 ? 1 : (vrams_write->pagedMode > 0));
				if (bShouldPageDouble)
				{
					// For the additional interlacing mode data, use main mem and the second part of vram_shr
					memPtr = memMgr->GetApple2MemPtr();				// main mem (E0)
					lineStartPtr = vrams_write->vram_shr + vramSHRInterlaceOffset + GetVramWidthSHR() * _TR_ANY_Y;
					
					goto DRAW_VRAM;
				}
			}
		}

		// reset the pointers
		memPtr = memMgr->GetApple2MemAuxPtr();
		lineStartPtr = vrams_write->vram_shr + GetVramWidthSHR() * _TR_ANY_Y;
		auto memInterlacePtr = memMgr->GetApple2MemPtr();
		uint8_t* lineInterlaceStartPtr = vrams_write->vram_shr + vramSHRInterlaceOffset + GetVramWidthSHR() * _TR_ANY_Y;

		switch (beamState)
		{
		case BeamState_e::UNKNOWN:
			break;
		case BeamState_e::NBHBLANK:
			// do nothing
			break;
		case BeamState_e::NBVBLANK:
			// do nothing
			break;
		case BeamState_e::BORDER_LEFT:
		case BeamState_e::BORDER_RIGHT:
		case BeamState_e::BORDER_TOP:
		case BeamState_e::BORDER_BOTTOM:
			memset(lineStartPtr + _COLORBYTESOFFSET + (_TR_ANY_X * 4), (uint8_t)memMgr->switch_c034, 4);
			if (bShouldPageDouble)
				memset(lineInterlaceStartPtr + _COLORBYTESOFFSET + (_TR_ANY_X * 4), (uint8_t)memMgr->switch_c034, 4);
			break;
		case BeamState_e::CONTENT:
		{
			if (_x < CYCLES_SC_HBL || _y >= mode_scanlines)
			{
				// Somehow in the middle of the frame the mode was switched, and we're beyond the
				// legacy content area. Disregard.
				break;
			}
			// Get the color info for the 4 bytes where the beam is
			auto xfb = (_x - CYCLES_SC_HBL) * 4;	// the x first byte, given that every beam cycle renders 4 bytes
			auto scb = lineStartPtr[0];
			memcpy(lineStartPtr + _COLORBYTESOFFSET + _TR_ANY_X * 4,
				memPtr + _A2VIDEO_SHR_START + _y * _A2VIDEO_SHR_BYTES_PER_LINE + xfb, 4);
			if (!(scb & 0x80u) && (scb & 0x20u))	// 320 mode and colorfill
			{
				// Pre-calculate colorfill, so that the shader doesn't have to do it
				// It's completely wasted on the shader. Here it's much more efficient
				for (uint32_t i = 0; i < 4; i++)
				{
					auto byteColor = lineStartPtr[_COLORBYTESOFFSET + (_TR_ANY_X * 4) + i];
					// if the first color of the byte is 0, give it the last color of the previous byte
					// assuming this is not the first byte of the line
					if (((byteColor & 0xF0) == 0) && ((xfb + i) != 0))
						byteColor |= (lineStartPtr[_COLORBYTESOFFSET + (_TR_ANY_X * 4) + i - 1] & 0b1111) << 4;
					// if the second color of the byte is 0, give it the first color of the byte
					if ((byteColor & 0x0F) == 0)
						byteColor |= (byteColor >> 4);
					lineStartPtr[_COLORBYTESOFFSET + (_TR_ANY_X * 4) + i] = byteColor;
				}
			}
			// Here deal with the new SHR4 mode PAL256, where each byte is an index into the full palette
			// of 256 colors. We have to do it here because the palette can be dynamically modified while
			// racing the beam.
			if (((scanlineSHR4Modes & A2_VSM_SHR4PAL256) != 0)
				|| (overrideSHRMode == A2_VSM_SHR4PAL256))
			{
				// calculate x value where x is 0-40 in the content area
				auto _x_just_content = _x - CYCLES_SC_HBL;
				auto pal256ByteStartPtr = vrams_write->vram_pal256 + (_y * _A2VIDEO_SHR_BYTES_PER_LINE + (4 * _x_just_content))*2;

				for (uint32_t i = 0; i < 4; i++)
				{
					// get the byte value, a pointer to the palette color
					auto byteColor = lineStartPtr[_COLORBYTESOFFSET + (_TR_ANY_X * 4) + i];
					// get the palette color (2 bytes), the palette being all 256 colors in a single palette
					auto paletteColorStart = memPtr + _A2VIDEO_SHR_PALETTE_START + ((uint32_t)byteColor * 2);
					pal256ByteStartPtr[2*i] = paletteColorStart[0];
					pal256ByteStartPtr[2*i + 1] = paletteColorStart[1];
				}
			}

			// Do the exact same thing for double SHR if necessary, getting the data from main RAM
			if (bShouldPageDouble)
			{
				scb = lineInterlaceStartPtr[0];
				memcpy(lineInterlaceStartPtr + _COLORBYTESOFFSET + _TR_ANY_X * 4,
					   memInterlacePtr + _A2VIDEO_SHR_START + _y * _A2VIDEO_SHR_BYTES_PER_LINE + xfb, 4);
				if (!(scb & 0x80u) && (scb & 0x20u))	// 320 mode and colorfill
				{
					// Pre-calculate colorfill, so that the shader doesn't have to do it
					// It's completely wasted on the shader. Here it's much more efficient
					for (uint32_t i = 0; i < 4; i++)
					{
						auto byteColor = lineInterlaceStartPtr[_COLORBYTESOFFSET + (_TR_ANY_X * 4) + i];
						// if the first color of the byte is 0, give it the last color of the previous byte
						// assuming this is not the first byte of the line
						if (((byteColor & 0xF0) == 0) && ((xfb + i) != 0))
							byteColor |= (lineInterlaceStartPtr[_COLORBYTESOFFSET + (_TR_ANY_X * 4) + i - 1] & 0b1111) << 4;
						// if the second color of the byte is 0, give it the first color of the byte
						if ((byteColor & 0x0F) == 0)
							byteColor |= (byteColor >> 4);
						lineInterlaceStartPtr[_COLORBYTESOFFSET + (_TR_ANY_X * 4) + i] = byteColor;
					}
				}
				// Here deal with the new SHR4 mode PAL256, where each byte is an index into the full palette
				// of 256 colors. We have to do it here because the palette can be dynamically modified while
				// racing the beam.
				if (((scanlineSHR4Modes & A2_VSM_SHR4PAL256) != 0)
					|| (overrideSHRMode == A2_VSM_SHR4PAL256))
				{
					// calculate x value where x is 0-40 in the content area
					// move to the offset to the interlace area which is the second half of the vram (i.e. slide down by _A2VIDEO_SHR_SCANLINES)
					auto _x_just_content = _x - CYCLES_SC_HBL;
					auto pal256ByteStartPtr = vrams_write->vram_pal256 + ((_y + _A2VIDEO_SHR_SCANLINES) * _A2VIDEO_SHR_BYTES_PER_LINE + (4 * _x_just_content))*2;

					for (uint32_t i = 0; i < 4; i++)
					{
						// get the byte value, a pointer to the palette color
						auto byteColor = lineInterlaceStartPtr[_COLORBYTESOFFSET + (_TR_ANY_X * 4) + i];
						// get the palette color (2 bytes), the palette being all 256 colors in a single palette
						auto paletteColorStart = memInterlacePtr + _A2VIDEO_SHR_PALETTE_START + ((uint32_t)byteColor * 2);
						pal256ByteStartPtr[2*i] = paletteColorStart[0];
						pal256ByteStartPtr[2*i + 1] = paletteColorStart[1];
					}
				}
			}	// end interlacing
		}
			break;
		default:
			break;
		}
		return;
	}	// if (memMgr->IsSoftSwitch(A2SS_SHR))


	// The byte isn't SHR, it's legacy
	// at least 1 byte in this vblank cycle is LEGACY
	// Note that overlay bytes are always legacy
	switch (vrams_write->mode)
	{
	case A2Mode_e::NONE:
		vrams_write->mode = A2Mode_e::LEGACY;
		break;
	case A2Mode_e::SHR:
		SwitchToMergedMode(_y);
		break;
	default:
		break;
	}
	// the flags byte is:
	// bits 0-2: mode (TEXT, DTEXT, LGR, DLGR, HGR, DHGR, DHGRMONO, BORDER)
	// bit 3: ALT charset for TEXT
	// bits 4-7: border color (like in the 2gs)
	uint8_t flags = 0;
	// the colors byte is:
	// bits 0-3: background color
	// bits 4-7: foreground color
	uint8_t colors = 0;
	
	bShouldPageDouble = (overrideLegacyPaging > 0 ? 1 : 0);

	switch (beamState)
	{
	case BeamState_e::UNKNOWN:
		break;
	case BeamState_e::NBHBLANK:
		// do nothing
		break;
	case BeamState_e::NBVBLANK:
		// do nothing
		break;
	case BeamState_e::BORDER_LEFT:
	case BeamState_e::BORDER_RIGHT:
	case BeamState_e::BORDER_TOP:
	case BeamState_e::BORDER_BOTTOM:
		// Legacy mode VRAM is 4 bytes (main, aux, flags, fg&bg colors)
		// Set byte 3 as border color in the top 4 bits, and mode BORDER in the lower 3 bits
		vrams_write->vram_legacy[(GetVramWidthLegacy() * _TR_ANY_Y + _TR_ANY_X) * 4 + 2] =
			(memMgr->switch_c034 << 4) + 0b111;
		break;
	case BeamState_e::CONTENT:
	{
		if (_x < CYCLES_SC_HBL || _y >= mode_scanlines)
		{
			// Somehow in the middle of the frame the mode was switched, and we're beyond the
			// legacy content area. Disregard.
			break;
		}
		// Set the mode, and depending on the mode, grab the bytes
		if (!memMgr->IsSoftSwitch(A2SS_TEXT))
		{
			if (memMgr->IsSoftSwitch(A2SS_MIXED) && _y > 159)	// check mixed mode
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
		}
		else { 	// Now check the text modes
			if (memMgr->IsSoftSwitch(A2SS_80COL))
				flags = 1;	// DTEXT
			else
			{
				flags = 0;	// TEXT
			}
		}

		// Fill in the rest of the flags. We already use bits 0-2 for the modes
		flags |= ((memMgr->IsSoftSwitch(A2SS_ALTCHARSET) ? 1 : 0) << 3);	// bit 3 is alt charset
		flags |= (memMgr->switch_c034 << 4);								// bits 4-7 are border color
		// and the colors
		colors = memMgr->switch_c022;
		// Check for page 2
		bool isPage2 = false;
		// Careful: it's only page 2 if 80STORE is off
		if (memMgr->IsSoftSwitch(A2SS_PAGE2) && !memMgr->IsSoftSwitch(A2SS_80STORE))
			isPage2 = true;

		// Finally set the 4 VRAM bytes
		// 4 bytes in VRAM for each beam byte
		uint8_t* byteStartPtr = vrams_write->vram_legacy +
			(GetVramWidthLegacy() * _TR_ANY_Y + _TR_ANY_X) * 4;
		auto _vramInterlaceOffset = GetVramSizeLegacy() / _INTERLACE_MULTIPLIER;	// Offset to 2nd half of the vram
		uint8_t* byteStartPtrInterlace = byteStartPtr + _vramInterlaceOffset;	// For paged mode
		uint32_t startMem;
		
		// Determine where in memory we should get the data from, and get it
		if ((flags & 0b111) < 4)	// D/TEXT AND D/LGR
		{
			startMem = _A2VIDEO_TEXT1_START;
			if (((flags & 0b111) < 3) && isPage2)		// check for page 2 (DLGR doesn't have it)
				startMem = _A2VIDEO_TEXT2_START;
			byteStartPtr[0] = *(memMgr->GetApple2MemPtr() + startMem + g_RAM_TEXTOffsets[_y / 8] + (_x - CYCLES_SC_HBL));
			byteStartPtr[1] = *(memMgr->GetApple2MemAuxPtr() + startMem + g_RAM_TEXTOffsets[_y / 8] + (_x - CYCLES_SC_HBL));
			if (bShouldPageDouble)
			{
				startMem = _A2VIDEO_TEXT2_START;		// does nothing if already in page 2
				byteStartPtrInterlace[0] = *(memMgr->GetApple2MemPtr() + startMem + g_RAM_TEXTOffsets[_y / 8] + (_x - CYCLES_SC_HBL));
				byteStartPtrInterlace[1] = *(memMgr->GetApple2MemAuxPtr() + startMem + g_RAM_TEXTOffsets[_y / 8] + (_x - CYCLES_SC_HBL));
			}
		}
		else {		// D/HIRES
			startMem = _A2VIDEO_HGR1_START;
			if (isPage2)
				startMem = _A2VIDEO_HGR2_START;
			byteStartPtr[0] = *(memMgr->GetApple2MemPtr() + startMem + g_RAM_HGROffsets[_y] + (_x - CYCLES_SC_HBL));
			byteStartPtr[1] = *(memMgr->GetApple2MemAuxPtr() + startMem + g_RAM_HGROffsets[_y] + (_x - CYCLES_SC_HBL));
			if (bShouldPageDouble)
			{
				startMem = _A2VIDEO_HGR2_START;		// does nothing if already in page 2
				byteStartPtrInterlace[0] = *(memMgr->GetApple2MemPtr() + startMem + g_RAM_HGROffsets[_y] + (_x - CYCLES_SC_HBL));
				byteStartPtrInterlace[1] = *(memMgr->GetApple2MemAuxPtr() + startMem + g_RAM_HGROffsets[_y] + (_x - CYCLES_SC_HBL));
			}
		}
		byteStartPtr[2] = flags;
		byteStartPtr[3] = colors;
		if (bShouldPageDouble)
		{
			byteStartPtrInterlace[2] = flags;
			byteStartPtrInterlace[3] = colors;
		}

		// Generate the debug VRAMs if necessary
		if (bRenderTEXT1)
		{
			byteStartPtr = vrams_write->vram_forced_text1 + (40 * _y + _x - CYCLES_SC_HBL) * 4;
			startMem = _A2VIDEO_TEXT1_START;
			byteStartPtr[0] = *(memMgr->GetApple2MemPtr() + startMem + g_RAM_TEXTOffsets[_y / 8] + (_x - CYCLES_SC_HBL));
			byteStartPtr[1] = *(memMgr->GetApple2MemAuxPtr() + startMem + g_RAM_TEXTOffsets[_y / 8] + (_x - CYCLES_SC_HBL));
			byteStartPtr[2] = (flags & 0b1111'1000) | 0;	// force TEXT
			byteStartPtr[3] = colors;
		}
		if (bRenderTEXT2)
		{
			byteStartPtr = vrams_write->vram_forced_text2 + (40 * _y + _x - CYCLES_SC_HBL) * 4;
			startMem = _A2VIDEO_TEXT2_START;
			byteStartPtr[0] = *(memMgr->GetApple2MemPtr() + startMem + g_RAM_TEXTOffsets[_y / 8] + (_x - CYCLES_SC_HBL));
			byteStartPtr[1] = *(memMgr->GetApple2MemAuxPtr() + startMem + g_RAM_TEXTOffsets[_y / 8] + (_x - CYCLES_SC_HBL));
			byteStartPtr[2] = (flags & 0b1111'1000) | 0;	// force TEXT
			byteStartPtr[3] = colors;
		}
		if (bRenderHGR1)
		{
			byteStartPtr = vrams_write->vram_forced_hgr1 + (40 * _y + _x - CYCLES_SC_HBL) * 4;
			startMem = _A2VIDEO_HGR1_START;
			byteStartPtr[0] = *(memMgr->GetApple2MemPtr() + startMem + g_RAM_HGROffsets[_y] + (_x - CYCLES_SC_HBL));
			byteStartPtr[1] = *(memMgr->GetApple2MemAuxPtr() + startMem + g_RAM_HGROffsets[_y] + (_x - CYCLES_SC_HBL));
			byteStartPtr[2] = (flags & 0b1111'1000) | 4;	// force HGR
			byteStartPtr[3] = colors;
		}
		if (bRenderHGR2)
		{
			byteStartPtr = vrams_write->vram_forced_hgr2 + (40 * _y + _x - CYCLES_SC_HBL) * 4;
			startMem = _A2VIDEO_HGR2_START;
			byteStartPtr[0] = *(memMgr->GetApple2MemPtr() + startMem + g_RAM_HGROffsets[_y] + (_x - CYCLES_SC_HBL));
			byteStartPtr[1] = *(memMgr->GetApple2MemAuxPtr() + startMem + g_RAM_HGROffsets[_y] + (_x - CYCLES_SC_HBL));
			byteStartPtr[2] = (flags & 0b1111'1000) | 4;	// force HGR
			byteStartPtr[3] = colors;
		}
	}
		break;
	default:
		break;
	}
}

// When we learn we're in merged mode, we need to update all previous scanlines
// to set the offset texture data. Ideally it should all be one integrated mode
// between legacy and SHR. But then we'd have to always update the offset texture
// to handle the wobble and remember the state, etc... That's a lot of work every
// scanline for the _very_ rare case of merged mode which is only used in demos.
void A2VideoManager::SwitchToMergedMode(uint32_t scanline)
{
	if (bIsSwitchingToMergedMode)
		return;
	bIsSwitchingToMergedMode = true;
	// set the vrams write mode, and flip the A2SS_SHR softswitch to the previous
	// setting to rerun the scanlines, before returning it to its current state
	vrams_write->mode = A2Mode_e::MERGED;
	auto memMgr = MemoryManager::GetInstance();
	memMgr->SetSoftSwitch(A2SS_SHR, !memMgr->IsSoftSwitch(A2SS_SHR));
	auto totalscanlines = (current_region == VideoRegion_e::NTSC ? SC_TOTAL_NTSC : SC_TOTAL_PAL);
	int starty = _SCANLINE_START_FRAME + 2;
	beamState = BeamState_e::NBVBLANK;
	for (uint32_t y = starty; y < totalscanlines; y++)
	{
		for (uint32_t x = 0; x < 65; x++)
		{
			this->BeamIsAtPosition(x, y);
		}
	}
	for (uint32_t y = 0; y < scanline; y++)
	{
		for (uint32_t x = 0; x < 65; x++)
		{
			this->BeamIsAtPosition(x, y);
		}
	}
	memMgr->SetSoftSwitch(A2SS_SHR, !memMgr->IsSoftSwitch(A2SS_SHR));
	bIsSwitchingToMergedMode = false;
}

void A2VideoManager::ForceBeamFullScreenRender(const uint64_t numFrames)
{
	// Move the beam over the whole screen
	auto totalscanlines = (current_region == VideoRegion_e::NTSC ? SC_TOTAL_NTSC : SC_TOTAL_PAL);
	// Start 2 lines after the frame flip in the VBLANK non-border area
	// and end 1 line after the frame flip, so we guarantee a clean frame flip
	int starty = _SCANLINE_START_FRAME + 2;
	beamState = BeamState_e::NBVBLANK;

	for (uint32_t y = starty; y < totalscanlines * numFrames; y++)
	{
		for (uint32_t x = 0; x < 65; x++)
		{
			this->BeamIsAtPosition(x, y);
		}
	}
	for (uint32_t y = 0; y < starty; y++)
	{
		// For testing the merged mode
		if (bDEMOMergedMode)
		{
			if (y == 50)
				MemoryManager::GetInstance()->SetSoftSwitch(A2SS_SHR, !MemoryManager::GetInstance()->IsSoftSwitch(A2SS_SHR));
			if (y == 130)
				MemoryManager::GetInstance()->SetSoftSwitch(A2SS_SHR, !MemoryManager::GetInstance()->IsSoftSwitch(A2SS_SHR));
		}
		for (uint32_t x = 0; x < 65; x++)
		{
			this->BeamIsAtPosition(x, y);
		}
	}
	// std::cerr << "finished FBFSR" << std::endl;
	// the y value _SCANLINE_START_FRAME flips the frame

}

bool A2VideoManager::SelectLegacyShader(const int index)
{
	switch (index)
	{
	case 0:		// full
		windowsbeam[A2VIDEOBEAM_LEGACY]->SetShaderPrograms(_SHADER_A2_VERTEX_DEFAULT, _SHADER_BEAM_LEGACY_FRAGMENT);
		break;
	default:
		return false;
	}
	return true;
}

bool A2VideoManager::SelectSHRShader(const int index)
{
	switch (index)
	{
	case 0:		// full
		windowsbeam[A2VIDEOBEAM_SHR]->SetShaderPrograms(_SHADER_A2_VERTEX_DEFAULT, _SHADER_BEAM_SHR_FRAGMENT);
		break;
	default:
		return false;
	}
	return true;
}

uXY A2VideoManager::ScreenSize()
{
	return uXY({ (uint32_t)fb_width, (uint32_t)fb_height});
}

void A2VideoManager::PrepareOffsetTexture() {
	// Associate the texture OFFSETTEX in TEXUNIT_DATABUFFER with the buffer
	// This is the x offset to slide each of legacy or shr when the mode switches
	// in order to simulate the sine wobble from the analog change between 14 and 16MHz
	if (!offsetTextureExists) {
		// Generate the texture if it doesn't exist
		if (OFFSETTEX == UINT_MAX)
			glGenTextures(1, &OFFSETTEX);
		glActiveTexture(_TEXUNIT_MERGE_OFFSET);
		glBindTexture(GL_TEXTURE_2D, OFFSETTEX);

		// Set texture parameters
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

		// Create the texture
		// Adjust the unpack alignment for textures with arbitrary widths
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, 1, GetVramHeightSHR(), 0, GL_RED, GL_FLOAT, vrams_read->offset_buffer);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

		offsetTextureExists = true;
	}
	else {
		// Update the texture
		glActiveTexture(_TEXUNIT_MERGE_OFFSET);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, GetVramHeightSHR(), GL_RED, GL_FLOAT, vrams_read->offset_buffer);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	}
}

void A2VideoManager::CreateOrResizeFramebuffer(int fb_width, int fb_height)
{
	// Create FBO and texture if they haven't been created yet
	if (FBO_A2Video == UINT_MAX)
	{
		glGenFramebuffers(1, &FBO_A2Video);
		glGenTextures(1, &a2video_texture_id);
		glGenFramebuffers(1, &FBO_NTSC);
		glGenTextures(1, &ntsc_texture_id);
	}

	// Create all the debug FBOs and textures, those have a static size and can be generated once only
	if (FBO_debug[0] == UINT_MAX)
	{
		glGenFramebuffers(4, FBO_debug);
		glGenTextures(4, debug_texture_id);
		for (int i = 0; i < 4; i++)
		{
			glBindFramebuffer(GL_FRAMEBUFFER, FBO_debug[i]);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, debug_texture_id[i]);

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, _A2VIDEO_LEGACY_WIDTH, _A2VIDEO_LEGACY_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, debug_texture_id[i], 0);

			GLenum _statusFBO = glCheckFramebufferStatus(GL_FRAMEBUFFER);
			if (_statusFBO != GL_FRAMEBUFFER_COMPLETE)
			{
				std::cerr << "Debug Framebuffer " << i << " is not complete: " << _statusFBO << std::endl;
			}
		}
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	// Generate the Apple 2 memory texture which will hold the Apple 2 memory
	// for the shaders to use
	if (APPLE2MEMORYTEX == UINT_MAX)
	{
		GLint glerr;
		glGenTextures(1, &APPLE2MEMORYTEX);
		if ((glerr = glGetError()) != GL_NO_ERROR) {
			std::cerr << "OpenGL glGenTextures APPLE2MEMORYTEX error: " << glerr << std::endl;
		}
		glActiveTexture(_TEXUNIT_APPLE2MEMORY_R8UI);
		glBindTexture(GL_TEXTURE_2D, APPLE2MEMORYTEX);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		if ((glerr = glGetError()) != GL_NO_ERROR) {
			std::cerr << "OpenGL APPLE2MEMORYTEX error: " << glerr << std::endl;
		}

		// Apple 2 memory texture: R8UI, 128k of RAM max, 1024 bytes per line
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R8UI, 1024, 128,
					 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE,
					 MemoryManager::GetInstance()->GetApple2MemPtr());
		if ((glerr = glGetError()) != GL_NO_ERROR) {
			std::cerr << "A2VideoManager APPLE2MEMORYTEX glTexImage2D error: " << glerr << std::endl;
		}

	}

	// for when NTSC is requested for legacy,
	// there's a pre-render pass into _TEXUNIT_PRE_NTSC
	glActiveTexture(_TEXUNIT_PRE_NTSC);
	glBindTexture(GL_TEXTURE_2D, ntsc_texture_id);

	// this will go to postprocessing
	glActiveTexture(_TEXUNIT_POSTPROCESS);
	glBindTexture(GL_TEXTURE_2D, a2video_texture_id);

	// Check if the textures already have the desired dimensions.
	GLint width = 0, height = 0;
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);

	if (width == fb_width && height == fb_height)
	{
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, 0);
		return;
	}

	// -------------------------
	// Setup framebuffer objects
	// -------------------------
	glBindFramebuffer(GL_FRAMEBUFFER, FBO_A2Video);		// in linear RGBA8 mode
	//glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, a2video_texture_id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, fb_width, fb_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, a2video_texture_id, 0);
	GLenum _statusFBO = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (_statusFBO != GL_FRAMEBUFFER_COMPLETE)
	{
		std::cerr << "Framebuffer A2Video is not complete: " << _statusFBO << std::endl;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, FBO_NTSC);		// in linear RGBA8 mode
	//glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, ntsc_texture_id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, fb_width, fb_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ntsc_texture_id, 0);
	_statusFBO = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (_statusFBO != GL_FRAMEBUFFER_COMPLETE)
	{
		std::cerr << "Framebuffer NTSC is not complete: " << _statusFBO << std::endl;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);

	// std::cerr << "Generated two FBOs with size " << fb_width << " x " << fb_height << std::endl;
}

bool A2VideoManager::Render(GLuint &_texUnit)
{
	// We first render both the legacy and shr "windows" as textures
	// (which ever one of the two, or both, is enabled for this frame)
	// and then merge them into one using a merge shader. If only a single
	// mode is active, then just pass its output texture to the postprocessor
	// directly and avoid all the mess, unless the user has requested that
	// the legacy mode use 16MHz instead of 14MHz for width.

	if ((!bIsReady) || (!bA2VideoEnabled) || bIsRebooting)
	{
		_texUnit = A2VIDEORENDER_ERROR;
		return false;
	}

	// Exit if we've already rendered the buffer
	if ((rendered_frame_idx == vrams_read->frame_idx) && !bAlwaysRenderBuffer)
	{
		_texUnit = _TEXUNIT_POSTPROCESS;
		return false;
	}
	if ((rendered_frame_idx == vrams_read->frame_idx) && bAlwaysRenderBuffer)
		this->ForceBeamFullScreenRender();

	GLenum glerr;

	// Initialization routine runs only once on init (or re-init)
	if (bShouldInitializeRender) {
		bShouldInitializeRender = false;

		if (font_rom_regular_idx >= font_roms_array.size())
			font_rom_regular_idx = 0;
		if (font_rom_regular_idx >= font_roms_array.size())
			font_rom_alternate_idx = (int)font_roms_array.size() - 1;
		// image asset 0: The apple 2e US font
		glActiveTexture(_TEXUNIT_IMAGE_FONT_ROM_DEFAULT);
		image_assets[0].AssignByFilename(std::string(fontpath).append("/").append(font_roms_array[font_rom_regular_idx]).c_str());
		// image asset 1: The alternate font
		glActiveTexture(_TEXUNIT_IMAGE_FONT_ROM_ALTERNATE);
		image_assets[1].AssignByFilename(std::string(fontpath).append("/").append(font_roms_array[font_rom_alternate_idx]).c_str());
		// image asset 2: LGR texture (overkill for color, useful for dithered b/w)
		glActiveTexture(_TEXUNIT_IMAGE_COMPOSITE_LGR);
		image_assets[2].AssignByFilename("assets/Texture_composite_lgr.png");
		// image asset 3: HGR texture
		glActiveTexture(_TEXUNIT_IMAGE_COMPOSITE_HGR);
		image_assets[3].AssignByFilename("assets/Texture_composite_hgr.png");
		// image asset 4: DHGR texture
		glActiveTexture(_TEXUNIT_IMAGE_COMPOSITE_DHGR);
		image_assets[4].AssignByFilename("assets/Texture_composite_dhgr.png");
		// image asset 5: 8x8 font for VidHD text modes
		glActiveTexture(_TEXUNIT_IMAGE_FONT_VIDHD_8X8);
		image_assets[5].AssignByFilename("assets/VidHDFont8x8/00_US-Default.png");
		if ((glerr = glGetError()) != GL_NO_ERROR) {
			std::cerr << "OpenGL AssignByFilename error: "
			<< 0 << " - " << glerr << std::endl;
		}

		glActiveTexture(GL_TEXTURE0);
		rendered_frame_idx = UINT64_MAX;
	}

	// ===============================================================================
	// ============================ SET FRAMEBUFFER SIZE =============================
	// ===============================================================================

	// Framebuffer size will change depending on the layers that are rendered:
	// If a VidHD text modes > 80x24 is active, then 1920x1080
	// Otherwise if it's a pure legacy frame, use legacy size
	// Otherwise if it's a pure SHR frame, use SHR size
	// Otherwise in merged SHR+Legacy mode it depends on the user preferences. The
	// problems are 2-fold: first, the width. Users may prefer the Legacy to use the
	// full SHR width, but it any case the quad needs to be full width to accommodate
	// the wobble effect if needed. And second, the vertical position. When both quads
	// are centered, the legacy is too low by 4 scanlines.
	// In SHR+Legacy the legacy shader will take care of centering horizontally the
	// legacy image within the quad and handle the wobble.

	SDL_FRect _legacyQuad = { 0, 0, (float)windowsbeam[A2VIDEOBEAM_LEGACY]->GetWidth(), (float)windowsbeam[A2VIDEOBEAM_LEGACY]->GetHeight() };
	SDL_FRect _legacyQuadWide = { 0, 0, (float)windowsbeam[A2VIDEOBEAM_SHR]->GetWidth(), (float)windowsbeam[A2VIDEOBEAM_LEGACY]->GetHeight() };
	SDL_FRect _shrQuad = { 0, 0, (float)windowsbeam[A2VIDEOBEAM_SHR]->GetWidth(), (float)windowsbeam[A2VIDEOBEAM_SHR]->GetHeight() };
	SDL_FRect _vidHdLegacyQuad = { 0, 0, (float)_A2VIDEO_LEGACY_WIDTH, (float)_A2VIDEO_LEGACY_HEIGHT };

	// Note: we always double the scanline value, so 4*2 for the shift
	SDL_FPoint _scanlineOffset = {0.f,0.f};
	if (bAlignQuadsToScanline)
		_scanlineOffset.y = 8.f;


	if (vidhdWindowBeam->GetVideoMode() > VIDHDMODE_TEXT_80X24) {	// Force 1080p size
		fb_width = vidhdWindowBeam->GetWidth();
		fb_height = vidhdWindowBeam->GetHeight();
		auto _rb = NormalizePixelQuad(CenteredQuadInFramebufferWithOffset(_legacyQuad, _scanlineOffset));
		windowsbeam[A2VIDEOBEAM_LEGACY]->SetQuadRelativeBounds(_rb);
		_rb = NormalizePixelQuad(CenteredQuadInFramebuffer(_shrQuad));
		windowsbeam[A2VIDEOBEAM_SHR]->SetQuadRelativeBounds(_rb);
		vidhdWindowBeam->SetQuadRelativeBounds({ -1.f, 1.f, 2.f, -2.f });
	} else {
		if (vrams_read->mode == A2Mode_e::LEGACY) {
			fb_width = windowsbeam[A2VIDEOBEAM_LEGACY]->GetWidth();
			if (bAlignQuadsToScanline) {
				fb_height = windowsbeam[A2VIDEOBEAM_SHR]->GetHeight();
				auto _rb = NormalizePixelQuad(CenteredQuadInFramebufferWithOffset(_legacyQuad, _scanlineOffset));
				windowsbeam[A2VIDEOBEAM_LEGACY]->SetQuadRelativeBounds(_rb);
			} else {
				fb_height = windowsbeam[A2VIDEOBEAM_LEGACY]->GetHeight();
				windowsbeam[A2VIDEOBEAM_LEGACY]->SetQuadRelativeBounds({ -1.f, 1.f, 2.f, -2.f });
			}
		} else if (vrams_read->mode == A2Mode_e::SHR) {
			fb_width = windowsbeam[A2VIDEOBEAM_SHR]->GetWidth();
			fb_height = windowsbeam[A2VIDEOBEAM_SHR]->GetHeight();
			windowsbeam[A2VIDEOBEAM_SHR]->SetQuadRelativeBounds({ -1.f, 1.f, 2.f, -2.f });
		} else {
			fb_width = windowsbeam[A2VIDEOBEAM_SHR]->GetWidth();
			fb_height = windowsbeam[A2VIDEOBEAM_SHR]->GetHeight();
			SDL_FRect _rb;
			_rb = NormalizePixelQuad(CenteredQuadInFramebufferWithOffset(_legacyQuadWide, _scanlineOffset));
			windowsbeam[A2VIDEOBEAM_LEGACY]->SetQuadRelativeBounds(_rb);
			windowsbeam[A2VIDEOBEAM_SHR]->SetQuadRelativeBounds({ -1.f, 1.f, 2.f, -2.f });
		}
		// add the vidhd layer
		if (vidhdWindowBeam->GetVideoMode() > VIDHDMODE_NONE) {
			auto _rb = NormalizePixelQuad(CenteredQuadInFramebufferWithOffset(_vidHdLegacyQuad, _scanlineOffset));
			vidhdWindowBeam->SetQuadRelativeBounds(_rb);
		}
	}

	CreateOrResizeFramebuffer(fb_width, fb_height);
	glBindFramebuffer(GL_FRAMEBUFFER, FBO_A2Video);

	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "GL Framebuffer error: " << glerr << std::endl;
	}

	glGetIntegerv(GL_VIEWPORT, last_viewport);	// remember existing viewport to restore it later
	glViewport(0, 0, fb_width, fb_height);		// new viewport the size of the output texture

	glClearColor(0.f, 0.f, 0.f, 0.f);
	glClear(GL_COLOR_BUFFER_BIT);


	// Set the magic bytes, currently only in SHR mode
	// This is where we handle the SHR modes override, telling the beam renderer what to do
	switch (overrideSHRMode)
	{
		case A2_VSM_SHR4SHR:
		case A2_VSM_SHR4RGGB:
		case A2_VSM_SHR4PAL256:
		case A2_VSM_SHR4R4G4B4:
		case A2_VSM_3200SHR:
			windowsbeam[A2VIDEOBEAM_SHR]->specialModesMask = overrideSHRMode;
			break;
		default:	// no override
			windowsbeam[A2VIDEOBEAM_SHR]->specialModesMask = vrams_read->frameSHRModes;
			break;
	}
	windowsbeam[A2VIDEOBEAM_SHR]->doubleSHR4 = ( overrideDoubleSHR > 0 ? overrideDoubleSHR - 1 : vrams_read->pagedMode);

	// if we're in merged mode, prepare the offset texture
	if (vrams_read->mode == A2Mode_e::MERGED)
		PrepareOffsetTexture();

	// ===============================================================================
	// ============================= LEGACY MODE RENDER ==============================
	// ===============================================================================
	if ((vrams_read->mode == A2Mode_e::LEGACY) || (vrams_read->mode == A2Mode_e::MERGED)) {
		// Only legacy is active, just bind the correct output for the postprocessor
		if (bUseDHGRCOL140Mixed)
			windowsbeam[A2VIDEOBEAM_LEGACY]->specialModesMask |= A2_VSM_DHGRCOL140Mixed;
		else
			windowsbeam[A2VIDEOBEAM_LEGACY]->specialModesMask &= ~A2_VSM_DHGRCOL140Mixed;
		if (bUseHGRSPEC1)
			windowsbeam[A2VIDEOBEAM_LEGACY]->specialModesMask |= A2_VSM_HGRSPEC1;
		else
			windowsbeam[A2VIDEOBEAM_LEGACY]->specialModesMask &= ~A2_VSM_HGRSPEC1;
		if (bUseHGRSPEC2)
			windowsbeam[A2VIDEOBEAM_LEGACY]->specialModesMask |= A2_VSM_HGRSPEC2;
		else
			windowsbeam[A2VIDEOBEAM_LEGACY]->specialModesMask &= ~A2_VSM_HGRSPEC2;

		windowsbeam[A2VIDEOBEAM_LEGACY]->monitorColorType = eA2MonitorType;
		windowsbeam[A2VIDEOBEAM_LEGACY]->bIsMergedMode = (vrams_read->mode == A2Mode_e::MERGED);
		windowsbeam[A2VIDEOBEAM_LEGACY]->bForceSHRWidth = bForceSHRWidth;

		// If NTSC output for legacy is requested (only for color monitors), render first into this texture
		// and then apply the NTSC shader to this texture into the FBO_A2Video
		if (p_b_ntsc && (eA2MonitorType == A2_MON_COLOR))
		{
			glBindFramebuffer(GL_FRAMEBUFFER, FBO_NTSC);
			glViewport(0, 0, fb_width, fb_height);
			glClearColor(0.f, 0.f, 0.f, 0.f);
			glClear(GL_COLOR_BUFFER_BIT);
		}
		windowsbeam[A2VIDEOBEAM_LEGACY]->Render(current_frame_idx);
		if (p_b_ntsc && (eA2MonitorType == A2_MON_COLOR))
		{
			glBindFramebuffer(GL_FRAMEBUFFER, FBO_A2Video);

			legacyNTSCQuad->SetInputTextureUnit(_TEXUNIT_PRE_NTSC);
			if (windowsbeam[A2VIDEOBEAM_LEGACY]->GetWidth() != legacyNTSCQuad->GetWidth() ||
				windowsbeam[A2VIDEOBEAM_LEGACY]->GetHeight() != legacyNTSCQuad->GetHeight())
			{
				legacyNTSCQuad->SetScreenCount(
					windowsbeam[A2VIDEOBEAM_LEGACY]->GetWidth(),
					windowsbeam[A2VIDEOBEAM_LEGACY]->GetHeight()
				);
			}
			auto _s = legacyNTSCQuad->shader;
			_s.Use();
			_s.SetUniform("bNOFILTERMONO", eA2MonitorType == A2_MON_COLOR ? p_b_ntscNoFilterMono : true);
			_s.SetUniform("NTSC_STR", p_f_ntscStrength);
			_s.SetUniform("NTSC_COMB_STR", p_f_ntscCombStrength);
			_s.SetUniform("NTSC_GAMMA_CORRECTION", p_f_ntscGammaCorrection);
			legacyNTSCQuad->Render(current_frame_idx);
		}
		// std::cerr << "Rendered legacy to viewport " << fb_width << "x" << fb_height << " - " << current_frame_idx << std::endl;
		if ((glerr = glGetError()) != GL_NO_ERROR) {
			std::cerr << "Legacy Mode draw error: " << glerr << std::endl;
		}
	}
	// ===============================================================================
	// =============================== SHR MODE RENDER ===============================
	// ===============================================================================
	if ((vrams_read->mode == A2Mode_e::SHR) || (vrams_read->mode == A2Mode_e::MERGED)) {
		// Only SHR is active, just bind the correct output for the postprocessor
		windowsbeam[A2VIDEOBEAM_SHR]->monitorColorType = eA2MonitorType;
		windowsbeam[A2VIDEOBEAM_SHR]->bIsMergedMode = (vrams_read->mode == A2Mode_e::MERGED);
		windowsbeam[A2VIDEOBEAM_SHR]->Render(current_frame_idx);
		// std::cerr << "Rendered SHR to viewport " << fb_width << "x" << fb_height << " - " << current_frame_idx << std::endl;
		if ((glerr = glGetError()) != GL_NO_ERROR) {
			std::cerr << "SHR Mode draw error: " << glerr << std::endl;
		}
	}
	// ===============================================================================
	// ============================ VIDHD LAYER RENDER ===============================
	// ===============================================================================
	if (vidhdWindowBeam->GetVideoMode() != VIDHDMODE_NONE)	// VidHD
	{
		vidhdWindowBeam->Render();
		if ((glerr = glGetError()) != GL_NO_ERROR) {
			std::cerr << "VidHD draw error: " << glerr << std::endl;
		}
	}

	glActiveTexture(_TEXUNIT_POSTPROCESS);
	glBindTexture(GL_TEXTURE_2D, a2video_texture_id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glGenerateMipmap(GL_TEXTURE_2D);

	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "A2VideoManager Bind Texture error: " << glerr << std::endl;
	}

	// cleanup
	glActiveTexture(GL_TEXTURE0);
	glUseProgram(0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(last_viewport[0], last_viewport[1], (GLsizei)last_viewport[2], (GLsizei)last_viewport[3]);
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "A2VideoManager glViewport error: " << glerr << std::endl;
	}
	
	// Render the debugging textures as necessary
	if (bRenderTEXT1) {
		glViewport(0, 0, _A2VIDEO_LEGACY_WIDTH, _A2VIDEO_LEGACY_HEIGHT);
		glBindFramebuffer(GL_FRAMEBUFFER, FBO_debug[0]);
		windowsbeam[A2VIDEOBEAM_FORCED_TEXT1]->Render(current_frame_idx);
	}
	if (bRenderTEXT2) {
		glViewport(0, 0, _A2VIDEO_LEGACY_WIDTH, _A2VIDEO_LEGACY_HEIGHT);
		glBindFramebuffer(GL_FRAMEBUFFER, FBO_debug[1]);
		windowsbeam[A2VIDEOBEAM_FORCED_TEXT2]->Render(current_frame_idx);
	}
	if (bRenderHGR1) {
		glViewport(0, 0, _A2VIDEO_LEGACY_WIDTH, _A2VIDEO_LEGACY_HEIGHT);
		glBindFramebuffer(GL_FRAMEBUFFER, FBO_debug[2]);
		windowsbeam[A2VIDEOBEAM_FORCED_HGR1]->Render(current_frame_idx);
	}
	if (bRenderHGR2) {
		glViewport(0, 0, _A2VIDEO_LEGACY_WIDTH, _A2VIDEO_LEGACY_HEIGHT);
		glBindFramebuffer(GL_FRAMEBUFFER, FBO_debug[3]);
		windowsbeam[A2VIDEOBEAM_FORCED_HGR2]->Render(current_frame_idx);
	}
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "A2VideoManager debugging textures render error: " << glerr << std::endl;
	}

	// Update _TEXUNIT_APPLE2MEMORY_R8UI
	// This is the apple 2's memory which is mapped to a "texture"

	if (!v_debug_rgb_windows.empty())
	{
		// Update _TEXUNIT_APPLE2MEMORY_R8UI
		// This is the apple 2's memory which is mapped to a "texture"

		glActiveTexture(_TEXUNIT_APPLE2MEMORY_R8UI);
		glBindTexture(GL_TEXTURE_2D, APPLE2MEMORYTEX);
		// Adjust the unpack alignment for textures with arbitrary widths
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		// Always send through the whole memory, which is
		// 2x(_A2_MEMORY_SHADOW_END-_A2_MEMORY_SHADOW_BEGIN)
		// for both main and aux. Optimizing for different legacy modes
		// is unnecessary added code for 128k (at most) of memory
		glTexSubImage2D(
						GL_TEXTURE_2D, 0,
						/* xoffset */ 0, /* yoffset */ 0,
						1024, (_A2_MEMORY_SHADOW_END-_A2_MEMORY_SHADOW_BEGIN)*2/1024,
						GL_RED_INTEGER, GL_UNSIGNED_BYTE,
						MemoryManager::GetInstance()->GetApple2MemPtr()
						);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
		/*
		// to check if the texture was properly upladed:
		std::vector<uint8_t> back(1024 * (_A2_MEMORY_SHADOW_END-_A2_MEMORY_SHADOW_BEGIN)*2/1024);
		glGetTexImage(GL_TEXTURE_2D, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, back.data());
		*/
		glActiveTexture(GL_TEXTURE0);
		if ((glerr = glGetError()) != GL_NO_ERROR) {
			std::cerr << "A2VideoManager APPLE2MEMORYTEX glTexImage2D error: " << glerr << std::endl;
		}

		// Render all debug RGB textures
		for (auto& debugWin : v_debug_rgb_windows) {
			debugWin.Render();
		}
	}

	// all done, the texture for this Apple 2 beam cycle frame is rendered
	rendered_frame_idx = vrams_read->frame_idx;
	vrams_read->bWasRendered = true;

	_texUnit = _TEXUNIT_POSTPROCESS;
	return true;
}

GLuint A2VideoManager::GetOutputTextureId()
{
	return a2video_texture_id;
}

///
///
/// ImGUI Interface
///
///

void A2VideoManager::DisplayCharRomsImGuiChunk()
{
	if (ImGui::BeginMenu("Regular"))
	{
		for (int i = 0; i < font_roms_array.size(); ++i)
		{
			if (ImGui::MenuItem(font_roms_array[i].c_str(), "", i==font_rom_regular_idx))
			{
				font_rom_regular_idx = i;
				bShouldInitializeRender = true;
			}
		}
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("Alternate"))
	{
		for (int i = 0; i < font_roms_array.size(); ++i)
		{
			if (ImGui::MenuItem(font_roms_array[i].c_str(), "", i==font_rom_alternate_idx))
			{
				font_rom_alternate_idx = i;
				bShouldInitializeRender = true;
			}
		}
		ImGui::EndMenu();
	}
}

void A2VideoManager::DisplayImGuiLoadFileWindow(bool* p_open)
{
	bImguiLoadFileWindowIsOpen = p_open;
	if (p_open) {
		ImGui::SetNextWindowSizeConstraints(ImVec2(300, 100), ImVec2(FLT_MAX, FLT_MAX));
		ImGui::Begin("Load File into Memory", p_open);
		if (!ImGui::IsWindowCollapsed())
		{
			ImGui::Text("Load Memory Start: ");
			ImGui::SameLine();
			ImGui::PushItemWidth(120);
			ImGui::InputInt("##mem_load", &iImguiMemLoadPosition, 1, 1024, ImGuiInputTextFlags_CharsHexadecimal);
			ImGui::PopItemWidth();
			iImguiMemLoadPosition = std::clamp(iImguiMemLoadPosition, 0, 0xFFFF);
			ImGui::Checkbox("AUX Bank", &bImguiMemLoadAuxBank);
			ImGui::SameLine(); ImGui::Text("   "); ImGui::SameLine();
			if (MemoryLoadUsingDialog(iImguiMemLoadPosition, bImguiMemLoadAuxBank, sImguiLoadPath))
			{
				// Force a double render because the top border is part of the previous beam scan
				this->ForceBeamFullScreenRender();
				this->ForceBeamFullScreenRender();
			}
		}
		ImGui::End();
	}
}

void A2VideoManager::DisplayImGuiExtraWindows()
{
	// Show the VRAM legacy window
	if (mem_edit_vram_legacy.Open)
	{
		mem_edit_vram_legacy.DrawWindow("Memory Editor: Beam VRAM Legacy", this->GetLegacyVRAMWritePtr(), this->GetVramSizeLegacy());
	}
	
	// Show the VRAM SHR window
	if (mem_edit_vram_shr.Open)
	{
		mem_edit_vram_shr.DrawWindow("Memory Editor: Beam VRAM SHR", this->GetSHRVRAMWritePtr(), this->GetVramSizeSHR());
	}
	
	// Show the merge offset window
	if (mem_edit_offset_buffer.Open)
	{
		mem_edit_offset_buffer.DrawWindow("Memory Editor: Offset Buffer", (void*)this->GetOffsetBufferReadPtr(), this->GetVramHeightSHR());
	}
	
	// Show extra render windows
	// The extra windows may not have been rendered yet so we may need to force a re-render
	float _debug_aspect_ratio;
	if (bRenderTEXT1)
	{
		_debug_aspect_ratio = _A2VIDEO_LEGACY_ASPECT_RATIO;
		ImGui::SetNextWindowSizeConstraints(
			ImVec2(_A2VIDEO_LEGACY_WIDTH/2, _A2VIDEO_LEGACY_HEIGHT/2),
			ImVec2(FLT_MAX, FLT_MAX),
			AspectConstraintCallback, (void*)&_debug_aspect_ratio);
		ImGui::Begin("TEXT1 Viewer", &bRenderTEXT1);
		ImGui::Image(reinterpret_cast<void*>(debug_texture_id[0]), ImGui::GetContentRegionAvail(), ImVec2(0, 0), ImVec2(1, 1));
		ImGui::End();
	}
	if (bRenderTEXT2)
	{
		_debug_aspect_ratio = _A2VIDEO_LEGACY_ASPECT_RATIO;
		ImGui::SetNextWindowSizeConstraints(
			ImVec2(_A2VIDEO_LEGACY_WIDTH / 2, _A2VIDEO_LEGACY_HEIGHT / 2), 
			ImVec2(FLT_MAX, FLT_MAX),
			AspectConstraintCallback, (void*)&_debug_aspect_ratio);
		ImGui::Begin("TEXT2 Viewer", &bRenderTEXT2);
		ImGui::Image(reinterpret_cast<void*>(debug_texture_id[1]), ImGui::GetContentRegionAvail(), ImVec2(0, 0), ImVec2(1, 1));
		ImGui::End();
	}
	if (bRenderHGR1)
	{
		_debug_aspect_ratio = _A2VIDEO_LEGACY_ASPECT_RATIO;
		ImGui::SetNextWindowSizeConstraints(
			ImVec2(_A2VIDEO_LEGACY_WIDTH / 2, _A2VIDEO_LEGACY_HEIGHT / 2), 
			ImVec2(FLT_MAX, FLT_MAX),
			AspectConstraintCallback, (void*)&_debug_aspect_ratio);
		ImGui::Begin("HGR1 Viewer", &bRenderHGR1);
		ImGui::Image(reinterpret_cast<void*>(debug_texture_id[2]), ImGui::GetContentRegionAvail(), ImVec2(0, 0), ImVec2(1, 1));
		ImGui::End();
	}
	if (bRenderHGR2)
	{
		_debug_aspect_ratio = _A2VIDEO_LEGACY_ASPECT_RATIO;
		ImGui::SetNextWindowSizeConstraints(
			ImVec2(_A2VIDEO_LEGACY_WIDTH / 2, _A2VIDEO_LEGACY_HEIGHT / 2), 
			ImVec2(FLT_MAX, FLT_MAX),
			AspectConstraintCallback, (void*)&_debug_aspect_ratio);
		ImGui::Begin("HGR2 Viewer", &bRenderHGR2);
		ImGui::Image(reinterpret_cast<void*>(debug_texture_id[3]), ImGui::GetContentRegionAvail(), ImVec2(0, 0), ImVec2(1, 1));
		ImGui::End();
	}
}

void A2VideoManager::DisplayImGuiWindow(bool* p_open)
{
	bImguiWindowIsOpen = p_open;
	if (p_open)
	{
		ImGui::SetNextWindowSizeConstraints(ImVec2(420, 400), ImVec2(FLT_MAX, FLT_MAX));
		ImGui::Begin("Apple 2 Video Manager", p_open);
		if (!ImGui::IsWindowCollapsed())
		{
			auto memManager = MemoryManager::GetInstance();
			ImGui::PushItemWidth(200);
			
			if (ImGui::Button("Run Vertical Refresh"))
				this->ForceBeamFullScreenRender();
			ImGui::SameLine();
			ImGui::Text("Frame ID: %d", this->GetVRAMReadId());
			
			ImGui::SeparatorText("[ BORDERS AND WIDTH ]");
			ImGui::SliderInt("Horizontal Borders", &border_w_slider_val, 0, _BORDER_WIDTH_MAX_CYCLES, "%d", 1);
			ImGui::SliderInt("Vertical Borders", &border_h_slider_val, 0, _BORDER_HEIGHT_MAX_MULT8, "%d", 1);
			if (ImGui::SliderInt("Border Color (0xC034)", &memManager->switch_c034, 0, 15))
				this->ForceBeamFullScreenRender();
			if (ImGui::Checkbox("Align Legacy to SHR Vertically", &this->bAlignQuadsToScanline))
				this->ForceBeamFullScreenRender();
			ImGui::SetItemTooltip("Aligns all vertical origins so that the scanlines match exactly");
			if (ImGui::Checkbox("Force SHR width in merged mode", &this->bForceSHRWidth))
				this->ForceBeamFullScreenRender();
			ImGui::SetItemTooltip("Legacy image is stretched horizontally to match the SHR image");
			if (ImGui::Checkbox("No wobble in merged mode", &this->bNoMergedModeWobble))
				this->ForceBeamFullScreenRender();
			ImGui::SetItemTooltip("Removes the wobble effect when shifting mode mid-frame");
			if (ImGui::Checkbox("Demo Merged Mode", &this->bDEMOMergedMode))
				this->ForceBeamFullScreenRender();
			ImGui::SetItemTooltip("A demo feature to force mid-frame switch between SHR and Legacy");
			if (ImGui::SliderFloat("Wobble Power", &bWobblePower, 0.0, 0.3))
				this->ForceBeamFullScreenRender();
			ImGui::SetItemTooltip("Wobble strength. A value of .2 is relatively accurate");

			//eA2MonitorType
			const char* monitorTypes[] = { "Color", "White", "Green", "Amber" };
			if (ImGui::Combo("Monitor Type", &this->eA2MonitorType, monitorTypes, IM_ARRAYSIZE(monitorTypes)))
			{
				for (auto i=0; i<A2VIDEOBEAM_TOTAL_COUNT; ++i) {
					windowsbeam[i]->monitorColorType = eA2MonitorType;
				}
				this->ForceBeamFullScreenRender();
			}

			// NTSC
			if (eA2MonitorType == A2_MON_COLOR)
			{
				if (ImGui::Checkbox("NTSC", &p_b_ntsc))
					this->ForceBeamFullScreenRender();
				if (p_b_ntsc)
				{
					ImGui::SameLine();
					if (ImGui::Checkbox("Clean Text", &p_b_ntscNoFilterMono))
						this->ForceBeamFullScreenRender();
					if (ImGui::SliderFloat("NTSC Strength", &p_f_ntscStrength, 0.05f, 1.f, "%.2f"))
						this->ForceBeamFullScreenRender();
					if (ImGui::SliderFloat("NTSC Comb Strength", &p_f_ntscCombStrength, 0.f, 1.f, "%.2f"))
						this->ForceBeamFullScreenRender();
					ImGui::SetItemTooltip("0.8 for model 1, 0.9 for model 2");
					if (ImGui::SliderFloat("NTSC Gamma Correction", &p_f_ntscGammaCorrection, 0.1f, 3.f, "%.1f"))
						this->ForceBeamFullScreenRender();
					ImGui::SetItemTooltip("Adjust to taste, 1.0 is neutral");
				}
			}

			ImGui::SeparatorText("[ VIDHD TEXT MODES ]");
			// VidHdMode_e
			const char* vidHdModes[] = { "OFF", "40x24", "80x24", "80x45", "120x67", "240x135" };
			overrideVidHDTextMode = (int)vidhdWindowBeam->GetVideoMode();
			if (ImGui::Combo("VidHD Text Modes", &this->overrideVidHDTextMode, vidHdModes, IM_ARRAYSIZE(vidHdModes)))
			{
				vidhdWindowBeam->SetVideoMode((VidHdMode_e)overrideVidHDTextMode);
				this->ForceBeamFullScreenRender();
			}
			c022TextColorForeNibble = memManager->switch_c022 >> 4;
			c022TextColorBackNibble = memManager->switch_c022 & 0xF;
			if (ImGui::SliderInt("Text Color FG (0xC022)", &c022TextColorForeNibble, 0, 15))
			{
				auto _color = (c022TextColorForeNibble << 4) | c022TextColorBackNibble;
				memManager->switch_c022 = _color;
				this->ForceBeamFullScreenRender();
			}
			if (ImGui::SliderInt("Text Color BG (0xC022)", &c022TextColorBackNibble, 0, 15))
			{
				auto _color = (c022TextColorForeNibble << 4) | c022TextColorBackNibble;
				memManager->switch_c022 = _color;
				this->ForceBeamFullScreenRender();
			}
			vidHdTextAlphaForeNibble = vidhdWindowBeam->GetAlpha() >> 4;
			vidHdTextAlphaBackNibble = vidhdWindowBeam->GetAlpha() & 0xF;
			if (ImGui::SliderInt("Text Alpha FG", &vidHdTextAlphaForeNibble, 0, 15))
			{
				auto _alpha = (vidHdTextAlphaForeNibble << 4) | vidHdTextAlphaBackNibble;
				vidhdWindowBeam->SetAlpha(_alpha);
				this->ForceBeamFullScreenRender();
			}
			if (ImGui::SliderInt("Text Alpha BG", &vidHdTextAlphaBackNibble, 0, 15))
			{
				auto _alpha = (vidHdTextAlphaForeNibble << 4) | vidHdTextAlphaBackNibble;
				vidhdWindowBeam->SetAlpha(_alpha);
				this->ForceBeamFullScreenRender();
			}

			ImGui::Columns(2, "Legacy_Columns", false);
			ImGui::SeparatorText("[ LEGACY EXTRA MODES ]");
			if (ImGui::Checkbox("HGR SPEC1", &bUseHGRSPEC1))
				this->ForceBeamFullScreenRender();
			ImGui::SetItemTooltip("A HGR mode that makes 11011 be black, found in the EVE RGB card");
			if (ImGui::Checkbox("HGR SPEC2", &bUseHGRSPEC2))
				this->ForceBeamFullScreenRender();
			ImGui::SetItemTooltip("A HGR mode that makes 00100 be white, found in the EVE RGB card");
			if (ImGui::Checkbox("DHGR COL140 Mixed", &bUseDHGRCOL140Mixed))
				this->ForceBeamFullScreenRender();
			ImGui::SetItemTooltip("A DHGR mode that mixes 16 colors and b/w, found in certain RGB cards");
			
			ImGui::NextColumn();
			ImGui::SeparatorText("[ LEGACY PAGING ]");
			if (ImGui::RadioButton("Normal##Legacyoverride", overrideLegacyPaging == DOUBLE_NONE))
				overrideLegacyPaging = DOUBLE_NONE;
			if (ImGui::RadioButton("Interlace##Legacyoverride", overrideLegacyPaging == DOUBLE_INTERLACE))
				overrideLegacyPaging = DOUBLE_INTERLACE;
			if (ImGui::RadioButton("Page Flip##Legacyoverride", overrideLegacyPaging == DOUBLE_PAGEFLIP))
				overrideLegacyPaging = DOUBLE_PAGEFLIP;
			if (windowsbeam[A2VIDEOBEAM_LEGACY]->pagingMode != overrideLegacyPaging)
			{
				windowsbeam[A2VIDEOBEAM_LEGACY]->pagingMode = overrideLegacyPaging;
				this->ForceBeamFullScreenRender();
			}
			ImGui::Columns(1);

			ImGui::Columns(2, "SHR_Columns", false);
			ImGui::SeparatorText("[ SHR EXTRA MODES ]");
			bool isNoneActive = (vrams_read->frameSHRModes == A2_VSM_NONE);
			bool isSHR4SHRActive = (vrams_read->frameSHRModes & A2_VSM_SHR4SHR) != 0;
			bool isSHR4RGGBActive = (vrams_read->frameSHRModes & A2_VSM_SHR4RGGB) >  A2_VSM_SHR4SHR;
			bool isSHR4PAL256Active = (vrams_read->frameSHRModes & A2_VSM_SHR4PAL256)  >  A2_VSM_SHR4SHR;
			bool isSHR4R4G4B4Active = (vrams_read->frameSHRModes & A2_VSM_SHR4R4G4B4)  >  A2_VSM_SHR4SHR;
			bool isSHR3200Active = (vrams_read->frameSHRModes & A2_VSM_3200SHR) != 0;
			bool isSHRInterlaced = (vrams_read->pagedMode == DOUBLE_INTERLACE);
			bool isSHRPageFlip = (vrams_read->pagedMode == DOUBLE_PAGEFLIP);

			ImGui::BeginDisabled();
			ImGui::Checkbox("None##SHR4", &isNoneActive);
			ImGui::Checkbox("SHR4 SHR", &isSHR4SHRActive);
			ImGui::Checkbox("SHR4 RGGB", &isSHR4RGGBActive);
			ImGui::Checkbox("SHR4 PAL256", &isSHR4PAL256Active);
			ImGui::Checkbox("SHR4 R4G4B4", &isSHR4R4G4B4Active);
			ImGui::Checkbox("SHR 3200", &isSHR3200Active);
			ImGui::Checkbox("SHR Interlacing", &isSHRInterlaced);
			ImGui::Checkbox("SHR Page Flip", &isSHRPageFlip);
			ImGui::EndDisabled();
			
			ImGui::NextColumn();
			ImGui::SeparatorText("[ OVERRIDE ]");
			auto _prevOverride = overrideSHRMode;
			if (ImGui::RadioButton("None##SHR4override", overrideSHRMode < A2_VSM_SHR4SHR))
				overrideSHRMode = A2_VSM_NONE;
			if (ImGui::RadioButton("Force SHR", overrideSHRMode == A2_VSM_SHR4SHR))
				overrideSHRMode = A2_VSM_SHR4SHR;
			if (ImGui::RadioButton("Force RGGB", overrideSHRMode == A2_VSM_SHR4RGGB))
				overrideSHRMode = A2_VSM_SHR4RGGB;
			if (ImGui::RadioButton("Force PAL256", overrideSHRMode == A2_VSM_SHR4PAL256))
				overrideSHRMode = A2_VSM_SHR4PAL256;
			if (ImGui::RadioButton("Force R4G4B4", overrideSHRMode == A2_VSM_SHR4R4G4B4))
				overrideSHRMode = A2_VSM_SHR4R4G4B4;
			if (ImGui::RadioButton("Force 3200", overrideSHRMode == A2_VSM_3200SHR)) {
				overrideSHRMode = A2_VSM_3200SHR;
			}
			if (overrideSHRMode != _prevOverride)
				this->ForceBeamFullScreenRender();
			const char* _doubleSHRModes[] = { "Default", "Disable", "Interlace", "Page Flip" };
			ImGui::PushItemWidth(100);
			if (ImGui::Combo("Force Double SHR", &this->overrideDoubleSHR, _doubleSHRModes, IM_ARRAYSIZE(_doubleSHRModes)))
			{
				this->ForceBeamFullScreenRender();
			}
			ImGui::PopItemWidth();
			ImGui::Columns(1);

			//vidhdWindowBeam->DisplayImGuiWindow(p_open);
		}
		ImGui::End();
	}
}

void A2VideoManager::DisplayImGUIRGBDebugWindows()
{
	if (!v_debug_rgb_windows.empty())
	{
		// delete closed debug windows
		for (auto it = v_debug_rgb_windows.begin(); it != v_debug_rgb_windows.end(); ) {
			if (!it->bImguiWindowIsOpen) {
				// erase returns the next valid iterator
				it = v_debug_rgb_windows.erase(it);
			} else {
				++it;
			}
		}

		// Display all debug RGB windows
		for (auto& debugWin : v_debug_rgb_windows) {
			debugWin.DisplayImGuiWindow();
		}
		GLuint glerr;
		if ((glerr = glGetError()) != GL_NO_ERROR) {
			std::cerr << "A2VideoManager DisplayImGUIRGBDebugWindows error: " << glerr << std::endl;
		}
	}

}

void A2VideoManager::CreateNewA2WindowRGB()
{
	if (v_debug_rgb_windows.size() < (_MAX_DEBUG_RGB_WINDOWS - 1))
	{
		v_debug_rgb_windows.emplace_back();
	}
}

nlohmann::json A2VideoManager::SerializeState()
{
	nlohmann::json jsonState = {
		{"borders_w_cycles", border_w_slider_val},
		{"borders_h_8scanlines", border_h_slider_val},
		{"borders_color", MemoryManager::GetInstance()->switch_c034},
		{"monitor_type", eA2MonitorType},
		{"enable_DHGRCOL140Mixed", bUseDHGRCOL140Mixed},
		{"enable_HGRSPEC1", bUseHGRSPEC1},
		{"enable_HGRSPEC2", bUseHGRSPEC2},
		{"align_quads_to_scanline", bAlignQuadsToScanline},
		{"force_shr_width_in_merge_mode", bForceSHRWidth},
		{"no_merged_mode_wobble", bNoMergedModeWobble},
		{"font_rom_regular_index", font_rom_regular_idx},
		{"font_rom_alternate_index", font_rom_alternate_idx},
		{"p_b_ntsc", p_b_ntsc},
		{"p_b_ntscNoFilterMono", p_b_ntscNoFilterMono},
		{"p_f_ntscStrength", p_f_ntscStrength},
		{"p_f_ntscCombStrength", p_f_ntscCombStrength},
		{"p_f_ntscGammaCorrection", p_f_ntscGammaCorrection},
	};
	return jsonState;
}

void A2VideoManager::DeserializeState(const nlohmann::json &jsonState)
{
	MemoryManager::GetInstance()->switch_c034 = jsonState.value("borders_color", MemoryManager::GetInstance()->switch_c034);
	eA2MonitorType = jsonState.value("monitor_type", eA2MonitorType);
	bUseDHGRCOL140Mixed = jsonState.value("enable_DHGRCOL140Mixed", bUseDHGRCOL140Mixed);
	bUseHGRSPEC1 = jsonState.value("enable_HGRSPEC1", bUseHGRSPEC1);
	bUseHGRSPEC2 = jsonState.value("enable_HGRSPEC2", bUseHGRSPEC2);
	bAlignQuadsToScanline = jsonState.value("force_shr_width_in_merge_mode", bAlignQuadsToScanline);
	bForceSHRWidth = jsonState.value("align_quads_to_scanline", bForceSHRWidth);
	bNoMergedModeWobble = jsonState.value("no_merged_mode_wobble", bNoMergedModeWobble);
	font_rom_regular_idx = jsonState.value("font_rom_regular_index", font_rom_regular_idx);
	font_rom_alternate_idx = jsonState.value("font_rom_alternate_index", font_rom_alternate_idx);
	p_b_ntsc = jsonState.value("p_b_ntsc", p_b_ntsc);
	p_b_ntscNoFilterMono = jsonState.value("p_b_ntscNoFilterMono", p_b_ntscNoFilterMono);
	p_f_ntscStrength = jsonState.value("p_f_ntscStrength", p_f_ntscStrength);
	p_f_ntscCombStrength = jsonState.value("p_f_ntscCombStrength", p_f_ntscCombStrength);
	p_f_ntscGammaCorrection = jsonState.value("p_f_ntscGammaCorrection", p_f_ntscGammaCorrection);

	border_w_slider_val = jsonState.value("borders_w_cycles", border_w_slider_val);
	border_h_slider_val = jsonState.value("borders_h_8scanlines", border_h_slider_val);
}
