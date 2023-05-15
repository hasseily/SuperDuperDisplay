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
	lastX = (float)SCR_WIDTH / 2.0f;
	lastY = (float)SCR_HEIGHT / 2.0f;
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
	GLuint textureID;
	glGenTextures(1, &textureID);
	v_texture_ids.push_back(textureID);

	load_texture(data, width, height, nrComponents, textureID);
	return textureID;
}

// This method loads the texture data into the texture specified at textureID
void OpenGLHelper::load_texture(unsigned char* data, int width, int height, int nrComponents, GLuint textureID)
{
	GLenum glerr;
	GLenum format = GL_RGBA;
	if (nrComponents == 1)
		format = GL_RED;
	else if (nrComponents == 3)
		format = GL_RGB;
	else if (nrComponents == 4)
		format = GL_RGBA;

	glBindTexture(GL_TEXTURE_2D, textureID);
	// TODO: Check if glGetError() == GL_OUT_OF_MEMORY !!!
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL load_texture glBindTexture error: " << glerr << std::endl;
		return;
	}
	glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL load_texture glTexImage2D error: " << glerr << std::endl;
		return;
	}
	// NOTE: May need to generate mipmaps in case we want to allow zooming in-out
	// glGenerateMipmap(GL_TEXTURE_2D);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL load_texture glTexParameteri error: " << glerr << std::endl;
		return;
	}
	glBindTexture(GL_TEXTURE_2D, 0);
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL glBindTexture 0 error: " << glerr << std::endl;
		return;
	}
}

void OpenGLHelper::clear_textures()
{
	v_texture_ids.clear();
}

// TODO: testing, remove
/*
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
*/

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

void OpenGLHelper::rescale_framebuffer(uint32_t width, uint32_t height)
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

void OpenGLHelper::setup_sdhr_render(GLuint shaderProgramID)
{
	GLenum glerr;
	bind_framebuffer();
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL bind_framebuffer error: " << glerr << std::endl;
	}
	glUseProgram(shaderProgramID);	// TODO: Check if necessary
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL glUseProgram error: " << glerr << std::endl;
	}

	// Bind all 16 textures at once to GL_TEXTURE0... GL_TEXTURE16
	// All the meshes will be able to use them

	auto vti = this->v_texture_ids;
	for (unsigned int i = 0; i < vti.size(); i++) {
		glActiveTexture(GL_TEXTURE0 + i);
		glBindTexture(GL_TEXTURE_2D, vti.at(i));
	}
}

void OpenGLHelper::cleanup_sdhr_render()
{
	glActiveTexture(GL_TEXTURE0);
	glUseProgram(0);
	unbind_framebuffer();
}

// TODO: testing, remove
/*
void OpenGLHelper::render()
{
	glBindVertexArray(VAO);
	glDrawArrays(GL_TRIANGLES, 0, 3);
	glBindVertexArray(0);
}
*/

