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

size_t OpenGLHelper::get_texture_id_at_slot(uint8_t slot)
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

void OpenGLHelper::create_framebuffer(uint32_t width, uint32_t height)
{
	// regenerate the framebuffer if different size
	if ((FBO != UINT_MAX) && ((width != fb_width) || (height != fb_height)))
	{
		return rescale_framebuffer(width, height);
	}

	if (FBO != UINT_MAX)
		return;

	glGenFramebuffers(1, &FBO);
	glBindFramebuffer(GL_FRAMEBUFFER, FBO);
	glViewport(0, 0, fb_width, fb_height);

	// bind the output texture
	// every time the framebuffer is bound, the output texture will be already bound
	glGenTextures(1, &output_texture_id);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, output_texture_id);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, fb_width, fb_height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, output_texture_id, 0);
	bDidChangeResolution = true;
	callbackResolutionChange(fb_width, fb_height);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		std::cerr << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!\n";
}

void OpenGLHelper::bind_framebuffer()
{
	
	if (FBO == UINT_MAX)
		create_framebuffer(_SCREEN_DEFAULT_WIDTH, _SCREEN_DEFAULT_HEIGHT);
	glBindFramebuffer(GL_FRAMEBUFFER, FBO);
	GLenum glerr;
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL bind_framebuffer error: " << glerr << std::endl;
	}
}

void OpenGLHelper::unbind_framebuffer()
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void OpenGLHelper::rescale_framebuffer(uint32_t width, uint32_t height)
{
	if ((fb_width == width) && (fb_height == height))
		return;
	GLenum glerr;
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, output_texture_id);
	glViewport(0, 0, width, height);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL rescale_framebuffer error: " << glerr << std::endl;
	}
	glBindTexture(GL_TEXTURE_2D, 0);
	fb_width = width;
	fb_height = height;
	bDidChangeResolution = true;
	if (callbackResolutionChange)
		callbackResolutionChange(fb_width, fb_height);
}

void OpenGLHelper::setup_render()
{
	GLenum glerr;
	bind_framebuffer();
	glClearColor(0.f, 0.f, 0.f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL setup_render error: " << glerr << std::endl;
	}

	// Check if we need to resize
	if ((fb_width_requested != UINT32_MAX) && (fb_height_requested != UINT32_MAX))
	{
		if ((fb_width_requested != fb_width) || (fb_height_requested == fb_height))
		{
			rescale_framebuffer(fb_width_requested, fb_height_requested);
			fb_width_requested = UINT32_MAX;
			fb_height_requested = UINT32_MAX;
		}
	}

// bUsePerspective toggle:
// Test using a prespective so we can zoom back and forth easily
// perspective uses (fov, aspect, near, far)
// Default FOV is 45 degrees

	if (bUsePerspective && (bDidChangeResolution || (!bIsUsingPerspective)))
	{
		camera.Position.x = fb_width / 2.f;
		camera.Position.y = fb_height / 2.f;
		camera.Position.z = glm::cos(glm::radians(ZOOM)) * fb_width;
		camera.Up = glm::vec3(0.f, 1.f, 0.f);
		camera.Yaw = -90.f;
		camera.Pitch = 0.f;
		mat_proj = glm::perspective<float>(glm::radians(this->camera.Zoom), (float)fb_width / fb_height, 0, 256);
		bIsUsingPerspective = bUsePerspective;
	}
	else if ((!bUsePerspective) && (bDidChangeResolution || bIsUsingPerspective))
	{
		camera.Position.x = fb_width / 2.f;
		camera.Position.y = fb_height / 2.f;
		camera.Position.z = _SDHR_MAX_WINDOWS;
		camera.Up = glm::vec3(0.f, 1.f, 0.f);
		camera.Yaw = -90.f;
		camera.Pitch = 0.f;
		mat_proj = glm::ortho<float>(
			-(float)fb_width / 2, (float)fb_width / 2,
			-(float)fb_height / 2, (float)fb_height / 2,
			0, 256);
		bIsUsingPerspective = bUsePerspective;
	}

	// And always update the projection when in perspective due to the zoom state
	if (bUsePerspective)
		mat_proj = glm::perspective<float>(glm::radians(this->camera.Zoom), (float)fb_width / fb_height, 0, 256);
}

void OpenGLHelper::cleanup_render()
{
	glUseProgram(0);
	unbind_framebuffer();
	bDidChangeResolution = false;
}

// METHODS THAT CAN BE CALLED FROM ANY THREAD
bool OpenGLHelper::request_framebuffer_resize(uint32_t width, uint32_t height)
{
	if ((width == fb_width) && (height == fb_height))
		return false;	// no change
	if ((width == UINT32_MAX) || (height == UINT32_MAX))
		return false;	// don't request ridiculous sizes that are used as "no resize requested"
	fb_width_requested = width;
	fb_height_requested = height;
	return true;
}

void OpenGLHelper::get_framebuffer_size(uint32_t* width, uint32_t* height)
{
	*width = fb_width;
	*height = fb_height;
}
