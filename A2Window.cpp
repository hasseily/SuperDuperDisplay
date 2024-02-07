#include "A2Window.h"
#include "common.h"
#include <SDL_timer.h>
#include "A2VideoManager.h"

// static unsigned int DBTEX = UINT_MAX;		// Data Buffer Texture (holds Apple 2 memory data)

void A2Window::Reset()
{
	SetEnabled(false);
	tile_dim = uXY({ 0,0 });
	tile_count = uXY({ 0,0 });
}

A2Window::~A2Window()
{
	if (DBTEX != UINT_MAX)
		glDeleteTextures(1, &DBTEX);

	if (VAO != UINT_MAX)
	{
		glDeleteVertexArrays(1, &VAO);
		glDeleteBuffers(1, &VBO);
	}
}

void A2Window::Define(A2VideoMode_e _video_mode, 
	uXY _screen_count,
	uXY _tile_dim, uXY _tile_count,
	uint8_t* _data, uint32_t _datasize,
	Shader* _shaderProgram)
{
	this->Reset();
	video_mode = _video_mode;
	screen_count = _screen_count;
	tile_dim = _tile_dim;
	tile_count = _tile_count;
	data = _data;
	datasize = _datasize;
	shaderProgram = _shaderProgram;
	bNeedsGPUDataUpdate = true;

	vertices[0].PixelPos = glm::vec2(0				, screen_count.y);	// top left
	vertices[1].PixelPos = glm::vec2(screen_count.x	, 0				);	// bottom right
	vertices[2].PixelPos = glm::vec2(screen_count.x	, screen_count.y);	// top right
	vertices[3].PixelPos = glm::vec2(0				, screen_count.y);	// top left
	vertices[4].PixelPos = glm::vec2(0				, 0				);	// bottom left
	vertices[5].PixelPos = glm::vec2(screen_count.x	, 0				);	// bottom right

	bNeedsGPUVertexUpdate = true;

}

void A2Window::Update()
{
	if (vertices.size() == 0)
		return;
	if (!(bNeedsGPUVertexUpdate || bNeedsGPUDataUpdate))
		return;				// doesn't need updating on the GPU

	GLenum glerr;
	if (DBTEX == UINT_MAX)
		glGenTextures(1, &DBTEX);

	if (VAO == UINT_MAX)
	{
		glGenVertexArrays(1, &VAO);
		glGenBuffers(1, &VBO);
	}

	glBindVertexArray(VAO);

	if (bNeedsGPUVertexUpdate)
	{
		// load data into vertex buffers
		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(A2Vertex), &vertices[0], GL_STATIC_DRAW);

		// set the vertex attribute pointers
		// vertex relative Positions: position 0, size 2
		// (vec4 values x and y)
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(A2Vertex), (void*)0);
		// vertex pixel Positions: position 1, size 2
		// (vec4 values z and w)
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(A2Vertex), (void*)offsetof(A2Vertex, PixelPos));
	}

	if (bNeedsGPUDataUpdate)
	{
		// Associate the texture DBTEX in GL_TEXTURE0+_SDHR_TBO_TEXUNIT with the buffer
		// This is the apple 2's memory which is mapped to a "texture"
		uint32_t _h = datasize / 1024;
		if ((_h * 1024) < datasize)
			_h++;
		glActiveTexture(GL_TEXTURE0 + _SDHR_TBO_TEXUNIT);
		glBindTexture(GL_TEXTURE_2D, DBTEX);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R8UI,
			1024,					// GL_R8UI is essentially an array of regular bytes (unbounded)
			_h,						// Split it by 1kB-sized rows
			0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, data);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	// reset the binding
	glBindVertexArray(0);

	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "A2Window::Update error: " << glerr << std::endl;
	}

	bNeedsGPUVertexUpdate = false;
	bNeedsGPUDataUpdate = false;
}

void A2Window::Render()
{
	if (!IsEnabled())
		return;
	if (shaderProgram == nullptr)
		return;
	GLenum glerr;
	shaderProgram->use();
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL A2Video glUseProgram error: " << glerr << std::endl;
		return;
	}

	glBindVertexArray(VAO);

	// Assign the textures
	switch (video_mode) {
		case A2VIDEO_TEXT1:
		case A2VIDEO_TEXT2:
		{
			shaderProgram->setFloat("isDouble", 0.f);
			if (A2VideoManager::GetInstance()->IsSoftSwitch(A2SS_ALTCHARSET))
			{
				shaderProgram->setFloat("hasFlashing", 0.f);
				shaderProgram->setInt("a2ModeTexture", _SDHR_START_TEXTURES + 1 - GL_TEXTURE0);
			} else {
				shaderProgram->setFloat("hasFlashing", 1.f);
				shaderProgram->setInt("a2ModeTexture", _SDHR_START_TEXTURES - GL_TEXTURE0);
			}
			break;
		}
		case A2VIDEO_DTEXT:
		{
			shaderProgram->setFloat("isDouble", 1.f);
			if (A2VideoManager::GetInstance()->IsSoftSwitch(A2SS_ALTCHARSET))
			{
				shaderProgram->setFloat("hasFlashing", 0.f);
				shaderProgram->setInt("a2ModeTexture", _SDHR_START_TEXTURES + 3 - GL_TEXTURE0);
			} else {
				shaderProgram->setFloat("hasFlashing", 1.f);
				shaderProgram->setInt("a2ModeTexture", _SDHR_START_TEXTURES + 2 - GL_TEXTURE0);
			}
			break;
		}
		case A2VIDEO_LGR1:
		case A2VIDEO_LGR2:
		{
			shaderProgram->setFloat("isDouble", 0.f);
			shaderProgram->setInt("a2ModeTexture", _SDHR_START_TEXTURES + 4 - GL_TEXTURE0);
			break;
		}
		case A2VIDEO_DLGR:
		{
			shaderProgram->setFloat("isDouble", 1.f);
			shaderProgram->setInt("a2ModeTexture", _SDHR_START_TEXTURES + 4 - GL_TEXTURE0);
			break;
		}
		case A2VIDEO_HGR1:
		case A2VIDEO_HGR2:
		{
			shaderProgram->setFloat("isDouble", 0.f);
			shaderProgram->setInt("a2ModeTexture", _SDHR_START_TEXTURES + 5 - GL_TEXTURE0);
			break;
		}
		case A2VIDEO_DHGR:
		{
			shaderProgram->setFloat("isDouble", 1.f);
			shaderProgram->setInt("a2ModeTexture", _SDHR_START_TEXTURES + 6 - GL_TEXTURE0);
			break;
		}
		case A2VIDEO_SHR:
		{
			// Shouldn't be here, SHR is done differently for now, in a CPU framebuffer
			break;
		}
		default:
			break;
	}

	if (A2VideoManager::GetInstance()->IsSoftSwitch(A2SS_MIXED))
		shaderProgram->setFloat("isMixed", 1.f);
	else
		shaderProgram->setFloat("isMixed", 0.f);

	// TODO: ADD SUPPORT FOR BLACK AND WHITE
	shaderProgram->setInt("ticks", SDL_GetTicks());
	shaderProgram->setVec2u("tileCount", tile_count.x, tile_count.y);
	shaderProgram->setVec2u("tileSize", tile_dim.x, tile_dim.y);

	auto cf = A2VideoManager::GetInstance()->color_foreground;
	shaderProgram->setVec4("colorTint", (cf & 0xFF) / 256.0, (cf >> 8 & 0xFF) / 256.0, (cf >> 16 & 0xFF) / 256.0, (cf >> 24 & 0xFF) / 256.0);

	// point the uniform at the tiles data texture (GL_TEXTURE0 + _SDHR_TBO_TEXUNIT)
	glActiveTexture(GL_TEXTURE0 + _SDHR_TBO_TEXUNIT);
	glBindTexture(GL_TEXTURE_2D, DBTEX);
	shaderProgram->setInt("DBTEX", _SDHR_TBO_TEXUNIT);
	// back to the output buffer to draw our scene
	glActiveTexture(GL_TEXTURE0);
	glDrawArrays(GL_TRIANGLES, 0, (GLsizei)this->vertices.size());
	glBindVertexArray(0);
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "MosaicMesh render error: " << glerr << std::endl;
	}
}


