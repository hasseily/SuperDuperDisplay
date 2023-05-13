#pragma once
#ifndef OPENGLHELPER_H
#define OPENGLHELPER_H
#include <stdint.h>
#include <stddef.h>
#include "camera.h"
#include "common.h"
#include <vector>

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
	unsigned int load_texture(unsigned char* data, int width, int height, int nrComponents);			// new texture
	void load_texture(unsigned char* data, int width, int height, int nrComponents, GLuint textureID);	// replace texture
	void clear_textures();
	void create_vertices();
	void add_shader(GLuint program, const char* shader_code, GLenum type);
	void create_shaders();
	void create_framebuffer();
	void bind_framebuffer();
	void unbind_framebuffer();
	void rescale_framebuffer(uint32_t width, uint32_t height);
	GLuint get_texture_id() { return texture_id; };
	void render();

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

	GLuint texture_id;
	GLuint VAO;
	GLuint VBO;
	GLuint FBO;
	GLuint RBO;
	GLuint shaderProgram;

	// settings
	const unsigned int SCR_WIDTH = 640;
	const unsigned int SCR_HEIGHT = 360;

	// camera
	Camera camera;
	float lastX = (float)SCR_WIDTH / 2.0f;
	float lastY = (float)SCR_HEIGHT / 2.0f;
	bool firstMouse = true;

	// timing
	float deltaTime = 0.0f;
	float lastFrame = 0.0f;

	const char* vertex_shader_code = R"*(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in int aTexIdx;       // texture index (max 16)

// the meshIndex is the index of the window for z-depth
uniform int meshIndex;

out vec2 vTexCoord;
out int vTexIdx;

void main()
{
    vTexCoord = aTexCoord;
    vTexIdx = aTexIdx;
    gl_Position = vec4(aPos, meshIndex / 256.0, 1.0); 
}
)*";

	const char* fragment_shader_code = R"*(
#version 330 core

uniform sampler2D tilesTexture[16];
in vec2 vTexCoord;
flat in int vTexIdx;    // the texture is the same for all pixels in the triangle

out vec4 fragColor;

void main()
{
    fragColor = texture(tilesTexture[vTexIdx], vTexCoord);
}
)*";


};
#endif // OPENGLHELPER_H