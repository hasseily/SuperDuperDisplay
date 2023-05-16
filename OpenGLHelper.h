#pragma once
#ifndef OPENGLHELPER_H
#define OPENGLHELPER_H
#include <stdint.h>
#include <stddef.h>
#include <vector>
#include "common.h"
#include "shader.h"

#define _SDHR_MAX_TEXTURES 16		// Max # of image assets available

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

	void load_texture(unsigned char* data, int width, int height, int nrComponents, GLuint textureID);
	GLuint get_texture_id() { return output_texture_id; };	// output texture id
	void clear_textures();

	// TODO: Testing, remove
	// void create_vertices();
	void create_framebuffer();	// also binds it
	void bind_framebuffer();
	void unbind_framebuffer();
	unsigned int get_next_free_texture_id();	// returns the next available tex slot that hasn't been used yet. UINT_MAX if full
	void rescale_framebuffer(uint32_t width, uint32_t height);

	void setup_sdhr_render();
	void cleanup_sdhr_render();

	// TODO: Testing, remove
	// void render();

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

	GLuint output_texture_id;
//	GLuint VAO;	// for testing
//	GLuint VBO;	// for testing
	GLuint FBO = UINT_MAX;

	// settings
	const unsigned int SCR_WIDTH = 640;
	const unsigned int SCR_HEIGHT = 360;

	float lastX = (float)SCR_WIDTH / 2.0f;
	float lastY = (float)SCR_HEIGHT / 2.0f;

};
#endif // OPENGLHELPER_H