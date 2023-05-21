#include "OpenGLHelper.h"

#include <iostream>
#include <string>

#include "common.h"
#include "imgui.h"
#include "imgui_impl_opengl3.h"


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
	glDeleteFramebuffers(1, &FBO);
	glDeleteTextures(1, &output_texture_id);
}

//////////////////////////////////////////////////////////////////////////
// Methods
//////////////////////////////////////////////////////////////////////////

// This method loads the texture data into the texture specified at textureID
void OpenGLHelper::load_texture(unsigned char* data, int width, int height, int nrComponents, GLuint textureID)
{
	GLenum glerr;
	GLenum format = GL_RGBA;
	if (nrComponents == 1)
		format = GL_RED;
	else if (nrComponents == 3)
		format = GL_RGBA;		// TODO: Understand why it shouldn't be GL_RGB
	else if (nrComponents == 4)
		format = GL_RGBA;

	glBindTexture(GL_TEXTURE_2D, textureID);
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL load_texture glBindTexture error: " << glerr << std::endl;
	}
	glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL load_texture glTexImage2D error: " << glerr << std::endl;
	}
	// NOTE: May need to generate mipmaps in case we want to allow zooming in-out
	// But then we need to change the GL_TEXTURE_MIN_FILTER to GL_XXX_MIPMAP_XXX
	glGenerateMipmap(GL_TEXTURE_2D);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);	// Note: Could also use GL_LINEAR, need to test
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL load_texture glTexParameteri error: " << glerr << std::endl;
	}
}

unsigned int OpenGLHelper::get_texture_id_at_slot(uint8_t slot)
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
	if (FBO != UINT_MAX)
		return;
	glGenFramebuffers(1, &FBO);
	glBindFramebuffer(GL_FRAMEBUFFER, FBO);
	glViewport(0, 0, _SDHR_WIDTH, _SDHR_HEIGHT);

	// bind the output texture
	// every time the framebuffer is bound, the output texture will be already bound
	glGenTextures(1, &output_texture_id);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, output_texture_id);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, _SDHR_WIDTH, _SDHR_HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, output_texture_id, 0);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		std::cerr << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!\n";
}

void OpenGLHelper::bind_framebuffer()
{
	if (FBO == UINT_MAX)
		create_framebuffer();
	glBindFramebuffer(GL_FRAMEBUFFER, FBO);
}

void OpenGLHelper::unbind_framebuffer()
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void OpenGLHelper::rescale_framebuffer(uint32_t width, uint32_t height)
{
	GLenum glerr;
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, output_texture_id);
	glViewport(0, 0, width, height);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL 5 error: " << glerr << std::endl;
	}
	glBindTexture(GL_TEXTURE_2D, 0);
}

void OpenGLHelper::setup_sdhr_render()
{
	GLenum glerr;
	bind_framebuffer();
	glClearColor(0.f, 0.f, 0.f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
}

void OpenGLHelper::cleanup_sdhr_render()
{
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

