#include "OpenGLHelper.h"

#include <iostream>
#include <string>

#include "imgui.h"
#include "imgui_impl_opengl3.h"


// below because "The declaration of a static data member in its class definition is not a definition"
OpenGLHelper* OpenGLHelper::s_instance;

//////////////////////////////////////////////////////////////////////////
// Basic singleton methods
//////////////////////////////////////////////////////////////////////////

void OpenGLHelper::Initialize()
{
	camera = Camera(glm::vec3(0.0f, 0.0f, 3.0f));
	lastX = (float)SCR_WIDTH / 2.0;
	lastY = (float)SCR_HEIGHT / 2.0;
	firstMouse = true;
	deltaTime = 0.0f;
	lastFrame = 0.0f;
}

OpenGLHelper::~OpenGLHelper()
{
	glDeleteFramebuffers(1, &FBO);
	glDeleteTextures(1, &texture_id);
	glDeleteRenderbuffers(1, &RBO);
}

//////////////////////////////////////////////////////////////////////////
// Methods
//////////////////////////////////////////////////////////////////////////

unsigned int OpenGLHelper::load_texture(unsigned char* data, int width, int height, int nrComponents)
{
	if (v_texture_ids.size() >= _SDHR_MAX_TEXTURES)
	{
		std::cerr << "ERROR: Already at max textures! Cannot create new texture" << '\n';
		return UINT_MAX;
	}
	unsigned int textureID;
	glGenTextures(1, &textureID);
	v_texture_ids.push_back(textureID);

	load_texture(data, width, height, nrComponents, textureID);
	return textureID;
}

// This method loads the texture data into the texture specified at textureID
void OpenGLHelper::load_texture(unsigned char* data, int width, int height, int nrComponents, GLuint textureID)
{
	GLenum format = GL_RGBA;
	if (nrComponents == 1)
		format = GL_RED;
	else if (nrComponents == 3)
		format = GL_RGB;
	else if (nrComponents == 4)
		format = GL_RGBA;

	glBindTexture(GL_TEXTURE_2D, textureID);
	// TODO: Check if glGetError() == GL_OUT_OF_MEMORY !!!
	glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);

	// NOTE: May need to generate mipmaps in case we want to allow zooming in-out
	// glGenerateMipmap(GL_TEXTURE_2D);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_2D, 0);
}

void OpenGLHelper::create_vertices()
{
	// TODO: Create vertices from the window command
	// Assign the texture id to each vertex as well

	float quadVertices[] = { // vertex attributes for a quad
		// positions   // texCoords
		-0.4f,  0.4f,  0.0f, 0.4f,
		-0.4f, -0.4f,  0.0f, 0.0f,
		 0.4f, -0.4f,  0.4f, 0.0f,

		-0.4f,  0.4f,  0.0f, 0.4f,
		 0.4f, -0.4f,  0.4f, 0.0f,
		 0.4f,  0.4f,  0.4f, 0.4f
	};

	glGenVertexArrays(1, &VAO);
	glBindVertexArray(VAO);

	glGenBuffers(1, &VBO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

}

void OpenGLHelper::add_shader(GLuint program, const char* shader_code, GLenum type)
{
	GLuint current_shader = glCreateShader(type);

	const GLchar* code[1];
	code[0] = shader_code;

	GLint code_length[1];
	code_length[0] = strlen(shader_code);

	glShaderSource(current_shader, 1, code, code_length);
	glCompileShader(current_shader);

	GLint result = 0;
	GLchar log[1024] = { 0 };

	glGetShaderiv(current_shader, GL_COMPILE_STATUS, &result);
	if (!result) {
		glGetShaderInfoLog(current_shader, sizeof(log), NULL, log);
		std::cerr << "Error compiling " << type << " shader: " << log << "\n";
		return;
	}

	glAttachShader(program, current_shader);
}

void OpenGLHelper::create_shaders()
{
	shaderProgram = glCreateProgram();
	if (!shaderProgram) {
		std::cerr << "Error creating shader program!\n";
		exit(1);
	}

	// TODO: Use file-based shader code!
	add_shader(shaderProgram, vertex_shader_code, GL_VERTEX_SHADER);
	add_shader(shaderProgram, fragment_shader_code, GL_FRAGMENT_SHADER);

	GLint result = 0;
	GLchar log[1024] = { 0 };

	glLinkProgram(shaderProgram);
	glGetProgramiv(shaderProgram, GL_LINK_STATUS, &result);
	if (!result) {
		glGetProgramInfoLog(shaderProgram, sizeof(log), NULL, log);
		std::cerr << "Error linking program:\n" << log << '\n';
		return;
	}

	glValidateProgram(shaderProgram);
	glGetProgramiv(shaderProgram, GL_VALIDATE_STATUS, &result);
	if (!result) {
		glGetProgramInfoLog(shaderProgram, sizeof(log), NULL, log);
		std::cerr << "Error validating program:\n" << log << '\n';
		return;
	}
}

void OpenGLHelper::create_framebuffer()
{
	glGenFramebuffers(1, &FBO);
	glBindFramebuffer(GL_FRAMEBUFFER, FBO);

	glGenTextures(1, &texture_id);
	glBindTexture(GL_TEXTURE_2D, texture_id);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, SCR_WIDTH, SCR_WIDTH, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_id, 0);

	glGenRenderbuffers(1, &RBO);
	glBindRenderbuffer(GL_RENDERBUFFER, RBO);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, SCR_WIDTH, SCR_WIDTH);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, RBO);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		std::cerr << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!\n";

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glBindRenderbuffer(GL_RENDERBUFFER, 0);
}

void OpenGLHelper::bind_framebuffer()
{
	glBindFramebuffer(GL_FRAMEBUFFER, FBO);
}

void OpenGLHelper::unbind_framebuffer()
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void OpenGLHelper::rescale_framebuffer(float width, float height)
{
	glBindTexture(GL_TEXTURE_2D, texture_id);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_id, 0);

	glBindRenderbuffer(GL_RENDERBUFFER, RBO);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, RBO);
}

void OpenGLHelper::render()
{
	// TODO: Loop through all the meshes and draw them
	bind_framebuffer();
	glUseProgram(shaderProgram);
	glBindVertexArray(VAO);
	glDrawArrays(GL_TRIANGLES, 0, 3);
	glBindVertexArray(0);
	glUseProgram(0);
	unbind_framebuffer();
}

