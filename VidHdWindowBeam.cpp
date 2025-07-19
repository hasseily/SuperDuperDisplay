//
//  VidHdModesManager.cpp
//  SuperDuperDisplay
//
//  Created by Henri Asseily on 27/01/2025.
//

#include "VidHdWindowBeam.h"
#include "common.h"
#include <SDL_timer.h>
#include "A2VideoManager.h"
#include "SDHRManager.h"
#include "MemoryManager.h"

VidHdWindowBeam::VidHdWindowBeam(VidHdMode_e _mode)
{
	vram_text = new uint32_t[_VIDHDMODES_TEXT_WIDTH*_VIDHDMODES_TEXT_HEIGHT];
	shader = Shader();
	shader.build(_SHADER_A2_VERTEX_DEFAULT, _SHADER_VIDHD_TEXT_FRAGMENT);
	this->SetVideoMode(_mode);
	this->UpdateVertexArray();
}

VidHdWindowBeam::~VidHdWindowBeam()
{
	if (vram_text!= nullptr)
	{
		delete[] vram_text;
		vram_text = nullptr;
	}
	if (VRAMTEX != UINT_MAX)
		glDeleteTextures(1, &VRAMTEX);
	if (VAO != UINT_MAX)
	{
		glDeleteVertexArrays(1, &VAO);
		glDeleteBuffers(1, &VBO);
	}
}

void VidHdWindowBeam::WriteCharacter(uint8_t hpos, uint8_t vpos, uint8_t value)
{
	if (hpos >= _VIDHDMODES_TEXT_WIDTH)
		return;
	if (vpos >= _VIDHDMODES_TEXT_HEIGHT)
		return;
	VidHdVramTextEntry textEntry;
	textEntry.character = value;
	textEntry.unused = 0;
	textEntry.color = MemoryManager::GetInstance()->switch_c022;
	textEntry.alpha = textAlpha;
	vram_text[_VIDHDMODES_TEXT_WIDTH*vpos + hpos] = *reinterpret_cast<uint32_t*>(&textEntry);
}

uint8_t VidHdWindowBeam::ReadCharacter(uint8_t hpos, uint8_t vpos)
{
	if (hpos >= _VIDHDMODES_TEXT_WIDTH)
		return 0;
	if (vpos >= _VIDHDMODES_TEXT_HEIGHT)
		return 0;
	VidHdVramTextEntry* textEntry = reinterpret_cast<VidHdVramTextEntry*>(vram_text + _VIDHDMODES_TEXT_WIDTH*vpos + hpos);
	return textEntry->character;
}

void VidHdWindowBeam::SetAlpha(uint8_t alpha)
{
	textAlpha = alpha;
}

void VidHdWindowBeam::SetVideoMode(VidHdMode_e mode)
{
	if (mode == video_mode)
		return;
	if (mode == VIDHDMODE_TOTAL_COUNT)
		video_mode = VIDHDMODE_NONE;
	else
		video_mode = mode;

	bModeDidChange = true;
	switch (video_mode) {
		case VIDHDMODE_TEXT_40X24:
			modeSize.x = 40;
			modeSize.y = 24;
			fontTex = _TEXUNIT_IMAGE_FONT_ROM_DEFAULT - GL_TEXTURE0;
			glyphSize = glm::uvec2(14,16);
			fontScale = glm::uvec2(2,2);
			// The base A2 font texture is twice the size it really is,
			// so we fix it by doubling the screen size we send to the shader
			screen_count = {_A2VIDEO_LEGACY_WIDTH*2,_A2VIDEO_LEGACY_HEIGHT*2};
			break;
		case VIDHDMODE_TEXT_80X24:
			modeSize.x = 80;
			modeSize.y = 24;
			fontTex = _TEXUNIT_IMAGE_FONT_ROM_DEFAULT - GL_TEXTURE0;
			glyphSize = glm::uvec2(14,16);
			fontScale = glm::uvec2(1,2);
			// The base A2 font texture is twice the size it really is,
			// so we fix it by doubling the screen size we send to the shader
			screen_count = {_A2VIDEO_LEGACY_WIDTH*2,_A2VIDEO_LEGACY_HEIGHT*2};
			break;
		case VIDHDMODE_TEXT_80X45:
			modeSize.x = 80;
			modeSize.y = 45;
			fontTex = _TEXUNIT_IMAGE_FONT_VIDHD_8X8 - GL_TEXTURE0;
			glyphSize = glm::uvec2(8,8);
			fontScale = glm::uvec2(3,3);
			screen_count = {_VIDHDMODES_PIXEL_WIDTH,_VIDHDMODES_PIXEL_HEIGHT};
			break;
		case VIDHDMODE_TEXT_120X67:
			modeSize.x = 120;
			modeSize.y = 67;
			fontTex = _TEXUNIT_IMAGE_FONT_VIDHD_8X8 - GL_TEXTURE0;
			glyphSize = glm::uvec2(8,8);
			fontScale = glm::uvec2(2,2);
			screen_count = {_VIDHDMODES_PIXEL_WIDTH,_VIDHDMODES_PIXEL_HEIGHT};
			break;
		case VIDHDMODE_TEXT_240X135:
			modeSize.x = 240;
			modeSize.y = 135;
			fontTex = _TEXUNIT_IMAGE_FONT_VIDHD_8X8 - GL_TEXTURE0;
			glyphSize = glm::uvec2(8,8);
			fontScale = glm::uvec2(1,1);
			screen_count = {_VIDHDMODES_PIXEL_WIDTH,_VIDHDMODES_PIXEL_HEIGHT};
			break;
		default:
			modeSize.x = 0;
			modeSize.y = 0;
			fontTex = _TEXUNIT_IMAGE_FONT_ROM_DEFAULT - GL_TEXTURE0;
			glyphSize = glm::uvec2(14,16);
			fontScale = glm::uvec2(2,2);
			screen_count = {0,0};
			break;
	}
	// Clear all the columns that are beyond the width of the mode
	if (modeSize.x < _VIDHDMODES_TEXT_WIDTH)
	{
		for (int j = 0; j < modeSize.y; ++j) {
			memset(vram_text+(_VIDHDMODES_TEXT_WIDTH*j + modeSize.x), 0, (_VIDHDMODES_TEXT_WIDTH - modeSize.x) * sizeof(uint32_t));
		}
	}
	// Clear all the rows that are beyond the height of the mode
	memset(vram_text+(modeSize.x*modeSize.y), 0, _VIDHDMODES_TEXT_WIDTH * (_VIDHDMODES_TEXT_HEIGHT - modeSize.y) * sizeof(uint32_t));
}

uint32_t VidHdWindowBeam::GetWidth() const
{
	return screen_count.x;
}

uint32_t VidHdWindowBeam::GetHeight() const
{
	return screen_count.y;
}

void VidHdWindowBeam::SetQuadRelativeBounds(SDL_FRect bounds)
{
	quad = bounds;
	this->UpdateVertexArray();
}

void VidHdWindowBeam::UpdateVertexArray()
{
	// Assign the vertex array.
	// The first 2 values are the relative XY, bound from -1 to 1.
	// The second pair of values is the actual pixel value on screen
	vertices.clear();
	vertices.push_back(VidHdBeamVertex({ glm::vec2(quad.x, quad.y), glm::ivec2(0, screen_count.y) }));	// top left
	vertices.push_back(VidHdBeamVertex({ glm::vec2(quad.x + quad.w, quad.y + quad.h), glm::ivec2(screen_count.x, 0) }));	// bottom right
	vertices.push_back(VidHdBeamVertex({ glm::vec2(quad.x + quad.w, quad.y), glm::ivec2(screen_count.x, screen_count.y) }));	// top right
	vertices.push_back(VidHdBeamVertex({ glm::vec2(quad.x, quad.y), glm::ivec2(0, screen_count.y) }));	// top left
	vertices.push_back(VidHdBeamVertex({ glm::vec2(quad.x, quad.y + quad.h), glm::ivec2(0, 0) }));	// bottom left
	vertices.push_back(VidHdBeamVertex({ glm::vec2(quad.x + quad.w, quad.y + quad.h), glm::ivec2(screen_count.x, 0) }));	// bottom right

}

void VidHdWindowBeam::Render()
{
	// std::cerr << "Rendering vidhd mode " << (int)video_mode  << std::endl;
	if (video_mode == VIDHDMODE_NONE)
		return;
	if (!shader.isReady)
		return;
	if (vertices.size() == 0)
		return;

	GLenum glerr;
	if (VRAMTEX == UINT_MAX)
	{
		glGenTextures(1, &VRAMTEX);
		glActiveTexture(_TEXUNIT_DATABUFFER_RGBA8UI);
		glBindTexture(GL_TEXTURE_2D, VRAMTEX);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glActiveTexture(GL_TEXTURE0);
	}

	if (VAO == UINT_MAX)
	{
		glGenVertexArrays(1, &VAO);
		glGenBuffers(1, &VBO);
	}

	shader.use();
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL VidHdWindowBeam glUseProgram error: " << glerr << std::endl;
		return;
	}

	glBindVertexArray(VAO);

	// Always reload the vertices
	// because compatibility with GL-ES on the rPi
	{
		// load data into vertex buffers
		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(VidHdBeamVertex), &vertices[0], GL_STATIC_DRAW);

		// set the vertex attribute pointers
		// vertex relative Positions: position 0, size 2
		// (vec4 values x and y)
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(VidHdBeamVertex), (void*)0);
		// vertex pixel Positions: position 1, size 2
		// (vec4 values z and w)
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(VidHdBeamVertex), (void*)offsetof(VidHdBeamVertex, PixelPos));
	}

	glActiveTexture(_TEXUNIT_DATABUFFER_RGBA8UI);
	glBindTexture(GL_TEXTURE_2D, VRAMTEX);
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "VidHdWindowBeam::Render 5 error: " << glerr << std::endl;
	}
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	if (bVramTextureExists)	// it exists, do a glTexSubImage2D() update
	{
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, _VIDHDMODES_TEXT_WIDTH, _VIDHDMODES_TEXT_HEIGHT, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, vram_text);
	}
	else {	// texture doesn't exist, create it with glTexImage2D()
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8UI, _VIDHDMODES_TEXT_WIDTH, _VIDHDMODES_TEXT_HEIGHT, 0, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, vram_text);
		bVramTextureExists = true;
	}

	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "VidHdWindowBeam::Render error: " << glerr << std::endl;
	}

	shader.setUniform("ticks", SDL_GetTicks());
	if (bModeDidChange)
	{
		bModeDidChange = false;
		shader.setUniform("VRAMTEX", _TEXUNIT_DATABUFFER_RGBA8UI - GL_TEXTURE0);
		shader.setUniform("vidhdMode", video_mode);
		shader.setUniform("modeSize", modeSize);
		shader.setUniform("fontTex", fontTex);
		shader.setUniform("glyphSize", glyphSize);
		shader.setUniform("fontScale", fontScale);
	}


	glDrawArrays(GL_TRIANGLES, 0, (GLsizei)this->vertices.size());
	glActiveTexture(GL_TEXTURE0);
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "VidHdWindowBeam render error: " << glerr << std::endl;
	}
	return;
}

void VidHdWindowBeam::DisplayImGuiWindow(bool* p_open)
{
	// Begin the ImGui window. If the window is collapsed, end immediately.
	if (!ImGui::Begin("VidHdWindowBeam Control", p_open))
	{
		ImGui::End();
		return;
	}

	if (ImGui::Button("Reload Shader"))
	{
		// shader.build(_SHADER_A2_VERTEX_DEFAULT, _SHADER_VIDHD_TEXT_FRAGMENT);
		std::string _ps = "/Users/henri/Documents/Repos/SuperDuperDisplay/";
		_ps.append(_SHADER_VIDHD_TEXT_FRAGMENT);
		shader.build(_SHADER_A2_VERTEX_DEFAULT, _ps.c_str());
		auto _vm = this->GetVideoMode();
		this->SetVideoMode(VidHdMode_e::VIDHDMODE_NONE);
		this->SetVideoMode(_vm);
		this->UpdateVertexArray();
	}

	// --- Input Controls to Test Writing into vram_text ---
	// Static variables retain values across frames.
	static int x_pos = 0;
	static int y_pos = 0;
	static int ascii_value = 32; // Default to a space character

	ImGui::InputInt("X Position", &x_pos);
	ImGui::InputInt("Y Position", &y_pos);
	ImGui::InputInt("ASCII Value", &ascii_value);

	if (ImGui::Button("Write Character"))
	{
		// Write the ASCII value (converted to uint8_t) at the specified position.
		WriteCharacter(static_cast<uint8_t>(x_pos),
					   static_cast<uint8_t>(y_pos),
					   static_cast<uint8_t>(ascii_value+128));
		A2VideoManager::GetInstance()->ForceBeamFullScreenRender();
	}
	if (ImGui::Button("Test String"))
	{
		for (int xx = 0; xx < modeSize.x; ++xx) {
			for (int yy = 0; yy < modeSize.y; ++yy) {
				WriteCharacter(static_cast<uint8_t>(xx),
							   static_cast<uint8_t>(yy),
							   static_cast<uint8_t>((xx % 0x80) + 0x80));
			}
		}
		A2VideoManager::GetInstance()->ForceBeamFullScreenRender();
	}

	// Display the character at the given position
	uint8_t current_char = ReadCharacter(static_cast<uint8_t>(x_pos), static_cast<uint8_t>(y_pos));
	// Only display printable characters; otherwise show a dot.
	char display_char = (current_char >= (128+32) && current_char < 255) ? static_cast<char>(current_char-128) : '.';
	ImGui::Text("Character at (%d, %d): '%c' (ASCII: %d)", x_pos, y_pos, display_char, current_char);

	// --- Button to Clear the Entire VRAM Text Buffer ---
	if (ImGui::Button("Clear VRAM Text"))
	{
		memset(vram_text, 0, _VIDHDMODES_TEXT_WIDTH * _VIDHDMODES_TEXT_HEIGHT * sizeof(uint32_t));
		A2VideoManager::GetInstance()->ForceBeamFullScreenRender();
	}

	ImGui::End();
}
