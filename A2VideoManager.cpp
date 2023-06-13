#include "A2VideoManager.h"
#include <cstring>
#include <zlib.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include "SDL.h"
#ifdef _DEBUGTIMINGS
#include <chrono>
#endif

#include "OpenGLHelper.h"
#include "camera.h"
#include "SDHRManager.h"

// below because "The declaration of a static data member in its class definition is not a definition"
A2VideoManager* A2VideoManager::s_instance;

static OpenGLHelper* oglHelper = OpenGLHelper::GetInstance();

static Camera camera;
static Shader shader_a2video_text = Shader();
static Shader shader_a2video_lores = Shader();
static Shader shader_a2video_hgr = Shader();


// TODO:
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
	// Reset the camera
	camera = Camera(
		rendererOutputWidth / 2.f,
		rendererOutputHeight / 2.f,					// x,y
		A2VIDEO_TOTAL_COUNT,						// z
		0.f, 1.f, 0.f,								// upVector xyz
		-90.f,										// yaw
		0.f											// pitch
	);

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

	// Initialize windows and meshes
	
	// TEXT1
	windows[A2VIDEO_TEXT1].Define(
		A2VIDEO_TEXT1,
		uXY({ (uint32_t)rendererOutputWidth , (uint32_t)rendererOutputHeight }),
		uXY({ _A2_TEXT40_CHAR_WIDTH, _A2_TEXT40_CHAR_HEIGHT }),
		uXY({ 40, 24 }),
		SDHRManager::GetInstance()->GetApple2MemPtr() + 0x400,		// Pointer to TEXT1
		0x400,														// Size of TEXT1
		&shader_a2video_text
	);
	// Activate TEXT1
	windows[A2VIDEO_TEXT1].enabled = true;
	// tell the next Render() call to run initialization routines
	bShouldInitializeRender = true;
}

A2VideoManager::~A2VideoManager()
{

}

void A2VideoManager::NotifyA2MemoryDidChange(uint32_t addr)
{
	if (addr >= 0x400 && addr < 0x800)
		windows[A2VIDEO_TEXT1].bNeedsGPUDataUpdate = true;
}

void A2VideoManager::SelectVideoMode(A2VideoMode_e mode)
{
	activeVideoMode = mode;
	bShouldInitializeRender = true;
	for (auto& _w : this->windows) {
		_w.enabled = false;
	}
	this->windows[activeVideoMode].enabled = true;
	if (!bIsMixedMode)
		return;

	// Now handle mixed mode
	switch (mode)
	{
	case A2VIDEO_LORES:
		this->windows[A2VIDEO_TEXT1].enabled = true;
		break;
	case A2VIDEO_HGR1:
		this->windows[A2VIDEO_TEXT1].enabled = true;
		break;
	case A2VIDEO_HGR2:
		this->windows[A2VIDEO_TEXT1].enabled = true;
		break;
	case A2VIDEO_DLORES:
		this->windows[A2VIDEO_DTEXT].enabled = true;
		break;
	case A2VIDEO_DHGR:
		this->windows[A2VIDEO_DTEXT].enabled = true;
		break;
	default:
		break;
	}
}

void A2VideoManager::ToggleMixedMode()
{
	bIsMixedMode = !bIsMixedMode;
	SelectVideoMode(activeVideoMode);	// Force reinitialization
}

A2VideoMode_e A2VideoManager::ActiveVideoMode()
{
	return activeVideoMode;
}

void A2VideoManager::Render()
{
	if (!bA2VideoEnabled)
		return;

	GLenum glerr;
	auto oglh = OpenGLHelper::GetInstance();

	oglh->setup_render();

	if (bDidChangeResolution) {
		oglh->rescale_framebuffer(rendererOutputWidth, rendererOutputHeight);
	}

	// activate the correct shader
	auto currentShader = windows[activeVideoMode].GetShaderProgram();
	currentShader->use();
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL glUseProgram error: " << glerr << std::endl;
	}

	// Initialization routine runs only once on init (or re-init)
	// We do that here because we know the framebuffer is bound, and everything
	// for drawing the SDHR stuff is active
	if (bShouldInitializeRender) {
		bShouldInitializeRender = false;

		// image asset 0: The apple 2e US font
		glActiveTexture(_SDHR_START_TEXTURES);
		image_assets[0].AssignByFilename(this, "Apple2eFont7x8 - Regular.png");
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

	// Assign the list of all the textures to the shader's "tilesTexture" uniform
	auto texUniformId = glGetUniformLocation(currentShader->ID, "tilesTexture");
	glUniform1i(texUniformId, _SDHR_START_TEXTURES - GL_TEXTURE0);
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL glUniform1iv error: " << glerr << std::endl;
	}

	// Assign the sdhr global (to all windows) uniforms
	currentShader->setInt("ticks", SDL_GetTicks());

	// Render the windows (i.e. the meshes with the windows stencils)
	auto mat_proj = glm::ortho<float>(-rendererOutputWidth / 2, rendererOutputWidth / 2, 
		-rendererOutputHeight / 2, rendererOutputHeight / 2, 0, 256);

	for (auto& _w : this->windows) {
		_w.Render(camera.GetViewMatrix(), mat_proj);
	}
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL draw error: " << glerr << std::endl;
	}
	oglh->cleanup_render();
	bDidChangeResolution = false;
}
