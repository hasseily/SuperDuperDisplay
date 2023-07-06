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
#include "HGRAddr2XY.h"

static inline uint32_t SETRGBCOLOR(uint8_t r, uint8_t g, uint8_t b)
{
	return ((0xFF << 24) | (b << 16) | (g << 8) | r);
}

static uint32_t gPaletteRGB[] =
{
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
};

// below because "The declaration of a static data member in its class definition is not a definition"
A2VideoManager* A2VideoManager::s_instance;
uint16_t A2VideoManager::a2SoftSwitches = 0;

static OpenGLHelper* oglHelper = OpenGLHelper::GetInstance();

static Shader shader_a2video_text = Shader();
static Shader shader_a2video_lores = Shader();
static Shader shader_a2video_hgr = Shader();

/*
	For TEXT1/2 support:
		- create a font texture Apple2eFont7x8.png. That's all we'll use for the shader
		- Create a window and a related mesh, both 40*7 x 24*8
		- Create a tileset of 40*24 7x8 tiles
		- Change the MosaicTile structure to use a normal/flashing/inverse flag instead of a tile texture
		- Default shader is the TEXT shader. Give it access to the ticks t variable (for flashing)
		- When toggling TEXT1 mode, request to initialize textures and set the update flag for all cells
		- If in TEXT1 mode, when receiving a memory packet from $400 to $800, set a flag for needing update for that cell
		- If need update on the next render, update the mesh's UpdateMosaicUV:
			- For each changed cell, determine its row via a screen mapping table, its column, and update the MosaicUV
		- When rendering, let the fragment shader do the job and modify the look of the tile given the n/f/i flag

	For other modes support:
		- For GR mode, do the same as TEXT1 but instead of char glyphs, use 2-color rectangles
		- For HGR modes, no tilesets. There are 2 uint32_t framebuffers of rgba.
			- The framebuffers are updated when a memory loc of HGR1/2 is updated
			- The framebuffers are then uploaded to the GPU as necessary
			- The shaders just use the framebuffers as textures
*/

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
	shader_a2video_hgr.build(_SHADER_HGR_VERTEX_DEFAULT, _SHADER_HGR_FRAGMENT_DEFAULT);

	// Initialize windows and meshes
	
	// TEXT1
	windows[A2VIDEO_TEXT1].Define(
		A2VIDEO_TEXT1,
		uXY({ (uint32_t)(_A2VIDEO_MIN_WIDTH) , (uint32_t)(_A2VIDEO_MIN_HEIGHT) }),
		uXY({ _A2_TEXT40_CHAR_WIDTH*2, _A2_TEXT40_CHAR_HEIGHT*2 }),
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

	// TODO: Make the other modes

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
	// TODO: Handle soft switches
	// TODO: Handle video modes
	if (addr >= _A2VIDEO_TEXT1_START && addr < (_A2VIDEO_TEXT1_START + _A2VIDEO_TEXT_SIZE))
		windows[A2VIDEO_TEXT1].bNeedsGPUDataUpdate = true;
	else if (addr >= _A2VIDEO_TEXT2_START && addr < (_A2VIDEO_TEXT2_START + _A2VIDEO_TEXT_SIZE))
		windows[A2VIDEO_TEXT2].bNeedsGPUDataUpdate = true;
	else if (addr >= _A2VIDEO_HGR1_START && addr < (_A2VIDEO_HGR1_START + _A2VIDEO_HGR_SIZE))
	{
		// UpdateHiResRGBCell(addr, _A2VIDEO_HGR1_START, &v_fbhgr1);
		//windows[A2VIDEO_HGR1].bNeedsGPUDataUpdate = true;
	}
	else if (addr >= _A2VIDEO_HGR2_START && addr < (_A2VIDEO_HGR2_START + _A2VIDEO_HGR_SIZE))
	{
		// UpdateHiResRGBCell(addr, _A2VIDEO_HGR2_START, &v_fbhgr2);
		//windows[A2VIDEO_HGR2].bNeedsGPUDataUpdate = true;
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
	default:
		break;
	}
	SelectVideoModes();
}

void A2VideoManager::SelectVideoModes()
{
	for (auto& _w : this->windows) {
		_w.enabled = false;
	}
	if (!(a2SoftSwitches & A2SS_TEXT))
	{
		if (a2SoftSwitches & A2SS_HIRES)
		{
			if (a2SoftSwitches & A2SS_80COL)
				this->windows[A2VIDEO_DHGR].enabled = true;
			else
			{
				if (a2SoftSwitches & A2SS_PAGE2)
					this->windows[A2VIDEO_HGR2].enabled = true;
				else
					this->windows[A2VIDEO_HGR1].enabled = true;
			}
		}
		else {
			if (a2SoftSwitches & A2SS_80COL)
				this->windows[A2VIDEO_DLORES].enabled = true;
			else
			{
				if (a2SoftSwitches & A2SS_PAGE2)
					this->windows[A2VIDEO_LORES2].enabled = true;
				else
					this->windows[A2VIDEO_LORES1].enabled = true;
			}
		}
	}
	if ((a2SoftSwitches & A2SS_TEXT) || (a2SoftSwitches & A2SS_MIXED))
	{
		if (a2SoftSwitches & A2SS_80COL)
			this->windows[A2VIDEO_DTEXT].enabled = true;
		else
		{
			if (a2SoftSwitches & A2SS_PAGE2)
				this->windows[A2VIDEO_TEXT2].enabled = true;
			else
				this->windows[A2VIDEO_TEXT1].enabled = true;
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
		image_assets[0].AssignByFilename(this, "textures/Apple2eFont7x8 - Regular.png");
		// image asset 1: The alternate font
		glActiveTexture(_SDHR_START_TEXTURES + 1);
		image_assets[1].AssignByFilename(this, "textures/Apple2eFont7x8 - Alternate.png");
		// image asset 2: The HGR texture
		glActiveTexture(_SDHR_START_TEXTURES + 2);
		image_assets[2].AssignByFilename(this, "textures/Texture_HGR.png");
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

	if (this->windows[A2VIDEO_HGR1].enabled)
	{
		for (size_t i = 0; i < _A2VIDEO_HGR_SIZE; i++)
		{
			UpdateHiResRGBCell(_A2VIDEO_HGR1_START + i, _A2VIDEO_HGR1_START, &v_fbhgr1);
		}
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, _A2VIDEO_MIN_WIDTH, _A2VIDEO_MIN_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, (void*)(&v_fbhgr1[0]));
	}
	if (this->windows[A2VIDEO_HGR2].enabled)
	{
		for (size_t i = 0; i < _A2VIDEO_HGR_SIZE; i++)
		{
			UpdateHiResRGBCell(_A2VIDEO_HGR2_START + i, _A2VIDEO_HGR2_START, &v_fbhgr2);
		}
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, _A2VIDEO_MIN_WIDTH, _A2VIDEO_MIN_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, (void*)(&v_fbhgr2[0]));
	}

	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL draw error: " << glerr << std::endl;
	}
	oglh->cleanup_render();
}


//////////////////////////////////////////////////////////////////////////
// HGR METHODS
// RGB Videocard code from AppleWin
//////////////////////////////////////////////////////////////////////////

// Updates a single cell given a memory byte change
// The "cell" is 7 consecutive bits
void A2VideoManager::UpdateHiResRGBCell(uint16_t addr, const uint16_t addr_start, std::vector<uint32_t>* framebuffer)
{
	// first get the number of bytes from the start of the lines, i.e. the xb value
	uint16_t x = HGR_ADDR2X[addr - addr_start];	// x start in pixels
	uint16_t y = HGR_ADDR2Y[addr - addr_start];	// y in pixels
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
	uint8_t byteval1 = (xb < 2 ? 0 : *(pMain - 1));	// XXX: should be xb < 1 ?
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
	for (size_t i = 0; i < _A2VIDEO_MIN_WIDTH; i++)
	{
		framebuffer->at((y + 1) * _A2VIDEO_MIN_WIDTH + i) = framebuffer->at(y * _A2VIDEO_MIN_WIDTH + i);
	}
}
