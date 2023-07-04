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

static const std::map<uint16_t, uint8_t> mapHGRRows = {
{0x0000, 0}, {0x0400, 1}, {0x0800, 2}, {0x0C00, 3}, {0x1000, 4}, {0x1400, 5}, {0x1800, 6}, {0x1C00, 7},
{0x0080, 8}, {0x0480, 9}, {0x0880, 10}, {0x0C80, 11}, {0x1080, 12}, {0x1480, 13}, {0x1880, 14}, {0x1C80, 15},
{0x0100, 16}, {0x0500, 17}, {0x0900, 18}, {0x0D00, 19}, {0x1100, 20}, {0x1500, 21}, {0x1900, 22}, {0x1D00, 23},
{0x0180, 24}, {0x0580, 25}, {0x0980, 26}, {0x0D80, 27}, {0x1180, 28}, {0x1580, 29}, {0x1980, 30}, {0x1D80, 31},
{0x0200, 32}, {0x0600, 33}, {0x0A00, 34}, {0x0E00, 35}, {0x1200, 36}, {0x1600, 37}, {0x1A00, 38}, {0x1E00, 39},
{0x0280, 40}, {0x0680, 41}, {0x0A80, 42}, {0x0E80, 43}, {0x1280, 44}, {0x1680, 45}, {0x1A80, 46}, {0x1E80, 47},
{0x0300, 48}, {0x0700, 49}, {0x0B00, 50}, {0x0F00, 51}, {0x1300, 52}, {0x1700, 53}, {0x1B00, 54}, {0x1F00, 55},
{0x0380, 56}, {0x0780, 57}, {0x0B80, 58}, {0x0F80, 59}, {0x1380, 60}, {0x1780, 61}, {0x1B80, 62}, {0x1F80, 63},
{0x0028, 64}, {0x0428, 65}, {0x0828, 66}, {0x0C28, 67}, {0x1028, 68}, {0x1428, 69}, {0x1828, 70}, {0x1C28, 71},
{0x00A8, 72}, {0x04A8, 73}, {0x08A8, 74}, {0x0CA8, 75}, {0x10A8, 76}, {0x14A8, 77}, {0x18A8, 78}, {0x1CA8, 79},
{0x0128, 80}, {0x0528, 81}, {0x0928, 82}, {0x0D28, 83}, {0x1128, 84}, {0x1528, 85}, {0x1928, 86}, {0x1D28, 87},
{0x01A8, 88}, {0x05A8, 89}, {0x09A8, 90}, {0x0DA8, 91}, {0x11A8, 92}, {0x15A8, 93}, {0x19A8, 94}, {0x1DA8, 95},
{0x0228, 96}, {0x0628, 97}, {0x0A28, 98}, {0x0E28, 99}, {0x1228, 100}, {0x1628, 101}, {0x1A28, 102}, {0x1E28, 103},
{0x02A8, 104}, {0x06A8, 105}, {0x0AA8, 106}, {0x0EA8, 107}, {0x12A8, 108}, {0x16A8, 109}, {0x1AA8, 110}, {0x1EA8, 111},
{0x0328, 112}, {0x0728, 113}, {0x0B28, 114}, {0x0F28, 115}, {0x1328, 116}, {0x1728, 117}, {0x1B28, 118}, {0x1F28, 119},
{0x03A8, 120}, {0x07A8, 121}, {0x0BA8, 122}, {0x0FA8, 123}, {0x13A8, 124}, {0x17A8, 125}, {0x1BA8, 126}, {0x1FA8, 127},
{0x0050, 128}, {0x0450, 129}, {0x0850, 130}, {0x0C50, 131}, {0x1050, 132}, {0x1450, 133}, {0x1850, 134}, {0x1C50, 135},
{0x00D0, 136}, {0x04D0, 137}, {0x08D0, 138}, {0x0CD0, 139}, {0x10D0, 140}, {0x14D0, 141}, {0x18D0, 142}, {0x1CD0, 143},
{0x0150, 144}, {0x0550, 145}, {0x0950, 146}, {0x0D50, 147}, {0x1150, 148}, {0x1550, 149}, {0x1950, 150}, {0x1D50, 151},
{0x01D0, 152}, {0x05D0, 153}, {0x09D0, 154}, {0x0DD0, 155}, {0x11D0, 156}, {0x15D0, 157}, {0x19D0, 158}, {0x1DD0, 159},
{0x0250, 160}, {0x0650, 161}, {0x0A50, 162}, {0x0E50, 163}, {0x1250, 164}, {0x1650, 165}, {0x1A50, 166}, {0x1E50, 167},
{0x02D0, 168}, {0x06D0, 169}, {0x0AD0, 170}, {0x0ED0, 171}, {0x12D0, 172}, {0x16D0, 173}, {0x1AD0, 174}, {0x1ED0, 175},
{0x0350, 176}, {0x0750, 177}, {0x0B50, 178}, {0x0F50, 179}, {0x1350, 180}, {0x1750, 181}, {0x1B50, 182}, {0x1F50, 183},
{0x03D0, 184}, {0x07D0, 185}, {0x0BD0, 186}, {0x0FD0, 187}, {0x13D0, 188}, {0x17D0, 189}, {0x1BD0, 190}, {0x1FD0, 191}
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
	v_fbhgr1.reserve(_A2VIDEO_MIN_WIDTH * _A2VIDEO_MIN_HEIGHT);
	v_fbhgr2.reserve(_A2VIDEO_MIN_WIDTH * _A2VIDEO_MIN_HEIGHT);
	v_fbdhgr.reserve(_A2VIDEO_MIN_WIDTH * 2 * _A2VIDEO_MIN_HEIGHT);
	v_fbhgr1.clear();
	v_fbhgr2.clear();
	v_fbdhgr.clear();

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
		uXY({ (uint32_t)(
			_A2VIDEO_MIN_WIDTH * _A2VIDEO_DEFAULT_ZOOM) ,
			(uint32_t)(_A2VIDEO_MIN_HEIGHT * _A2VIDEO_DEFAULT_ZOOM) }),
		uXY({
			_A2_TEXT40_CHAR_WIDTH,
			_A2_TEXT40_CHAR_HEIGHT }),
			uXY({
				40,
				24 }),
				SDHRManager::GetInstance()->GetApple2MemPtr() + 0x400,		// Pointer to TEXT1
				0x400,														// Size of TEXT1
				&shader_a2video_text
				);
	// TEXT2
	windows[A2VIDEO_TEXT2].Define(
		A2VIDEO_TEXT2,
		uXY({ (uint32_t)(
			_A2VIDEO_MIN_WIDTH * _A2VIDEO_DEFAULT_ZOOM) ,
			(uint32_t)(_A2VIDEO_MIN_HEIGHT * _A2VIDEO_DEFAULT_ZOOM) }),
		uXY({
			_A2_TEXT40_CHAR_WIDTH,
			_A2_TEXT40_CHAR_HEIGHT }),
			uXY({
				40,
				24 }),
				SDHRManager::GetInstance()->GetApple2MemPtr() + 0x800,		// Pointer to TEXT2
				0x400,														// Size of TEXT2
				&shader_a2video_text
				);
	// HGR1
	windows[A2VIDEO_HGR1].Define(
		A2VIDEO_HGR1,
		uXY({ (uint32_t)(
			_A2VIDEO_MIN_WIDTH * 2 * _A2VIDEO_DEFAULT_ZOOM) ,	// in dots
			(uint32_t)(_A2VIDEO_MIN_HEIGHT * _A2VIDEO_DEFAULT_ZOOM) }),
		uXY({
			1,
			1 }),
			uXY({
				_A2VIDEO_MIN_WIDTH * 2,			// in dots
				_A2VIDEO_MIN_HEIGHT }),		// 192 lines
				SDHRManager::GetInstance()->GetApple2MemPtr() + 0x2000,
				0x2000,
				&shader_a2video_hgr
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
		UpdateHiResRGBCell(addr, _A2VIDEO_HGR1_START, &v_fbhgr1);
		windows[A2VIDEO_HGR1].bNeedsGPUDataUpdate = true;
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
	// first get the number of bytes from the start of the lines, i.e. the x value
	const uint8_t x = ((addr - addr_start) & 0x7F) % 0x28;
	auto _res = mapHGRRows.find((addr - addr_start) - x);
	if (_res != mapHGRRows.end())
	{
		std::cerr << "ERROR: Couldn't find HGR row value. Something is very wrong" << std::endl;
		return;
	}
	const uint8_t y = _res->second;
	int xoffset = x & 1; // offset to start of the 2 bytes
	addr -= xoffset;

	uint8_t* pMain = SDHRManager::GetInstance()->GetApple2MemPtr() + addr;

	// We need all 28 bits because each pixel needs a three bit evaluation
	// Anything outside the bounds of the row is 0
	uint8_t byteval1 = (x < 2 ? 0 : *(pMain - 1));
	uint8_t byteval2 = *pMain;
	uint8_t byteval3 = *(pMain + 1);
	uint8_t byteval4 = (x >= 38 ? 0 : *(pMain + 2));

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

	uint32_t* pDst = (uint32_t*)framebuffer;

	if (xoffset)
	{
		// Second byte of the 14 pixels block
		dwordval = dwordval >> 7;
		xoffset = 7;
	}

	for (int i = xoffset; i < xoffset + 7; i++)
	{
		// remove bleed if a 0 pixel is between 2 white pixels ( 11 0 11 )
		if ((dwordval & mask0) == chck01)
		{
			*(pDst) = bw[0];
			*(pDst + 1) = *(pDst);
			pDst += 2;
		}
		else if (((dwordval & mask) == chck1) || ((dwordval & mask) == chck2))
		{
			// Color pixel
			*(pDst) = colors[i];
			*(pDst + 1) = *(pDst);
			pDst += 2;
		}
		else
		{
			// B&W pixel
			*(pDst) = bw[(dwordval & chck2 ? 1 : 0)];
			*(pDst + 1) = *(pDst);
			pDst += 2;
		}
		// Next pixel
		dwordval = dwordval >> 1;
	}
}
