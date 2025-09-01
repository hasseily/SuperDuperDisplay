#include "OpenGLHelper.h"

#include <iostream>
#include <string>
#include <thread>
#include "glm/gtc/epsilon.hpp"
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "LogTextManager.h"
#include <SDL.h>

// The stb_image_write PNG implementation is suboptimal. So we're combining it with miniz for
// a close to optimal compression. See:
// https://blog.gibson.sh/2015/07/18/comparing-png-compression-ratios-of-stb_image_write-lodepng-miniz-and-libpng/

#include "miniz.h"
#define STBIW_MALLOC(sz) SDL_malloc(sz)
#define STBIW_FREE(p)	SDL_free(p)
#define STBIW_REALLOC(p)	SDL_realloc(p)
static unsigned char* my_stbi_zlib_compress(unsigned char* data, int data_len, int* out_len, int quality)
{
	// Guard: data_len must be >= 0 for the miniz API (it takes size_t internally).
	if (data_len < 0) return nullptr;

	mz_ulong in_len = static_cast<mz_ulong>(data_len);
	mz_ulong buf_len = mz_compressBound(in_len);

	unsigned char* buf = static_cast<unsigned char*>(STBIW_MALLOC(buf_len));
	if (!buf) return nullptr;

	// miniz "quality" is zlib level (0..9). Clamp to be safe.
	if (quality < 0) quality = 0;
	if (quality > 9) quality = 9;

	int rc = mz_compress2(buf, &buf_len, data, in_len, quality);
	if (rc != MZ_OK) {
		STBIW_FREE(buf);
		return nullptr;
	}

	*out_len = static_cast<int>(buf_len);
	return buf; // stb_image_write will free via STBIW_FREE
}
#define STBIW_ZLIB_COMPRESS  my_stbi_zlib_compress
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// To get the exact size of the window for the screenshot
extern SDL_Window* Main_GetSDLWindow();
// To get the image format
extern bool Main_GetbUsePNGForScreenshots();

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
// We assume the image is sRGB so it'll be loaded as such, and will automatically
// convert to linear RGB when sampled
void OpenGLHelper::load_texture(unsigned char* data, int width, int height, int nrComponents, GLuint textureID)
{
	GLenum glerr;
	GLenum format = GL_SRGB8_ALPHA8;
	if (nrComponents == 1)
		format = GL_RED;
	else if (nrComponents == 3)
		format = GL_SRGB8;
	else if (nrComponents == 4)
		format = GL_SRGB8_ALPHA8;

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
	if (slot >= (int)v_texture_ids.size())
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

bool OpenGLHelper::SaveFramebufferToFile(const std::string& filename, bool bUsePNG) {
	int _w = 0, _h = 0;
	SDL_GL_GetDrawableSize(Main_GetSDLWindow(), &_w, &_h);
	if (_w <= 0 || _h <= 0) return false;

	// Read RGBA pixels from the back framebuffer
	// The back framebuffer has the latest render without the ImGUI stuff yet
	// If we were to use the front framebuffer it'd have the menu in it
	std::vector<GLubyte> pixels(static_cast<size_t>(_w) * static_cast<size_t>(_h) * 4);

	glReadBuffer(GL_BACK);

	// Ensure tight packing (no 4-byte alignment padding beyond stride = _w*4)
	GLint oldPack = 0;
	glGetIntegerv(GL_PACK_ALIGNMENT, &oldPack);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, _w, _h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
	glPixelStorei(GL_PACK_ALIGNMENT, oldPack);

	GLenum glerr;
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL SaveFramebufferToFile error: " << glerr << std::endl;
		// continue; data may still be valid
	}

	// Wrap the rest of the code in a thread because the PNG compression can be quite expensive
	// and a Raspberry Pi will take hundreds of milliseconds to process a 1080p image

	std::thread([filename, bUsePNG, w = _w, h = _h, px = std::move(pixels)]() mutable {
		// Flip rows because OpenGL’s origin is bottom-left and PNG/top-left tools expect top-left
		for (int y = 0; y < h / 2; ++y) {
			int idx1 = y * w * 4;
			int idx2 = (h - 1 - y) * w * 4;
			for (int x = 0; x < w * 4; ++x)
				std::swap(px[idx1 + x], px[idx2 + x]);
		}

		if (bUsePNG) {
			// Write PNG (4 components, stride = w * 4)
			if (!stbi_write_png(filename.c_str(), w, h, 4, px.data(), w * 4)) {
				LogStreamErr() << "stbi_write_png failed";
			}
			else {
				LogStream() << "SCREENSHOT SAVED - " << filename;
			}
		}
		else {	// basic BMP

			// Create an SDL surface from the pixel data
			SDL_Surface* surface = SDL_CreateRGBSurfaceFrom(
				px.data(),
				w, h,
				32,			// depth
				w * 4,		// pitch (bytes per row)
				0x000000FF,	// R mask
				0x0000FF00,	// G mask
				0x00FF0000,	// B mask
				0xFF000000	// A mask
			);
			if (!surface) {
				LogStreamErr() << "SDL_CreateRGBSurfaceFrom failed: " << SDL_GetError();
			}
			else {
				// Save to BMP
				if (SDL_SaveBMP(surface, filename.c_str()) != 0) {
					LogStreamErr() << "SDL_SaveBMP failed: " << SDL_GetError();
				}
				else {
					LogStream() << "SCREENSHOT SAVED - " << filename;
				}
				SDL_FreeSurface(surface);
			}
		}
	}).detach();

	return true;
}

bool OpenGLHelper::SaveTextureInSlotToFile(GLuint slot, const std::string& filename, bool bUsePNG) {
	// Find the current texture id that is in this slot, and call SaveTextureBMP
	glActiveTexture(slot);
	GLint target_tex_id = 0;
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &target_tex_id);
	glActiveTexture(GL_TEXTURE0);
	return this->SaveTextureToFile(target_tex_id, filename, bUsePNG);
}

bool OpenGLHelper::SaveTextureToFile(GLuint tex, const std::string& filename, bool bUsePNG) {
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
		LogStreamErr() << "FBO incomplete!\n";
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glDeleteFramebuffers(1, &fbo);
		return false;
	}

	std::vector<GLubyte> pixels(static_cast<size_t>(_w) * static_cast<size_t>(_h) * 4);
	glReadBuffer(GL_COLOR_ATTACHMENT0);

	// Ensure tight packing (no 4-byte alignment padding beyond stride = _w*4)
	GLint oldPack = 0;
	glGetIntegerv(GL_PACK_ALIGNMENT, &oldPack);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, _w, _h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
	glPixelStorei(GL_PACK_ALIGNMENT, oldPack);

	// Cleanup FBO binding
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDeleteFramebuffers(1, &fbo);

	GLenum glerr;
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		LogStreamErr() << "OpenGL SaveTextureBMP error: " << glerr << std::endl;
	}

	// Wrap the rest of the code in a thread because the PNG compression can be quite expensive
	// and a Raspberry Pi will take hundreds of milliseconds to process a 1080p image

	std::thread([filename, bUsePNG, w = _w, h = _h, px = std::move(pixels)]() mutable {

		// we don't flip rows, it's already inverted

		if (bUsePNG) {
			// Write PNG (4 components, stride = w * 4)
			if (!stbi_write_png(filename.c_str(), w, h, 4, px.data(), w * 4)) {
				LogStreamErr() << "stbi_write_png failed";
			}
			else {
				LogStream() << "SCREENSHOT SAVED - " << filename;
			}
		}
		else {	// basic BMP
			// Create SDL surface
			SDL_Surface* surface = SDL_CreateRGBSurfaceFrom(
				px.data(),
				w, h,
				32,			// depth
				w * 4,		// pitch (bytes per row)
				0x000000FF,	// R mask
				0x0000FF00,	// G mask
				0x00FF0000,	// B mask
				0xFF000000	// A mask
			);
			if (!surface) {
				LogStreamErr() << "SDL_CreateRGBSurfaceFrom failed: " << SDL_GetError();
			}
			else {
				// Save
				if (SDL_SaveBMP(surface, filename.c_str()) != 0) {
					LogStreamErr() << "SDL_SaveBMP failed: " << SDL_GetError();
				}
				else {
					LogStream() << "SCREENSHOT SAVED - " << filename;
				}
				SDL_FreeSurface(surface);
			}
		}
	}).detach();

	return true;
}

std::string OpenGLHelper::GetScreenshotSaveFilePath()
{
	std::filesystem::path appRoot = std::filesystem::current_path();
	std::filesystem::path screenshotsDir = appRoot / "screenshots";
	if (!std::filesystem::exists(screenshotsDir)) {
		if (!std::filesystem::create_directory(screenshotsDir)) {
			LogStreamErr() << "Failed to create directory: " << screenshotsDir.string();
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
	std::string ssname = "Apple2_Screenshot_" + ss.str();
	ssname += (Main_GetbUsePNGForScreenshots() ? ".png" : ".bmp");

	// Construct the file path: screenshots/<timestamp>
	std::filesystem::path filePath = screenshotsDir / ssname;

	return filePath.string();

}
