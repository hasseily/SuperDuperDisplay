#include "A2WindowBeam.h"
#include "common.h"
#include <SDL_timer.h>
#include "A2VideoManager.h"
#include "SDHRManager.h"

A2WindowBeam::A2WindowBeam(A2VideoModeBeam_e _video_mode, const char* shaderVertexPath, const char* shaderFragmentPath)
{
	video_mode = _video_mode;
	shader = Shader();
	shader.build(shaderVertexPath, shaderFragmentPath);
}

A2WindowBeam::~A2WindowBeam()
{
	if (VRAMTEX != UINT_MAX)
		glDeleteTextures(1, &VRAMTEX);
	if (FBO != UINT_MAX)
	{
		glDeleteFramebuffers(1, &FBO);
		glDeleteTextures(1, &output_texture_id);
	}
	if (VAO != UINT_MAX)
	{
		glDeleteVertexArrays(1, &VAO);
		glDeleteBuffers(1, &VBO);
	}
}

void A2WindowBeam::SetShaderPrograms(const char* shaderVertexPath, const char* shaderFragmentPath)
{
	this->shader.build(shaderVertexPath, shaderFragmentPath);
}


const uint32_t A2WindowBeam::GetWidth()
{
	return screen_count.x;
}

const uint32_t A2WindowBeam::GetHeight()
{
	return screen_count.y;
}

void A2WindowBeam::SetBorder(uint32_t cycles_horizontal, uint32_t scanlines_vertical)
{
	border_width_cycles = cycles_horizontal;
	border_height_scanlines = scanlines_vertical;
	uint32_t cycles_h_with_border = 40 + (2 * border_width_cycles);
	// Legacy is 14 dots per cycle, SHR is 16 dots per cycle
	// Multiply border size by 4 and not 2 because height is doubled
	switch (video_mode) {
	case A2VIDEOBEAM_SHR:
		screen_count = uXY({ cycles_h_with_border * 16 , 400 + (4 * border_height_scanlines) });
		break;
	default:	//e
		screen_count = uXY({ cycles_h_with_border * 14, 384 + (4 * border_height_scanlines) });
		break;
	}
	UpdateVertexArray();
}


void A2WindowBeam::UpdateVertexArray()
{
	// Assign the vertex array.
	// The first 2 values are the relative XY, bound from -1 to 1.
	// The A2WindowBeam always covers the whole screen, so from -1 to 1 on both axes
	// The second pair of values is the actual pixel value on screen
	vertices.clear();
	vertices.push_back(A2BeamVertex({ glm::vec2(-1,  1), glm::ivec2(0, screen_count.y) }));	// top left
	vertices.push_back(A2BeamVertex({ glm::vec2(1, -1), glm::ivec2(screen_count.x, 0) }));	// bottom right
	vertices.push_back(A2BeamVertex({ glm::vec2(1,  1), glm::ivec2(screen_count.x, screen_count.y) }));	// top right
	vertices.push_back(A2BeamVertex({ glm::vec2(-1,  1), glm::ivec2(0, screen_count.y) }));	// top left
	vertices.push_back(A2BeamVertex({ glm::vec2(-1, -1), glm::ivec2(0, 0) }));	// bottom left
	vertices.push_back(A2BeamVertex({ glm::vec2(1, -1), glm::ivec2(screen_count.x, 0) }));	// bottom right
}


GLuint A2WindowBeam::GetOutputTextureId()
{
	return output_texture_id;
}

GLuint A2WindowBeam::Render(bool shouldUpdateDataInGPU)
{
	// std::cerr << "Rendering " << (int)video_mode << " - " << shouldUpdateDataInGPU << std::endl;
	// std::cerr << "border w " << border_width_cycles << " - h " << border_height_scanlines << std::endl;
	if (!shader.isReady)
		return UINT32_MAX;
	if (vertices.size() == 0)
		return UINT32_MAX;

	GLenum glerr;
	if (VRAMTEX == UINT_MAX)
		glGenTextures(1, &VRAMTEX);

	if (VAO == UINT_MAX)
	{
		glGenVertexArrays(1, &VAO);
		glGenBuffers(1, &VBO);
	}

	if (FBO == UINT_MAX)
	{
		glGenFramebuffers(1, &FBO);
		glGenTextures(1, &output_texture_id);
		glBindFramebuffer(GL_FRAMEBUFFER, FBO);
		glActiveTexture(_TEXUNIT_POSTPROCESS);
		glBindTexture(GL_TEXTURE_2D, output_texture_id);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, screen_count.x, screen_count.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, output_texture_id, 0);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, 0);
	}
	glBindFramebuffer(GL_FRAMEBUFFER, FBO);
	glClearColor(0.f, 0.f, 0.f, 0.f);
	glClear(GL_COLOR_BUFFER_BIT);
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL render A2VideoManager error: " << glerr << std::endl;
	}
	// std::cerr << "VRAMTEX " << VRAMTEX << " VAO " << VAO << " FBO " << FBO << std::endl;

	shader.use();
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL A2Video glUseProgram error: " << glerr << std::endl;
		return UINT32_MAX;
	}
	
	glBindVertexArray(VAO);

	// Always reload the vertices
	// because compatibility with GL-ES on the rPi
	{
		// load data into vertex buffers
		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(A2BeamVertex), &vertices[0], GL_STATIC_DRAW);

		// set the vertex attribute pointers
		// vertex relative Positions: position 0, size 2
		// (vec4 values x and y)
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(A2BeamVertex), (void*)0);
		// vertex pixel Positions: position 1, size 2
		// (vec4 values z and w)
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(A2BeamVertex), (void*)offsetof(A2BeamVertex, PixelPos));
		
		// And set the borders
		shader.setInt("hborder", (int)border_width_cycles);
		shader.setInt("vborder", (int)border_height_scanlines);
	}

	// Associate the texture VRAMTEX in TEXUNIT_DATABUFFER with the buffer
	// This is the apple 2's memory which is mapped to a "texture"
	// Always update that buffer in the GPU
	if (shouldUpdateDataInGPU)
	{
		uint32_t cycles_w_with_border = 40 + (2 * border_width_cycles);
		glActiveTexture(_TEXUNIT_DATABUFFER);
		glBindTexture(GL_TEXTURE_2D, VRAMTEX);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
		if (vramTextureExists)	// it exists, do a glTexSubImage2D() update
		{
			switch (video_mode) {
			case A2VIDEOBEAM_SHR:
				// Adjust the unpack alignment for textures with arbitrary widths
				glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, _COLORBYTESOFFSET + (cycles_w_with_border * 4), 200 + (2 * border_height_scanlines), GL_RED_INTEGER, GL_UNSIGNED_BYTE, A2VideoManager::GetInstance()->GetSHRVRAMReadPtr());
				glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
				break;
			default:
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, cycles_w_with_border, 192 + (2 * border_height_scanlines), GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, A2VideoManager::GetInstance()->GetLegacyVRAMReadPtr());
				break;
			}
		}
		else {	// texture doesn't exist, create it with glTexImage2D()
			switch (video_mode) {
			case A2VIDEOBEAM_SHR:
				// Adjust the unpack alignment for textures with arbitrary widths
				glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_R8UI, _COLORBYTESOFFSET + (cycles_w_with_border * 4), 200 + (2 * border_height_scanlines), 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, A2VideoManager::GetInstance()->GetSHRVRAMReadPtr());
				glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
				break;
			default:
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8UI, cycles_w_with_border, 192 + (2 * border_height_scanlines), 0, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, A2VideoManager::GetInstance()->GetLegacyVRAMReadPtr());
				break;
			}
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			vramTextureExists = true;
		}
		glActiveTexture(GL_TEXTURE0);
	}

	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "A2WindowBeam::Update error: " << glerr << std::endl;
	}

	shader.setInt("ticks", SDL_GetTicks());
	shader.setInt("specialModesMask", specialModesMask);
	shader.setInt("monitorColorType", monitorColorType);

	// point the uniform at the VRAM texture
	glActiveTexture(_TEXUNIT_DATABUFFER);
	glBindTexture(GL_TEXTURE_2D, VRAMTEX);
	shader.setInt("VRAMTEX", _TEXUNIT_DATABUFFER - GL_TEXTURE0);
	
	// And set all the modes textures that the shader will use
	// 2 font textures + lgr, hgr, dhgr
	shader.setInt("a2ModesTex0", _TEXUNIT_IMAGE_ASSETS_START + 0 - GL_TEXTURE0);	// D/TEXT font regular
	shader.setInt("a2ModesTex1", _TEXUNIT_IMAGE_ASSETS_START + 1 - GL_TEXTURE0);	// D/TEXT font alternate
	shader.setInt("a2ModesTex2", _TEXUNIT_IMAGE_ASSETS_START + 2 - GL_TEXTURE0);	// D/LGR
	shader.setInt("a2ModesTex3", _TEXUNIT_IMAGE_ASSETS_START + 3 - GL_TEXTURE0);	// HGR
	shader.setInt("a2ModesTex4", _TEXUNIT_IMAGE_ASSETS_START + 4 - GL_TEXTURE0);	// DHGR
	
	glDrawArrays(GL_TRIANGLES, 0, (GLsizei)this->vertices.size());
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE0);
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "A2WindowBeam render error: " << glerr << std::endl;
	}
	return output_texture_id;
}
