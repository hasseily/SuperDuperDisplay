#include "OpenGLHelper.h"

#include <iostream>
#include <string>
#include "glm/gtc/epsilon.hpp"
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include <SDL.h>

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