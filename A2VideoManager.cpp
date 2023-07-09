#include "A2VideoManager.h"
#include <cstring>
#include <zlib.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <map>
#include "SDL.h"
#ifdef _DEBUGTIMINGS
#include <chrono>
#endif

#include "OpenGLHelper.h"
#include "SDHRManager.h"
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
	// TODO: Later do TV fringe colors
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

static uint32_t gRGBTransparent = 0;

// below because "The declaration of a static data member in its class definition is not a definition"
A2VideoManager* A2VideoManager::s_instance;
uint16_t A2VideoManager::a2SoftSwitches = 0;

static OpenGLHelper* oglHelper = OpenGLHelper::GetInstance();

static Shader shader_a2video_text = Shader();
static Shader shader_a2video_dtext = Shader();

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
	v_fblgr1 = std::vector<uint32_t>(_A2VIDEO_MIN_WIDTH * _A2VIDEO_MIN_HEIGHT, 0);
	v_fblgr2 = std::vector<uint32_t>(_A2VIDEO_MIN_WIDTH * _A2VIDEO_MIN_HEIGHT, 0);
	v_fbdlgr = std::vector<uint32_t>(_A2VIDEO_MIN_WIDTH * _A2VIDEO_MIN_HEIGHT * 2, 0);
	v_fbhgr1 = std::vector<uint32_t>(_A2VIDEO_MIN_WIDTH * _A2VIDEO_MIN_HEIGHT, 0);
	v_fbhgr2 = std::vector<uint32_t>(_A2VIDEO_MIN_WIDTH * _A2VIDEO_MIN_HEIGHT, 0);
	v_fbdhgr = std::vector<uint32_t>(_A2VIDEO_MIN_WIDTH * _A2VIDEO_MIN_HEIGHT * 2, 0);

	a2SoftSwitches = A2SS_TEXT; // default to TEXT1

	// Set up the image assets (textures)
	// There's no need for tileset records since we know exactly
	// what the image assets look like, and the shaders will use them directly
	*image_assets = {};
	for (size_t i = 0; i < (sizeof(image_assets) / sizeof(ImageAsset)); i++)
	{
		image_assets[i].tex_id = oglHelper->get_texture_id_at_slot(i);
	}

	// Generate shaders
	shader_a2video_text.build(_SHADER_TEXT_VERTEX_DEFAULT, _SHADER_TEXT_FRAGMENT_DEFAULT);
	shader_a2video_dtext.build(_SHADER_TEXT_VERTEX_DEFAULT, _SHADER_DTEXT_FRAGMENT);

	// Initialize windows and meshes
	
	// TEXT1
	windows[A2VIDEO_TEXT1].Define(
		A2VIDEO_TEXT1,
		uXY({ (uint32_t)(_A2VIDEO_MIN_WIDTH) , (uint32_t)(_A2VIDEO_MIN_HEIGHT) }),
		uXY({ _A2_TEXT40_CHAR_WIDTH, _A2_TEXT40_CHAR_HEIGHT }),
		uXY({ 40, 24 }),
		SDHRManager::GetInstance()->GetApple2MemPtr() + _A2VIDEO_TEXT1_START,
		_A2VIDEO_TEXT_SIZE,
		&shader_a2video_text
	);
	// TEXT2
	windows[A2VIDEO_TEXT2].Define(
		A2VIDEO_TEXT2,
		uXY({ (uint32_t)(_A2VIDEO_MIN_WIDTH) , (uint32_t)(_A2VIDEO_MIN_HEIGHT) }),
		uXY({ _A2_TEXT40_CHAR_WIDTH, _A2_TEXT40_CHAR_HEIGHT }),
		uXY({ 40, 24 }),
		SDHRManager::GetInstance()->GetApple2MemPtr() + _A2VIDEO_TEXT2_START,
		_A2VIDEO_TEXT_SIZE,
		&shader_a2video_text
	);
	// DTEXT
	windows[A2VIDEO_DTEXT].Define(
		A2VIDEO_DTEXT,
		uXY({ (uint32_t)(_A2VIDEO_MIN_WIDTH) , (uint32_t)(_A2VIDEO_MIN_HEIGHT) }),
		uXY({ _A2_TEXT80_CHAR_WIDTH, _A2_TEXT80_CHAR_HEIGHT }),
		uXY({ 80, 24 }),
		SDHRManager::GetInstance()->GetApple2MemPtr() + _A2VIDEO_TEXT1_START,
		_A2VIDEO_TEXT_SIZE + _SDHR_MEMORY_SHADOW_END,
		&shader_a2video_dtext
	);
	// LGR1
	windows[A2VIDEO_LGR1].Define(
		A2VIDEO_LGR1,
		uXY({ (uint32_t)(_A2VIDEO_MIN_WIDTH), (uint32_t)(_A2VIDEO_MIN_HEIGHT) }),
		uXY({ 1, 1 }),
		uXY({ _A2VIDEO_MIN_WIDTH, _A2VIDEO_MIN_HEIGHT }),		// 192 lines
		(uint8_t*)(&v_fblgr1[0]),
		v_fblgr1.size() * sizeof(v_fblgr1[0]),
		nullptr		// do not render in the window. Rendering is done here
	);
	// LGR2
	windows[A2VIDEO_LGR2].Define(
		A2VIDEO_LGR2,
		uXY({ (uint32_t)(_A2VIDEO_MIN_WIDTH), (uint32_t)(_A2VIDEO_MIN_HEIGHT) }),
		uXY({ 1, 1 }),
		uXY({ _A2VIDEO_MIN_WIDTH, _A2VIDEO_MIN_HEIGHT }),		// 192 lines
		(uint8_t*)(&v_fblgr2[0]),
		v_fblgr2.size() * sizeof(v_fblgr2[0]),
		nullptr		// do not render in the window. Rendering is done here
	);
	// DLGR
	windows[A2VIDEO_DLGR].Define(
		A2VIDEO_DLGR,
		uXY({ (uint32_t)(_A2VIDEO_MIN_WIDTH), (uint32_t)(_A2VIDEO_MIN_HEIGHT) }),
		uXY({ 1, 1 }),
		uXY({ _A2VIDEO_MIN_WIDTH, _A2VIDEO_MIN_HEIGHT }),		// 192 lines
		(uint8_t*)(&v_fbdlgr[0]),
		v_fbdlgr.size() * sizeof(v_fbdlgr[0]),
		nullptr		// do not render in the window. Rendering is done here
	);
	// HGR1
	windows[A2VIDEO_HGR1].Define(
		A2VIDEO_HGR1,
		uXY({ (uint32_t)(_A2VIDEO_MIN_WIDTH), (uint32_t)(_A2VIDEO_MIN_HEIGHT) }),
		uXY({ 1, 1 }),
		uXY({ _A2VIDEO_MIN_WIDTH, _A2VIDEO_MIN_HEIGHT }),		// 192 lines
		(uint8_t*)(&v_fbhgr1[0]),
		v_fbhgr1.size() * sizeof(v_fbhgr1[0]),
		nullptr		// do not render in the window. Rendering is done here
	);
	// HGR2
	windows[A2VIDEO_HGR2].Define(
		A2VIDEO_HGR2,
		uXY({ (uint32_t)(_A2VIDEO_MIN_WIDTH), (uint32_t)(_A2VIDEO_MIN_HEIGHT) }),
		uXY({ 1, 1 }),
		uXY({ _A2VIDEO_MIN_WIDTH, _A2VIDEO_MIN_HEIGHT }),		// 192 lines
		(uint8_t*)(&v_fbhgr2[1]),
		v_fbhgr2.size() * sizeof(v_fbhgr2[0]),
		nullptr		// do not render in the window. Rendering is done here
	);
	// DHGR
	windows[A2VIDEO_DHGR].Define(
		A2VIDEO_DHGR,
		uXY({ (uint32_t)(_A2VIDEO_MIN_WIDTH), (uint32_t)(_A2VIDEO_MIN_HEIGHT) }),
		uXY({ 1, 1 }),
		uXY({ _A2VIDEO_MIN_WIDTH, _A2VIDEO_MIN_HEIGHT }),		// 192 lines
		(uint8_t*)(&v_fbdhgr[1]),
		v_fbdhgr.size() * sizeof(v_fbdhgr[0]),
		nullptr		// do not render in the window. Rendering is done here
	);

	// Activate TEXT1 by default
	SelectVideoModes();
	// tell the next Render() call to run initialization routines
	bShouldInitializeRender = true;
}

A2VideoManager::~A2VideoManager()
{

}

void A2VideoManager::NotifyA2MemoryDidChange(uint16_t addr)
{
	// Note: We could do delta updates here for the video modes
	// but for better reliability we do full updates of the video modes
	// every frame in the render method
	if (IsSoftSwitch(A2SS_RAMWRT))
	{
		if (addr >= _A2VIDEO_TEXT1_START && addr < (_A2VIDEO_TEXT1_START + _A2VIDEO_TEXT_SIZE))
			windows[A2VIDEO_DTEXT].bNeedsGPUDataUpdate = true;
	}
	else {
		if (addr >= _A2VIDEO_TEXT1_START && addr < (_A2VIDEO_TEXT1_START + _A2VIDEO_TEXT_SIZE))
			if (IsSoftSwitch(A2SS_80STORE) && IsSoftSwitch(A2SS_PAGE2))	// writing to aux text and video memory
				windows[A2VIDEO_DTEXT].bNeedsGPUDataUpdate = true;
			else
				windows[A2VIDEO_TEXT1].bNeedsGPUDataUpdate = true;
		else if (addr >= _A2VIDEO_TEXT2_START && addr < (_A2VIDEO_TEXT2_START + _A2VIDEO_TEXT_SIZE))
			windows[A2VIDEO_TEXT2].bNeedsGPUDataUpdate = true;
	}
}

void A2VideoManager::ToggleA2Video(bool value)
{
	bA2VideoEnabled = value;
	if (bA2VideoEnabled)
	{
		auto scrSz = ScreenSize();
		oglHelper->request_framebuffer_resize(scrSz.x, scrSz.y);
		bShouldInitializeRender = true;
		SelectVideoModes();
	}
}

void A2VideoManager::ProcessSoftSwitch(uint16_t addr)
{
	switch (addr)
	{
	case 0xC000:	// 80STOREOFF
		a2SoftSwitches &= ~A2SS_80STORE;
		break;
	case 0xC001:	// 80STOREON
		a2SoftSwitches |= A2SS_80STORE;
		break;
	case 0xC002:	// RAMRDOFF
		a2SoftSwitches &= ~A2SS_RAMRD;
		break;
	case 0xC003:	// RAMRDON
		a2SoftSwitches |= A2SS_RAMRD;
		break;
	case 0xC004:	// RAMWRTOFF
		a2SoftSwitches &= ~A2SS_RAMWRT;
		break;
	case 0xC005:	// RAMWRTON
		a2SoftSwitches |= A2SS_RAMWRT;
		break;
	case 0xC006:	// INTCXROMOFF
		a2SoftSwitches &= ~A2SS_INTCXROM;
		break;
	case 0xC007:	// INTCXROMON
		a2SoftSwitches |= A2SS_INTCXROM;
		break;
	case 0xC00A:	// SLOTC3ROMOFF
		a2SoftSwitches &= ~A2SS_SLOTC3ROM;
		break;
	case 0xC00B:	// SLOTC3ROMOFF
		a2SoftSwitches |= A2SS_SLOTC3ROM;
		break;
	case 0xC00C:	// 80COLOFF
		a2SoftSwitches &= ~A2SS_80COL;
		break;
	case 0xC00D:	// 80COLON
		a2SoftSwitches |= A2SS_80COL;
		break;
	case 0xC00E:	// ALTCHARSETOFF
		a2SoftSwitches &= ~A2SS_ALTCHARSET;
		break;
	case 0xC00F:	// ALTCHARSETON
		a2SoftSwitches |= A2SS_ALTCHARSET;
		break;
	case 0xC050:	// TEXTOFF
		a2SoftSwitches &= ~A2SS_TEXT;
		break;
	case 0xC051:	// TEXTON
		a2SoftSwitches |= A2SS_TEXT;
		break;
	case 0xC052:	// MIXEDOFF
		a2SoftSwitches &= ~A2SS_MIXED;
		break;
	case 0xC053:	// MIXEDON
		a2SoftSwitches |= A2SS_MIXED;
		break;
	case 0xC054:	// PAGE2OFF
		a2SoftSwitches &= ~A2SS_PAGE2;
		break;
	case 0xC055:	// PAGE2ON
		a2SoftSwitches |= A2SS_PAGE2;
		break;
	case 0xC056:	// HIRESOFF
		a2SoftSwitches &= ~A2SS_HIRES;
		break;
	case 0xC057:	// HIRESON
		a2SoftSwitches |= A2SS_HIRES;
		break;
	case 0xC05E:	// DRESON
		a2SoftSwitches |= A2SS_DRES;
		break;
	case 0xC05F:	// DRESOFF
		a2SoftSwitches &= ~A2SS_DRES;
		break;
	default:
		break;
	}
	SelectVideoModes();
}

void A2VideoManager::SelectVideoModes()
{
	for (auto& _w : this->windows) {
		_w.SetEnabled(false);
	}
	if (!IsSoftSwitch(A2SS_TEXT))
	{
		if (IsSoftSwitch(A2SS_80COL))	// double resolution
		{
			if (IsSoftSwitch(A2SS_DRES))
				this->windows[A2VIDEO_DHGR].SetEnabled(true);
			else
				this->windows[A2VIDEO_DLGR].SetEnabled(true);
		}
		else if (IsSoftSwitch(A2SS_HIRES))	// standard hires
		{
			if (IsSoftSwitch(A2SS_PAGE2))
				this->windows[A2VIDEO_HGR2].SetEnabled(true);
			else
				this->windows[A2VIDEO_HGR1].SetEnabled(true);
		}
		else {	// standard lores
			if (IsSoftSwitch(A2SS_PAGE2))
				this->windows[A2VIDEO_LGR2].SetEnabled(true);
			else
				this->windows[A2VIDEO_LGR1].SetEnabled(true);
		}
	}
	// Now check the text modes
	if (IsSoftSwitch(A2SS_TEXT) || IsSoftSwitch(A2SS_MIXED))
	{
		if (IsSoftSwitch(A2SS_80COL))
			this->windows[A2VIDEO_DTEXT].SetEnabled(true);
		else
		{
			if (IsSoftSwitch(A2SS_PAGE2) && !IsSoftSwitch(A2SS_80STORE))
				this->windows[A2VIDEO_TEXT2].SetEnabled(true);
			else
				this->windows[A2VIDEO_TEXT1].SetEnabled(true);
		}
	}
	return;
}

void A2VideoManager::Render()
{
	if (!bA2VideoEnabled)
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
		image_assets[0].AssignByFilename(this, "textures/Apple2eFont14x16 - Regular.png");
		// image asset 1: The alternate font
		glActiveTexture(_SDHR_START_TEXTURES + 1);
		image_assets[1].AssignByFilename(this, "textures/Apple2eFont14x16 - Alternate.png");
		// image asset 0: The apple 2e US font 80COL
		glActiveTexture(_SDHR_START_TEXTURES + 2);
		image_assets[2].AssignByFilename(this, "textures/Apple2eFont7x16 - Regular.png");
		// image asset 1: The alternate font 80COL
		glActiveTexture(_SDHR_START_TEXTURES + 3);
		image_assets[3].AssignByFilename(this, "textures/Apple2eFont7x16 - Alternate.png");
		// image asset 4: The Scanline texture
		glActiveTexture(_SDHR_START_TEXTURES + 4);
		image_assets[4].AssignByFilename(this, "textures/Texture_Scanlines.png");
		if ((glerr = glGetError()) != GL_NO_ERROR) {
			std::cerr << "OpenGL AssignByFilename error: " 
				<< 0 << " - " << glerr << std::endl;
		}
		glActiveTexture(GL_TEXTURE0);
	}

	// Update Apple 2 video windows
	for (auto& _w : this->windows) {
		_w.Update();
	}

	for (auto& _w : this->windows) {
		_w.Render();
	}

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, oglHelper->get_output_texture_id());

	if (this->windows[A2VIDEO_LGR1].IsEnabled())
	{
		for (size_t i = 0; i < _A2VIDEO_TEXT_SIZE; i++)
		{
			this->UpdateLoResRGBCell(_A2VIDEO_TEXT1_START + i, _A2VIDEO_TEXT1_START, &v_fblgr1);
		}
		this->RenderSubMixed(&v_fblgr1);
	}
	if (this->windows[A2VIDEO_LGR2].IsEnabled())
	{
		for (size_t i = 0; i < _A2VIDEO_TEXT_SIZE; i++)
		{
			this->UpdateLoResRGBCell(_A2VIDEO_TEXT2_START + i, _A2VIDEO_TEXT2_START, &v_fblgr2);
		}
		this->RenderSubMixed(&v_fblgr2);
	}
	if (this->windows[A2VIDEO_DLGR].IsEnabled())
	{
		for (size_t i = 0; i < _A2VIDEO_TEXT_SIZE; i++)
		{
			this->UpdateDLoResRGBCell(_A2VIDEO_TEXT1_START + i, _A2VIDEO_TEXT1_START, &v_fbdlgr);
		}
		this->RenderSubMixed(&v_fbdlgr);
	}
	if (this->windows[A2VIDEO_HGR1].IsEnabled())
	{
		for (size_t i = 0; i < _A2VIDEO_HGR_SIZE; i++)
		{
			this->UpdateHiResRGBCell(_A2VIDEO_HGR1_START + i, _A2VIDEO_HGR1_START, &v_fbhgr1);
		}
		this->RenderSubMixed(&v_fbhgr1);
	}
	if (this->windows[A2VIDEO_HGR2].IsEnabled())
	{
		for (size_t i = 0; i < _A2VIDEO_HGR_SIZE; i++)
		{
			this->UpdateHiResRGBCell(_A2VIDEO_HGR2_START + i, _A2VIDEO_HGR2_START, &v_fbhgr2);
		}
		this->RenderSubMixed(&v_fbhgr2);
	}
	if (this->windows[A2VIDEO_DHGR].IsEnabled())
	{
		for (size_t i = 0; i < _A2VIDEO_HGR_SIZE; i++)
		{
			this->UpdateDHiResRGBCell(_A2VIDEO_HGR1_START + i, _A2VIDEO_HGR1_START, &v_fbdhgr);
		}
		this->RenderSubMixed(&v_fbdhgr);
	}

	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL draw error: " << glerr << std::endl;
	}
	oglh->cleanup_render();
}

void A2VideoManager::RenderSubMixed(std::vector<uint32_t>* framebuffer)
{
	if (IsSoftSwitch(A2SS_MIXED))
		glTexSubImage2D(GL_TEXTURE_2D, 0,
			0, 0, _A2VIDEO_MIN_WIDTH, _A2VIDEO_MIN_MIXED_HEIGHT,
			GL_RGBA, GL_UNSIGNED_BYTE, (void*)(&framebuffer->at(0)));
	else
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
			_A2VIDEO_MIN_WIDTH, _A2VIDEO_MIN_HEIGHT,
			0, GL_RGBA, GL_UNSIGNED_BYTE, (void*)(&framebuffer->at(0)));
}

//////////////////////////////////////////////////////////////////////////
// GRAPHICS METHODS
// RGB Videocard code from AppleWin
//////////////////////////////////////////////////////////////////////////

void A2VideoManager::UpdateLoResRGBCell(uint16_t addr, const uint16_t addr_start, std::vector<uint32_t>* framebuffer)
{
	int x = LGR_ADDR2X[addr - addr_start];	// x start in pixels
	int y = LGR_ADDR2Y[addr - addr_start];	// y in pixels
	if (x < 0 || y < 0)	// the holes!
		return;

	// Everything is double the resolution
	x *= 2;
	y *= 2;
	uint8_t val = *(SDHRManager::GetInstance()->GetApple2MemPtr() + addr);

	uint8_t colorIdx;
	// Set all 14 dots in the top 4 rows for the low 4 bits color
	// and the bottom 4 rows for the high bits color
	// Duplicate each row for the double resolution rows
	for (size_t j = 0; j < 8; j++)
	{
		colorIdx = (j < 4) ? (val & 0xF) : (val & 0xF0) >> 4;
		for (size_t i = 0; i < 14; i++)
		{
			framebuffer->at(y * _A2VIDEO_MIN_WIDTH + x + i) = gPaletteRGB[colorIdx + 12];	// LoRes colors start at index 12
			framebuffer->at((y + 1) * _A2VIDEO_MIN_WIDTH + x + i) = gPaletteRGB[colorIdx + 12];
		}
		y += 2;
	}
}

#define ROL_NIB(x) ( (((x)<<1)&0xF) | (((x)>>3)&1) )

void A2VideoManager::UpdateDLoResRGBCell(uint16_t addr, const uint16_t addr_start, std::vector<uint32_t>* framebuffer)
{
	int x = LGR_ADDR2X[addr - addr_start];	// x start in pixels
	int y = LGR_ADDR2Y[addr - addr_start];	// y in pixels
	if (x < 0 || y < 0)	// the holes!
		return;

	// Everything is double the resolution
	x *= 2;
	y *= 2;
	uint8_t mainval = *(SDHRManager::GetInstance()->GetApple2MemPtr() + addr);
	uint8_t auxval = *(SDHRManager::GetInstance()->GetApple2MemAuxPtr() + addr);

	const uint8_t auxval_h = auxval >> 4;
	const uint8_t auxval_l = auxval & 0xF;
	auxval = (ROL_NIB(auxval_h) << 4) | ROL_NIB(auxval_l);

	uint8_t colorIdx;
	// Set all 7 dots of aux mem in the top 4 rows for the low 4 bits color
	// and the bottom 4 rows for the high bits color
	// Duplicate each row for the double resolution rows
	// And do it again for the 7 dots of main mem
	for (size_t j = 0; j < 8; j++)
	{
		colorIdx = (j < 4) ? (auxval & 0xF) : (auxval & 0xF0) >> 4;
		for (size_t i = 0; i < 7; i++)
		{
			framebuffer->at(y * _A2VIDEO_MIN_WIDTH + x + i) = gPaletteRGB[colorIdx + 12];	// LoRes colors start at index 12
			framebuffer->at((y + 1) * _A2VIDEO_MIN_WIDTH + x + i) = gPaletteRGB[colorIdx + 12];
		}
		colorIdx = (j < 4) ? (mainval & 0xF) : (mainval & 0xF0) >> 4;
		for (size_t i = 7; i < 14; i++)
		{
			framebuffer->at(y * _A2VIDEO_MIN_WIDTH + x + i) = gPaletteRGB[colorIdx + 12];	// LoRes colors start at index 12
			framebuffer->at((y + 1) * _A2VIDEO_MIN_WIDTH + x + i) = gPaletteRGB[colorIdx + 12];
		}

		y += 2;
	}
}

// Updates a single cell given a memory byte change
// The "cell" is 7 consecutive bits
void A2VideoManager::UpdateHiResRGBCell(uint16_t addr, const uint16_t addr_start, std::vector<uint32_t>* framebuffer)
{
	// first get the number of bytes from the start of the lines, i.e. the xb value
	int x = HGR_ADDR2X[addr - addr_start];	// x start in pixels
	int y = HGR_ADDR2Y[addr - addr_start];	// y in pixels
	if (x < 0 || y < 0)	// the holes!
		return;
	uint8_t xb = x / 7;	// x in bytes
	uint8_t xoffset = xb & 1; // offset to start of the 2 bytes. Always start with the even byte
	addr -= xoffset;
	x = HGR_ADDR2X[addr - addr_start];
	// Everything is double the resolution
	x *= 2;
	y *= 2;

	uint8_t* pMain = SDHRManager::GetInstance()->GetApple2MemPtr() + addr;

	// We need all 28 bits because each pixel needs a three bit evaluation
	// Anything outside the bounds of the row is 0
	uint8_t byteval1 = (xb < 2 ? 0 : *(pMain - 1));
	uint8_t byteval2 = *pMain;
	uint8_t byteval3 = *(pMain + 1);
	uint8_t byteval4 = (xb >= 38 ? 0 : *(pMain + 2));

	// all 28 bits chained
	uint32_t dwordval = (byteval1 & 0x7F) | ((byteval2 & 0x7F) << 7) | ((byteval3 & 0x7F) << 14) | ((byteval4 & 0x7F) << 21);

	// Extraction of 14 color pixels
	uint32_t colors[14];
	int color = 0;
	uint32_t dwordval_tmp = dwordval;
	dwordval_tmp = dwordval_tmp >> 7;
	bool offset = (byteval2 & 0x80) ? true : false;
	for (int i = 0; i < 14; i++)
	{
		if (i == 7) offset = (byteval3 & 0x80) ? true : false;
		color = dwordval_tmp & 0x3;
		// Two cases for the two palettes
		if (offset)
			colors[i] = gPaletteRGB[1 + color];
		else
			colors[i] = gPaletteRGB[6 - color];
		if (i % 2) dwordval_tmp >>= 2;
	}
	// Black and White
	uint32_t bw[2];
	bw[0] = gPaletteRGB[0];
	bw[1] = gPaletteRGB[1];

	uint32_t mask = 0x01C0; //  00|000001 1|1000000
	uint32_t chck1 = 0x0140; //  00|000001 0|1000000
	uint32_t chck2 = 0x0080; //  00|000000 1|0000000

	// To remove bleed when a pixel is between 2 white pixels
	uint32_t mask0 = 0b0000001111100000;
	uint32_t chck01 = 0b0000001101100000;

	// HIRES render in RGB works on a pixel-basis (1-bit data in framebuffer)
	// The pixel can be 'color', if it makes a 101 or 010 pattern with the two neighbour bits
	// In all other cases, it's black if 0 and white if 1
	// The value of 'color' is defined on a 2-bits basis

	if (xoffset)
	{
		// Second byte of the 14 pixels block
		dwordval = dwordval >> 7;
		xoffset = 7;
	}

	uint32_t dst = (y * _A2VIDEO_MIN_WIDTH) + x + (xoffset * 2);	// destination offset in the pixel framebuffer

	for (int i = xoffset; i < xoffset + 7; i++)
	{
		// remove bleed if a 0 pixel is between 2 white pixels ( 11 0 11 )
		if ((dwordval & mask0) == chck01)
		{
			framebuffer->at(dst) = bw[0];
			framebuffer->at(dst+1) = bw[0];
			dst += 2;
		}
		else if (((dwordval & mask) == chck1) || ((dwordval & mask) == chck2))
		{
			// Color pixel
			framebuffer->at(dst) = colors[i];
			framebuffer->at(dst+1) = colors[i];
			dst += 2;
		}
		else
		{
			// B&W pixel
			framebuffer->at(dst) = bw[(dwordval & chck2 ? 1 : 0)];
			framebuffer->at(dst + 1) = framebuffer->at(dst);
			dst += 2;
		}
		// Next pixel
		dwordval = dwordval >> 1;
	}
	// duplicate on the next row (it may be overridden by the scanlines)
	for (size_t i = 0; i < _A2VIDEO_MIN_WIDTH; i++)
	{
		framebuffer->at((y + 1) * _A2VIDEO_MIN_WIDTH + i) = framebuffer->at(y * _A2VIDEO_MIN_WIDTH + i);
	}
}

void A2VideoManager::UpdateDHiResRGBCell(uint16_t addr, const uint16_t addr_start, std::vector<uint32_t>* framebuffer)
{
	// first get the number of bytes from the start of the lines, i.e. the xb value
	int x = HGR_ADDR2X[addr - addr_start];	// x start in pixels
	int y = HGR_ADDR2Y[addr - addr_start];	// y in pixels
	if (x < 0 || y < 0)	// the holes!
		return;
	uint8_t xb = x / 7;	// x in bytes
	uint8_t xoffset = xb & 1; // offset to start of the 2 bytes. Always start with the even byte
	addr -= xoffset;
	if (IsSoftSwitch(A2SS_PAGE2))
		x = HGR_ADDR2X[_SDHR_MEMORY_SHADOW_END + addr - addr_start];
	else
		x = HGR_ADDR2X[addr - addr_start];
	// Everything is double the resolution
	x *= 2;
	y *= 2;

	uint8_t* pMain = SDHRManager::GetInstance()->GetApple2MemPtr() + addr;
	uint8_t* pAux = SDHRManager::GetInstance()->GetApple2MemAuxPtr() + addr;

	// We need all 28 bits because one 4-bits pixel overlaps two 14-bits cells
	uint8_t byteval1 = *pAux;
	uint8_t byteval2 = *pMain;
	uint8_t byteval3 = *(pAux + 1);
	uint8_t byteval4 = *(pMain + 1);

	// all 28 bits chained
	uint32_t dwordval = (byteval1 & 0x7F) | ((byteval2 & 0x7F) << 7) | ((byteval3 & 0x7F) << 14) | ((byteval4 & 0x7F) << 21);

	// Extraction of 7 color pixels and 7x4 bits
	int bits[7];
	uint32_t colors[7];
	int color = 0;
	uint32_t dwordval_tmp = dwordval;
	for (int i = 0; i < 7; i++)
	{
		bits[i] = dwordval_tmp & 0xF;
		color = ((bits[i] & 7) << 1) | ((bits[i] & 8) >> 3); // DHGR colors are rotated 1 bit to the right
		colors[i] = *reinterpret_cast<const uint32_t*>(&gPaletteRGB[12 + color]);
		dwordval_tmp >>= 4;
	}

	// destination offset in the pixel framebuffer
	// We process a complete byte very time, so the offset for even/odd is 7 pixels * 2
	uint32_t dst = (y * _A2VIDEO_MIN_WIDTH) + x + (xoffset * 14);
	uint32_t* pDst = &framebuffer->at(dst);
	if (xoffset == 0)	// First cell
	{
		// Color

		// Color cell 0
		*(pDst++) = colors[0];
		*(pDst++) = colors[0];
		*(pDst++) = colors[0];
		*(pDst++) = colors[0];
		// Color cell 1
		*(pDst++) = colors[1];
		*(pDst++) = colors[1];
		*(pDst++) = colors[1];

		// Remaining of color cell 1
		*(pDst++) = colors[1];

		// Color cell 2
		*(pDst++) = colors[2];
		*(pDst++) = colors[2];
		*(pDst++) = colors[2];
		*(pDst++) = colors[2];
		// Color cell 3
		*(pDst++) = colors[3];
		*(pDst++) = colors[3];
	}
	else  // Second cell
	{

		// Remaining of color cell 3
		*(pDst++) = colors[3];
		*(pDst++) = colors[3];

		// Color cell 4
		*(pDst++) = colors[4];
		*(pDst++) = colors[4];
		*(pDst++) = colors[4];
		*(pDst++) = colors[4];
		// Color cell 5
		*(pDst++) = colors[5];

		// Remaining of color cell 5
		*(pDst++) = colors[5];
		*(pDst++) = colors[5];
		*(pDst++) = colors[5];

		// Color cell 6
		*(pDst++) = colors[6];
		*(pDst++) = colors[6];
		*(pDst++) = colors[6];
		*(pDst++) = colors[6];
	}

	// duplicate on the next row (it may be overridden by the scanlines)
	for (size_t i = 0; i < _A2VIDEO_MIN_WIDTH; i++)
	{
		framebuffer->at((y + 1) * _A2VIDEO_MIN_WIDTH + i) = framebuffer->at(y * _A2VIDEO_MIN_WIDTH + i);
	}
}