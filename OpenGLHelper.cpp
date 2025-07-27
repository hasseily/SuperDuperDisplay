#include "OpenGLHelper.h"

#include <iostream>
#include <string>
#include "glm/gtc/epsilon.hpp"
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include <SDL.h>

// To get the exact size of the window for the screenshot
extern SDL_Window* Main_GetSDLWindow();

// below because "The declaration of a static data member in its class definition is not a definition"
OpenGLHelper* OpenGLHelper::s_instance;

//////////////////////////////////////////////////////////////////////////
// Basic singleton methods
//////////////////////////////////////////////////////////////////////////

void OpenGLHelper::Initialize()
{
	for (size_t i = 0; i < _SDHR_MAX_TEXTURES; i++)
	{
		v_texture_ids.push_back(UINT_MAX);
	}
}

OpenGLHelper::~OpenGLHelper()
{
	for (auto texid : v_texture_ids)
	{
		glDeleteTextures(1, &texid);
	}
}

//////////////////////////////////////////////////////////////////////////
// Image Asset Methods
//////////////////////////////////////////////////////////////////////////

// NOTE:	The below image asset method uses OpenGL
//			so it _must_ be called from the main thread
void OpenGLHelper::ImageAsset::AssignByFilename(const char* filename) {
	int width;
	int height;
	int channels;
	unsigned char* data = stbi_load(filename, &width, &height, &channels, 4);
	if (data == NULL) {
		std::cerr << "ERROR: STBI load failure: " << stbi_failure_reason() << std::endl;
		std::cerr << "Tried loading: " << filename << std::endl;
		image_xcount = 0;
		image_ycount = 0;
		return;
	}
	if (tex_id != UINT_MAX)
	{
		OpenGLHelper::GetInstance()->load_texture(data, width, height, channels, tex_id);
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
// Methods
//////////////////////////////////////////////////////////////////////////

// Sets the correct gl version and returns the glsl version string
void OpenGLHelper::set_gl_version()
{
	// Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
	// GL ES 3.0 + GLSL 300 es
    // ImGui only supports 3.0, not 3.1
	glsl_version = "#version 300 es";
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#elif defined(__APPLE__)
	// GL 4.1 Core + GLSL 410
	glsl_version = "#version 410";
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG); // Always required on Mac
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
#else
	// GL 4.1 Core + GLSL 410
	glsl_version = "#version 410";
	if (SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0) != 0)
		std::cerr << "SDL Error: " << SDL_GetError() << std::endl;
	if (SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE) != 0)
		std::cerr << "SDL Error: " << SDL_GetError() << std::endl;
	if (SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4) != 0)
		std::cerr << "SDL Error: " << SDL_GetError() << std::endl;
	if (SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1) != 0)
		std::cerr << "SDL Error: " << SDL_GetError() << std::endl;
#endif

}


const std::string* OpenGLHelper::get_glsl_version()
{
	return &glsl_version;
}

// This method loads the texture data into the texture specified at textureID
void OpenGLHelper::load_texture(unsigned char* data, int width, int height, int nrComponents, GLuint textureID)
{
	GLenum glerr;
	GLenum format = GL_RGBA;
	if (nrComponents == 1)
		format = GL_RED;
	else if (nrComponents == 3)
		format = GL_RGBA;
	else if (nrComponents == 4)
		format = GL_RGBA;

	glBindTexture(GL_TEXTURE_2D, textureID);
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL load_texture glBindTexture error: " << glerr << std::endl;
	}
	glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL load_texture glTexImage2D error: " << glerr << std::endl;
	}
	// NOTE: May need to generate mipmaps in case we want to allow zooming in-out
	// But then we need to change the GL_TEXTURE_MIN_FILTER to GL_XXX_MIPMAP_XXX
	//glGenerateMipmap(GL_TEXTURE_2D);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);	// Note: Could also use GL_LINEAR, need to test
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL load_texture glTexParameteri error: " << glerr << std::endl;
	}
}

GLuint OpenGLHelper::get_texture_id_at_slot(int slot)
{
	if (slot >= v_texture_ids.size())
	{
#ifdef DEBUG
		std::cerr << "ERROR: Requesting a texture slot above _SDHR_MAX_TEXTURES!\n";
#endif
		return UINT_MAX;
	}
	GLuint texid = v_texture_ids.at(slot);
	if (texid == UINT_MAX)
	{
		glGenTextures(1, &texid);
		v_texture_ids.at(slot) = texid;
	}
	return texid;
}

glm::vec2 OpenGLHelper::get_dpi_scaling_factors(SDL_Window* window) {
	int windowWidth = 0, windowHeight = 0;
	int drawableWidth = 0, drawableHeight = 0;
	SDL_GetWindowSize(window, &windowWidth, &windowHeight);
	SDL_GL_GetDrawableSize(window, &drawableWidth, &drawableHeight);
	// std::cout << "Logical window size: " << windowWidth << "x" << windowHeight << "\n";
	// std::cout << "Actual drawable size: " << drawableWidth << "x" << drawableHeight << "\n";

	float scaleX = static_cast<float>(drawableWidth) / windowWidth;
	float scaleY = static_cast<float>(drawableHeight) / windowHeight;
	// std::cout << "Calculated scaling factors - X: " << scaleX << ", Y: " << scaleY << "\n";
	return glm::vec2(scaleX, scaleY);
}

bool OpenGLHelper::are_matrices_approx_equal(const glm::mat4& m1, const glm::mat4& m2, float epsilon)
{
	for (int i = 0; i < 4; ++i) {
		if (!glm::all(glm::epsilonEqual(m1[i], m2[i], epsilon)))
			return false;
	}
	return true;
}

bool OpenGLHelper::SaveFramebufferBMP(const std::string& filename) {
	int _w = 0, _h = 0;
	SDL_GL_GetDrawableSize(Main_GetSDLWindow(), &_w, &_h);

	// Read RGBA pixels from the back framebuffer
	// The back framebuffer has the latest render without the ImGUI stuff yet
	// If we were to use the front framebuffer it'd have the menu in it
	std::vector<GLubyte> pixels(_w * _h * 4);
	glReadBuffer(GL_BACK);
	glReadPixels(0, 0, _w, _h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

	GLenum glerr;
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL SaveFramebufferBMP error: " << glerr << std::endl;
	}

	// Flip rows because OpenGL’s origin is bottom-left, SDL’s is top-left
	for (int y = 0; y < _h/2; ++y) {
		int idx1 = y * _w * 4;
		int idx2 = (_h - 1 - y) * _w * 4;
		for (int x = 0; x < _w*4; ++x)
			std::swap(pixels[idx1 + x], pixels[idx2 + x]);
	}

	// Create an SDL surface from the pixel data
	SDL_Surface* surface = SDL_CreateRGBSurfaceFrom(
													pixels.data(),
													_w, _h,
													32,			// depth
													_w * 4,		// pitch (bytes per row)
													0x000000FF,	// R mask
													0x0000FF00,	// G mask
													0x00FF0000,	// B mask
													0xFF000000	// A mask
													);
	if (!surface) {
		std::cerr << "SDL_CreateRGBSurfaceFrom failed: " << SDL_GetError() << "\n";
		return false;
	}

	// Save to BMP
	if (SDL_SaveBMP(surface, filename.c_str()) != 0) {
		std::cerr << "SDL_SaveBMP failed: " << SDL_GetError() << "\n";
		SDL_FreeSurface(surface);
		return false;
	}

	SDL_FreeSurface(surface);
	return true;
}

bool OpenGLHelper::SaveTextureInSlotBMP(GLuint slot, const std::string& filename) {
	// Find the current texture id that is in this slot, and call SaveTextureBMP
	glActiveTexture(slot);
	GLint target_tex_id = 0;
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &target_tex_id);
	glActiveTexture(GL_TEXTURE0);
	return this->SaveTextureBMP(target_tex_id, filename);
}

bool OpenGLHelper::SaveTextureBMP(GLuint tex, const std::string& filename) {
	GLint _w = 0, _h = 0;
	glBindTexture(GL_TEXTURE_2D, tex);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &_w);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &_h);
	glBindTexture(GL_TEXTURE_2D, 0);

	if (_w == 0 || _h == 0)
		return false;

	// Create temporary FBO
	GLuint fbo = 0;
	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER,
						   GL_COLOR_ATTACHMENT0,
						   GL_TEXTURE_2D,
						   tex, 0);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		std::cerr << "FBO incomplete!\n";
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glDeleteFramebuffers(1, &fbo);
		return false;
	}

	std::vector<GLubyte> pixels(_w * _h * 4);
	glReadBuffer(GL_COLOR_ATTACHMENT0);
	glReadPixels(0, 0, _w, _h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

	// Cleanup FBO binding
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDeleteFramebuffers(1, &fbo);

	GLenum glerr;
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL SaveTextureBMP error: " << glerr << std::endl;
	}

	// we don't flip rows, it's already inverted

	// Create SDL surface
	SDL_Surface* surface = SDL_CreateRGBSurfaceFrom(
													pixels.data(),
													_w, _h,
													32,			// depth
													_w * 4,		// pitch (bytes per row)
													0x000000FF,	// R mask
													0x0000FF00,	// G mask
													0x00FF0000,	// B mask
													0xFF000000	// A mask
													);
	if (!surface) {
		std::cerr << "SDL_CreateRGBSurfaceFrom failed: " << SDL_GetError() << "\n";
		return false;
	}

	// Save
	if (SDL_SaveBMP(surface, filename.c_str()) != 0) {
		std::cerr << "SDL_SaveBMP failed: " << SDL_GetError() << "\n";
		SDL_FreeSurface(surface);
		return false;
	}

	SDL_FreeSurface(surface);
	return true;
}

std::string OpenGLHelper::GetScreenshotSaveFilePath()
{
	std::filesystem::path appRoot = std::filesystem::current_path();
	std::filesystem::path screenshotsDir = appRoot / "screenshots";
	if (!std::filesystem::exists(screenshotsDir)) {
		if (!std::filesystem::create_directory(screenshotsDir)) {
			throw std::runtime_error("Failed to create directory: " + screenshotsDir.string());
		}
	}

	auto now = std::chrono::system_clock::now();
	std::time_t now_c = std::chrono::system_clock::to_time_t(now);
	std::tm tm_local;
#ifdef _WIN32
	localtime_s(&tm_local, &now_c);  // Thread-safe on Windows
#else
	std::tm* tm_ptr = std::localtime(&now_c);  // Note: not thread-safe
	if (tm_ptr) {
		tm_local = *tm_ptr;
	}
	else {
		throw std::runtime_error("Failed to convert current time.");
	}
#endif
	std::stringstream ss;
	ss << std::put_time(&tm_local, "%Y%m%d_%H%M%S");	// "YYYYMMDD_HHMMSS"
	std::string ssname = "Apple2_Screenshot_" + ss.str() + ".bmp";

	// Construct the file path: screenshots/<timestamp>
	std::filesystem::path filePath = screenshotsDir / ssname;

	return filePath.string();

}
