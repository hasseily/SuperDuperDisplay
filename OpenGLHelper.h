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
	// public singleton code
	static OpenGLHelper* GetInstance()
	{
		if (NULL == s_instance)
			s_instance = new OpenGLHelper();
		return s_instance;
	}
	~OpenGLHelper();

	void set_gl_version();	// must be called after SDL_Init()
	const std::string* get_glsl_version();	// returns the glsl version string
	void load_texture(unsigned char* data, int width, int height, int nrComponents, GLuint textureID);
	GLuint get_texture_id_at_slot(int slot);	// returns the opengl-generated texture id for this tex slot

	// The created texture ids (max is _SDHR_MAX_TEXTURES)
	std::vector<GLuint>v_texture_ids;

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
