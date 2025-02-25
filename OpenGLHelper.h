#pragma once
#ifndef OPENGLHELPER_H
#define OPENGLHELPER_H
#include <stdint.h>
#include <stddef.h>
#include <vector>
#include <string>
#include "common.h"
#include "shader.h"
#include "camera.h"

/*
	This class has helper methods for versioning, managing textures...
	It does not manage any framebuffers
*/
class OpenGLHelper
{
public:
	// An image asset is a texture with its metadata (width, height)
	// The actual texture data is in the GPU memory
	struct ImageAsset {
		void AssignByFilename(const char* filename);

		// image assets are full 32-bit bitmap files, uploaded from PNG
		uint32_t image_xcount = 0;	// width and height of asset in pixels
		uint32_t image_ycount = 0;
		GLuint tex_id = UINT_MAX;	// Texture ID on the GPU that holds the image data
	};

	void set_gl_version();	// must be called after SDL_Init()
	const std::string* get_glsl_version();	// returns the glsl version string
	void load_texture(unsigned char* data, int width, int height, int nrComponents, GLuint textureID);
	GLuint get_texture_id_at_slot(int slot);	// returns the opengl-generated texture id for this tex slot
	glm::vec2 get_dpi_scaling_factors(SDL_Window* window);		// returns the scaling of width and height for high dpi screens

	// The created texture ids (max is _SDHR_MAX_TEXTURES)
	std::vector<GLuint>v_texture_ids;

	// public singleton code
	static OpenGLHelper* GetInstance()
	{
		if (NULL == s_instance)
			s_instance = new OpenGLHelper();
		return s_instance;
	}
	~OpenGLHelper();

private:
//////////////////////////////////////////////////////////////////////////
// Singleton pattern
//////////////////////////////////////////////////////////////////////////
	void Initialize();

	static OpenGLHelper* s_instance;
	OpenGLHelper()
	{
		Initialize();
	}

	//////////////////////////////////////////////////////////////////////////
	// Internal attributes
	//////////////////////////////////////////////////////////////////////////
	std::string glsl_version = "#version 100";
};
#endif // OPENGLHELPER_H
