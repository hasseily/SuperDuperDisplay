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


uint32_t A2WindowBeam::GetWidth() const
{
	return screen_count.x;
}

uint32_t A2WindowBeam::GetHeight() const
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

void A2WindowBeam::SetQuadRelativeBounds(SDL_FRect bounds)
{
	quad = bounds;
	this->UpdateVertexArray();
}

void A2WindowBeam::UpdateVertexArray()
{
	// Assign the vertex array.
	// The first 2 values are the relative XY, bound from -1 to 1.
	// The second pair of values is the actual pixel value on screen
	vertices.clear();
	vertices.push_back(A2RenderVertex({ glm::vec2(quad.x, quad.y), glm::ivec2(0, screen_count.y) }));	// top left
	vertices.push_back(A2RenderVertex({ glm::vec2(quad.x + quad.w, quad.y + quad.h), glm::ivec2(screen_count.x, 0) }));	// bottom right
	vertices.push_back(A2RenderVertex({ glm::vec2(quad.x + quad.w, quad.y), glm::ivec2(screen_count.x, screen_count.y) }));	// top right
	vertices.push_back(A2RenderVertex({ glm::vec2(quad.x, quad.y), glm::ivec2(0, screen_count.y) }));	// top left
	vertices.push_back(A2RenderVertex({ glm::vec2(quad.x, quad.y + quad.h), glm::ivec2(0, 0) }));	// bottom left
	vertices.push_back(A2RenderVertex({ glm::vec2(quad.x + quad.w, quad.y + quad.h), glm::ivec2(screen_count.x, 0) }));	// bottom right
}

void A2WindowBeam::Render(uint64_t frame_idx)
{
	// std::cerr << "Rendering " << (int)video_mode << " - " << shouldUpdateDataInGPU << std::endl;
	// std::cerr << "border w " << border_width_cycles << " - h " << border_height_scanlines << std::endl;
	if (!shader.isReady)
		return;
	if (vertices.size() == 0)
		return;

	GLenum glerr;
	if (VRAMTEX == UINT_MAX)
	{
		glGenTextures(1, &VRAMTEX);
		if (video_mode == A2VIDEOBEAM_SHR)	// the SHR modes use a R8UI VRAM data buffer
			glActiveTexture(_TEXUNIT_DATABUFFER_R8UI);
		else
			glActiveTexture(_TEXUNIT_DATABUFFER_RGBA8UI);
		glBindTexture(GL_TEXTURE_2D, VRAMTEX);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glActiveTexture(GL_TEXTURE0);
	}
	if (video_mode == A2VIDEOBEAM_SHR)
	{
		if (PAL256TEX == UINT_MAX)
		{
			glGenTextures(1, &PAL256TEX);
			glActiveTexture(_TEXUNIT_PAL256BUFFER);
			glBindTexture(GL_TEXTURE_2D, PAL256TEX);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glActiveTexture(GL_TEXTURE0);
		}
	}

	if (VAO == UINT_MAX)
	{
		glGenVertexArrays(1, &VAO);
		glGenBuffers(1, &VBO);
	}

	shader.use();
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL A2Video glUseProgram error: " << glerr << std::endl;
		return;
	}
	
	glBindVertexArray(VAO);

	// Always reload the vertices
	// because compatibility with GL-ES on the rPi
	{
		// load data into vertex buffers
		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(A2RenderVertex), &vertices[0], GL_STATIC_DRAW);

		// set the vertex attribute pointers
		// vertex relative Positions: position 0, size 2
		// (vec4 values x and y)
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(A2RenderVertex), (void*)0);
		// vertex pixel Positions: position 1, size 2
		// (vec4 values z and w)
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(A2RenderVertex), (void*)offsetof(A2RenderVertex, PixelPos));
		
		// And set the borders
		shader.setUniform("hborder", (int)border_width_cycles);
		if (video_mode == A2VIDEOBEAM_SHR)
			shader.setUniform("vborder", (int)border_height_scanlines);
	}

	// Associate the texture VRAMTEX in TEXUNIT_DATABUFFER with the buffer
	// This is the apple 2's memory which is mapped to a "texture"
	// Always update that buffer in the GPU because you don't know which window was previously updated

	uint32_t cycles_w_with_border = 40 + (2 * border_width_cycles);

	if (video_mode == A2VIDEOBEAM_SHR)	// the SHR modes use a R8UI VRAM data buffer
		glActiveTexture(_TEXUNIT_DATABUFFER_R8UI);
	else
		glActiveTexture(_TEXUNIT_DATABUFFER_RGBA8UI);
	glBindTexture(GL_TEXTURE_2D, VRAMTEX);
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "A2WindowBeam::Render 5 error: " << glerr << std::endl;
	}
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	if (vramTextureExists)	// it exists, do a glTexSubImage2D() update
	{
		switch (video_mode) {
			case A2VIDEOBEAM_SHR:
				{
					// Adjust the unpack alignment for textures with arbitrary widths
					glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
					// Don't update the interlace part if unnecessary
					int _hasDSHR4 = (doubleSHR4 == DOUBLE_NONE ? 0 : 1);
					glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, _COLORBYTESOFFSET + (cycles_w_with_border * 4),
									(_A2VIDEO_SHR_SCANLINES + (2 * border_height_scanlines)) * (_hasDSHR4 + 1),
									GL_RED_INTEGER, GL_UNSIGNED_BYTE, A2VideoManager::GetInstance()->GetSHRVRAMReadPtr());
					if (((specialModesMask & A2_VSM_SHR4PAL256) != 0) || (overrideSHR4Mode == 2))
					{
						glActiveTexture(_TEXUNIT_PAL256BUFFER);
						glBindTexture(GL_TEXTURE_2D, PAL256TEX);
						glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, _A2VIDEO_SHR_BYTES_PER_LINE, _A2VIDEO_SHR_SCANLINES * (_hasDSHR4 + 1),
										GL_RED_INTEGER, GL_UNSIGNED_SHORT, (uint16_t*)(A2VideoManager::GetInstance()->GetPAL256VRAMReadPtr()));
						glActiveTexture(_TEXUNIT_DATABUFFER_R8UI);
					}
					glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
				}
				break;
			case A2VIDEOBEAM_FORCED_TEXT1:
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 40, 192, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, A2VideoManager::GetInstance()->GetTEXT1VRAMReadPtr());
				break;
			case A2VIDEOBEAM_FORCED_TEXT2:
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 40, 192, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, A2VideoManager::GetInstance()->GetTEXT2VRAMReadPtr());
				break;
			case A2VIDEOBEAM_FORCED_HGR1:
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 40, 192, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, A2VideoManager::GetInstance()->GetHGR1VRAMReadPtr());
				break;
			case A2VIDEOBEAM_FORCED_HGR2:
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 40, 192, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, A2VideoManager::GetInstance()->GetHGR2VRAMReadPtr());
				break;
			default:
				int _hasDoubleSize = (pagingMode == DOUBLE_NONE ? 0 : 1);
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, cycles_w_with_border, 
					(192 + (2 * border_height_scanlines)) * (_hasDoubleSize + 1),
						GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, A2VideoManager::GetInstance()->GetLegacyVRAMReadPtr());
				break;
		}
	}
	else {	// texture doesn't exist, create it with glTexImage2D()
		switch (video_mode) {
			case A2VIDEOBEAM_SHR:
				// Adjust the unpack alignment for textures with arbitrary widths
				glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
				// the size of the SHR texture is Scanlines+2xBorderHeight, doubled for interlace
				glTexImage2D(GL_TEXTURE_2D, 0, GL_R8UI, _COLORBYTESOFFSET + (cycles_w_with_border * 4),
							 (_A2VIDEO_SHR_SCANLINES + (2 * border_height_scanlines)) * _INTERLACE_MULTIPLIER, 0,
							 GL_RED_INTEGER, GL_UNSIGNED_BYTE, A2VideoManager::GetInstance()->GetSHRVRAMReadPtr());
				// Create the PAL256TEX texture
				glActiveTexture(_TEXUNIT_PAL256BUFFER);
				glBindTexture(GL_TEXTURE_2D, PAL256TEX);
				// The size of the PAL256 texture is 2 bytes per SHR byte. Add the interlacing and that's 4x scanlines
				glTexImage2D(GL_TEXTURE_2D, 0, GL_R16UI, _A2VIDEO_SHR_BYTES_PER_LINE,
							 2 * _A2VIDEO_SHR_SCANLINES * _INTERLACE_MULTIPLIER, 0,
							 GL_RED_INTEGER, GL_UNSIGNED_SHORT, (uint16_t*)(A2VideoManager::GetInstance()->GetPAL256VRAMReadPtr()));
				glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
				break;
			case A2VIDEOBEAM_FORCED_TEXT1:
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8UI, 40, 192, 0, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, A2VideoManager::GetInstance()->GetTEXT1VRAMReadPtr());
				break;
			case A2VIDEOBEAM_FORCED_TEXT2:
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8UI, 40, 192, 0, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, A2VideoManager::GetInstance()->GetTEXT2VRAMReadPtr());
				break;
			case A2VIDEOBEAM_FORCED_HGR1:
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8UI, 40, 192, 0, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, A2VideoManager::GetInstance()->GetHGR1VRAMReadPtr());
				break;
			case A2VIDEOBEAM_FORCED_HGR2:
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8UI, 40, 192, 0, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, A2VideoManager::GetInstance()->GetHGR2VRAMReadPtr());
				break;
			default:
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8UI,
					cycles_w_with_border, (192 + (2 * border_height_scanlines))*_INTERLACE_MULTIPLIER,
					0, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, A2VideoManager::GetInstance()->GetLegacyVRAMReadPtr());
				break;
		}
		vramTextureExists = true;
	}

	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "A2WindowBeam::Render error: " << glerr << std::endl;
	}

	shader.setUniform("ticks", SDL_GetTicks());
	shader.setUniform("frameIsOdd", (int)(frame_idx & 1));
	shader.setUniform("bIsMergedMode", bIsMergedMode);
	shader.setUniform("specialModesMask", specialModesMask);
	shader.setUniform("monitorColorType", monitorColorType);
	// The OFFSETTEX will only be used in case of merged SHR+legacy in the same frame
	shader.setUniform("OFFSETTEX", _TEXUNIT_MERGE_OFFSET - GL_TEXTURE0);

	// point the uniform at the VRAM texture
	if (video_mode == A2VIDEOBEAM_SHR)
		shader.setUniform("VRAMTEX", _TEXUNIT_DATABUFFER_R8UI - GL_TEXTURE0);
	else {
		shader.setUniform("VRAMTEX", _TEXUNIT_DATABUFFER_RGBA8UI - GL_TEXTURE0);
		shader.setUniform("bForceSHRWidth", bForceSHRWidth);
	}

	// And set all the modes textures that the shader will use
	// 2 font textures + lgr, hgr, dhgr
	// as well as any other unique mode data
	if (video_mode == A2VIDEOBEAM_SHR)
	{
		shader.setUniform("PAL256TEX", _TEXUNIT_PAL256BUFFER - GL_TEXTURE0);
		shader.setUniform("overrideSHR4Mode", overrideSHR4Mode);
		shader.setUniform("doubleSHR4Mode", doubleSHR4);
		int _hasDSHR4 = (doubleSHR4 == DOUBLE_NONE ? 0 : 1);
		int _dblshr4off = _hasDSHR4 * (_A2VIDEO_SHR_SCANLINES + (2 * border_height_scanlines));
		shader.setUniform("doubleSHR4YOffset", _dblshr4off);
		int _dblpaloff = _hasDSHR4 * _A2VIDEO_SHR_SCANLINES;
		shader.setUniform("doublePal256YOffset", _dblpaloff);
	}
	else 
	{
		shader.setUniform("pagingMode", pagingMode);
		int _hasPaging = (pagingMode == DOUBLE_NONE ? 0 : 1);
		shader.setUniform("pagingOffset", _hasPaging * (192 + (2 * (int)border_height_scanlines)));
		shader.setUniform("a2ModesTex0", _TEXUNIT_IMAGE_FONT_ROM_DEFAULT - GL_TEXTURE0);
		shader.setUniform("a2ModesTex1", _TEXUNIT_IMAGE_FONT_ROM_ALTERNATE - GL_TEXTURE0);
		shader.setUniform("a2ModesTex2", _TEXUNIT_IMAGE_COMPOSITE_LGR - GL_TEXTURE0);
		shader.setUniform("a2ModesTex3", _TEXUNIT_IMAGE_COMPOSITE_HGR - GL_TEXTURE0);
		shader.setUniform("a2ModesTex4", _TEXUNIT_IMAGE_COMPOSITE_DHGR - GL_TEXTURE0);
	}


	glDrawArrays(GL_TRIANGLES, 0, (GLsizei)this->vertices.size());
	glActiveTexture(GL_TEXTURE0);
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "A2WindowBeam render error: " << glerr << std::endl;
	}
	return;
}
