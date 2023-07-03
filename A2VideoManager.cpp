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

// below because "The declaration of a static data member in its class definition is not a definition"
A2VideoManager* A2VideoManager::s_instance;
uint16_t A2VideoManager::a2SoftSwitches = 0;

static OpenGLHelper* oglHelper = OpenGLHelper::GetInstance();

static Shader shader_a2video_text = Shader();
static Shader shader_a2video_lores = Shader();
static Shader shader_a2video_hgr = Shader();

/*
	For TEXT1 support:
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
		- Create a window and mesh for each mode
		- For GR mode, create tileset of 40x48 7x4 tiles (or just 40x48 pixels?)
			- MosaicTile structure has a single byte for the color
			- Shader just determines which tile it's on, picks up the color and draws
		- For HGR modes, no tilesets. Every render we send $2000 to $4000 as a texture2D
			- shader determines which byte it needs based on where it is
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
			_A2VIDEO_MIN_WIDTH * _A2VIDEO_DEFAULT_ZOOM) ,	// in dots
			(uint32_t)(_A2VIDEO_MIN_HEIGHT * _A2VIDEO_DEFAULT_ZOOM) }),
		uXY({
			1,
			1 }),
			uXY({
				_A2VIDEO_MIN_WIDTH,			// in dots
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
	if (addr >= 0x400 && addr < 0x800)
		windows[A2VIDEO_TEXT1].bNeedsGPUDataUpdate = true;
	else if (addr >= 0x800 && addr < 0xB00)
		windows[A2VIDEO_TEXT2].bNeedsGPUDataUpdate = true;
	else if (addr >= 0x2000 && addr < 0x4000)
		windows[A2VIDEO_HGR1].bNeedsGPUDataUpdate = true;
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


