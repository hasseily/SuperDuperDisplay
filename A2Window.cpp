#include "A2Window.h"
#include "common.h"
#include <SDL_timer.h>
#include "A2VideoManager.h"
#include "SDHRManager.h"

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

void A2Window::Define(A2VideoMode_e _video_mode, Shader* _shaderProgram)
{
	this->Reset();
	video_mode = _video_mode;
	shaderProgram = _shaderProgram;

	switch (video_mode) {
	case A2VIDEO_TEXT:
		screen_count	= uXY({ (uint32_t)(_A2VIDEO_MIN_WIDTH) , (uint32_t)(_A2VIDEO_MIN_HEIGHT) });
		tile_dim		= uXY({ _A2_TEXT40_CHAR_WIDTH, _A2_TEXT40_CHAR_HEIGHT });
		tile_count		= uXY({ 40, 24 });
		datasize		= _A2VIDEO_TEXT_SIZE;
		break;
	case A2VIDEO_DTEXT:
		screen_count	= uXY({ (uint32_t)(_A2VIDEO_MIN_WIDTH) , (uint32_t)(_A2VIDEO_MIN_HEIGHT) });
		tile_dim		= uXY({ _A2_TEXT80_CHAR_WIDTH, _A2_TEXT80_CHAR_HEIGHT });
		tile_count		= uXY({ 80, 24 });
		datasize		= _A2VIDEO_TEXT_SIZE + _A2_MEMORY_SHADOW_END;
		break;
	case A2VIDEO_LGR:
		screen_count	= uXY({ (uint32_t)(_A2VIDEO_MIN_WIDTH), (uint32_t)(_A2VIDEO_MIN_HEIGHT) });
		tile_dim		= uXY({ _A2_TEXT40_CHAR_WIDTH, _A2_TEXT40_CHAR_HEIGHT });
		tile_count		= uXY({ 40, 24 });
		datasize		= _A2VIDEO_TEXT_SIZE;
		break;
	case A2VIDEO_DLGR:
		screen_count	= uXY({ (uint32_t)(_A2VIDEO_MIN_WIDTH), (uint32_t)(_A2VIDEO_MIN_HEIGHT) });
		tile_dim		= uXY({ _A2_TEXT80_CHAR_WIDTH, _A2_TEXT80_CHAR_HEIGHT });
		tile_count		= uXY({ 80, 24 });
		datasize		= _A2VIDEO_TEXT_SIZE + _A2_MEMORY_SHADOW_END;
		break;
	case A2VIDEO_HGR:
		screen_count	= uXY({ (uint32_t)(_A2VIDEO_MIN_WIDTH), (uint32_t)(_A2VIDEO_MIN_HEIGHT) });
		tile_dim		= uXY({ 14, 2 });
		tile_count		= uXY({ _A2VIDEO_MIN_WIDTH, _A2VIDEO_MIN_HEIGHT });		// 192 lines
		datasize		= _A2VIDEO_HGR_SIZE;
		break;
	case A2VIDEO_DHGR:
		screen_count	= uXY({ (uint32_t)(_A2VIDEO_MIN_WIDTH), (uint32_t)(_A2VIDEO_MIN_HEIGHT) });
		tile_dim		= uXY({ 14, 2 });
		tile_count		= uXY({ _A2VIDEO_MIN_WIDTH, _A2VIDEO_MIN_HEIGHT });		// 192 lines
		datasize		= (2 * _A2VIDEO_HGR_SIZE) + _A2_MEMORY_SHADOW_END;
		break;
	case A2VIDEO_SHR:
		screen_count	= uXY({ (uint32_t)(_A2VIDEO_SHR_WIDTH), (uint32_t)(_A2VIDEO_SHR_HEIGHT) });
		tile_dim		= uXY({ 4, 2 });	// each byte is 4 pixels wide. If in 320 mode, each pixel is duplicated
		tile_count		= uXY({ _A2VIDEO_SHR_WIDTH, _A2VIDEO_SHR_HEIGHT });
		datasize		= _A2VIDEO_SHR_SIZE;
		break;
	default:
		break;

	}
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
		bNeedsGPUVertexUpdate = false;

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

	// Careful: it's only page 2 if 80STORE is off
	bool isPage2 = false;
	if (A2VideoManager::GetInstance()->IsSoftSwitch(A2SS_PAGE2)
		&& !A2VideoManager::GetInstance()->IsSoftSwitch(A2SS_80STORE))
		isPage2 = true;

	uint8_t* data = SDHRManager::GetInstance()->GetApple2MemPtr();
	switch (video_mode) {
	case A2VIDEO_TEXT:
		data += (isPage2 ? _A2VIDEO_TEXT2_START : _A2VIDEO_TEXT1_START);
		break;
	case A2VIDEO_DTEXT:
		data += (isPage2 ? _A2VIDEO_TEXT2_START : _A2VIDEO_TEXT1_START);
		break;
	case A2VIDEO_LGR:
		data += (isPage2 ? _A2VIDEO_TEXT2_START : _A2VIDEO_TEXT1_START);
		break;
	case A2VIDEO_DLGR:	// TODO: Does DLGR have a page 2?
		data += (isPage2 ? _A2VIDEO_TEXT2_START : _A2VIDEO_TEXT1_START);
		break;
	case A2VIDEO_HGR:
		data += (isPage2 ? _A2VIDEO_HGR2_START : _A2VIDEO_HGR1_START);
		break;
	case A2VIDEO_DHGR:
		data += (isPage2 ? _A2VIDEO_HGR2_START : _A2VIDEO_HGR1_START);
		break;
	case A2VIDEO_SHR:
		data += _A2_MEMORY_SHADOW_END + _A2VIDEO_SHR_START;
		break;
	default:
		break;

	}
	// Associate the texture DBTEX in GL_TEXTURE0+_SDHR_TBO_TEXUNIT with the buffer
	// This is the apple 2's memory which is mapped to a "texture"
	// Always update that buffer in the GPU
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


	// reset the binding
	glBindVertexArray(0);

	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "A2Window::Update error: " << glerr << std::endl;
	}
}

void A2Window::Render()
{
	if (!IsEnabled())
		return;
	if (shaderProgram == nullptr)
		return;

	// Always update the data before render
	Update();

	GLenum glerr;
	shaderProgram->use();
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL A2Video glUseProgram error: " << glerr << std::endl;
		return;
	}

	glBindVertexArray(VAO);

	// Assign the textures
	switch (video_mode) {
		case A2VIDEO_TEXT:
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
		case A2VIDEO_LGR:
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
		case A2VIDEO_HGR:
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
			// Nothing to do here, all the data is in the control bytes in memory
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
		std::cerr << "A2Window render error: " << glerr << std::endl;
	}
}


