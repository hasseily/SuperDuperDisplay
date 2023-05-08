#pragma once
#include <stdint.h>
#include <stddef.h>
#include <GL\glew.h>

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
	void create_triangle();
	void add_shader(GLuint program, const char* shader_code, GLenum type);
	void create_shaders();
	void create_framebuffer();
	void bind_framebuffer();
	void unbind_framebuffer();
	void rescale_framebuffer(float width, float height);
	GLuint get_texture_id() { return texture_id; };
	void render();
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

	const GLint WIDTH = 640;
	const GLint HEIGHT = 360;

	GLuint VAO;
	GLuint VBO;
	GLuint FBO;
	GLuint RBO;
	GLuint texture_id;
	GLuint shader;

	const char* vertex_shader_code = R"*(
#version 110

layout (location = 0) in vec3 pos;

void main()
{
	gl_Position = vec4(0.9*pos.x, 0.9*pos.y, 0.5*pos.z, 1.0);
}
)*";

	const char* fragment_shader_code = R"*(
#version 110

void main()
{
	gl_FragColor = vec4(0.0, 1.0, 0.0, 1.0);
}
)*";


};

