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

VidHdWindowBeam::VidHdWindowBeam(VidHdMode_e _mode)
{
	vram_text = new uint8_t[_VIDHDMODES_TEXT_WIDTH*_VIDHDMODES_TEXT_HEIGHT];
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

void VidHdWindowBeam::WriteCharacter(uint8_t hpos, uint8_t vpos, uint8_t value)
{
	if (hpos >= _VIDHDMODES_TEXT_WIDTH)
		return;
	if (vpos >= _VIDHDMODES_TEXT_HEIGHT)
		return;
	vram_text[_VIDHDMODES_TEXT_WIDTH*vpos + hpos] = value;
}

uint8_t VidHdWindowBeam::ReadCharacter(uint8_t hpos, uint8_t vpos)
{
	if (hpos >= _VIDHDMODES_TEXT_WIDTH)
		return 0;
	if (vpos >= _VIDHDMODES_TEXT_HEIGHT)
		return 0;
	return vram_text[_VIDHDMODES_TEXT_WIDTH*vpos + hpos];
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
			mode_width = 40;
			mode_height = 24;
			fontTex = _TEXUNIT_IMAGE_ASSETS_START + 0 - GL_TEXTURE0;
			glyphSize = glm::uvec2(14,16);
			fontScale = glm::vec2(2.0,2.0);
			border = glm::vec2(400.0,156.0);
			break;
		case VIDHDMODE_TEXT_80X24:
			mode_width = 80;
			mode_height = 24;
			fontTex = _TEXUNIT_IMAGE_ASSETS_START + 0 - GL_TEXTURE0;
			glyphSize = glm::uvec2(14,16);
			fontScale = glm::vec2(1.0,2.0);
			border = glm::vec2(400.0,156.0);
			break;
		case VIDHDMODE_TEXT_80X45:
			mode_width = 80;
			mode_height = 45;
			fontTex = _TEXUNIT_IMAGE_ASSETS_START + 5 - GL_TEXTURE0;
			glyphSize = glm::uvec2(8,8);
			fontScale = glm::vec2(3.0,3.0);
			border = glm::vec2(0.0,0.0);
			break;
		case VIDHDMODE_TEXT_120X67:
			mode_width = 120;
			mode_height = 67;
			fontTex = _TEXUNIT_IMAGE_ASSETS_START + 5 - GL_TEXTURE0;
			glyphSize = glm::uvec2(8,8);
			fontScale = glm::vec2(2.0,2.0);
			border = glm::vec2(0.0,0.0);
			break;
		case VIDHDMODE_TEXT_240X135:
			mode_width = 240;
			mode_height = 135;
			fontTex = _TEXUNIT_IMAGE_ASSETS_START + 5 - GL_TEXTURE0;
			glyphSize = glm::uvec2(8,8);
			fontScale = glm::vec2(1.0,1.0);
			border = glm::vec2(0.0,0.0);
			break;
		default:
			mode_width = 0;
			mode_height = 0;
			fontTex = _TEXUNIT_IMAGE_ASSETS_START + 0 - GL_TEXTURE0;
			glyphSize = glm::uvec2(14,16);
			fontScale = glm::vec2(2.0,2.0);
			border = glm::vec2(400.0,156.0);
			break;
	}
	// Clear all the columns that are beyond the width of the mode
	if (mode_width < _VIDHDMODES_TEXT_WIDTH)
	{
		for (int j = 0; j < mode_height; ++j) {
			memset(vram_text+(_VIDHDMODES_TEXT_WIDTH*j + mode_width), 0, _VIDHDMODES_TEXT_WIDTH - mode_width);
		}
	}
	// Clear all the row that are beyond the height of the mode
	memset(vram_text+(mode_width*mode_height), 0, _VIDHDMODES_TEXT_WIDTH * (_VIDHDMODES_TEXT_HEIGHT - mode_height));
}

uint32_t VidHdWindowBeam::GetWidth() const
{
	return screen_count.x;
}

uint32_t VidHdWindowBeam::GetHeight() const
{
	return screen_count.y;
}


void VidHdWindowBeam::UpdateVertexArray()
{
	// Assign the vertex array.
	// The first 2 values are the relative XY, bound from -1 to 1.
	// The VidHdWindowBeam always covers the whole screen, so from -1 to 1 on both axes
	// The second pair of values is the actual pixel value on screen
	vertices.clear();
	vertices.push_back(VidHdBeamVertex({ glm::vec2(-1,  1), glm::ivec2(0, screen_count.y) }));	// top left
	vertices.push_back(VidHdBeamVertex({ glm::vec2(1, -1), glm::ivec2(screen_count.x, 0) }));	// bottom right
	vertices.push_back(VidHdBeamVertex({ glm::vec2(1,  1), glm::ivec2(screen_count.x, screen_count.y) }));	// top right
	vertices.push_back(VidHdBeamVertex({ glm::vec2(-1,  1), glm::ivec2(0, screen_count.y) }));	// top left
	vertices.push_back(VidHdBeamVertex({ glm::vec2(-1, -1), glm::ivec2(0, 0) }));	// bottom left
	vertices.push_back(VidHdBeamVertex({ glm::vec2(1, -1), glm::ivec2(screen_count.x, 0) }));	// bottom right
}


GLuint VidHdWindowBeam::GetOutputTextureId() const
{
	return output_texture_id;
}

GLuint VidHdWindowBeam::Render()
{
	// std::cerr << "Rendering vidhd mode " << (int)video_mode  << std::endl;
	if (video_mode == VIDHDMODE_NONE)
		return UINT32_MAX;
	if (!shader.isReady)
		return UINT32_MAX;
	if (vertices.size() == 0)
		return UINT32_MAX;

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

	if (FBO == UINT_MAX)
	{
		glGenFramebuffers(1, &FBO);
		glBindFramebuffer(GL_FRAMEBUFFER, FBO);
		glGenTextures(1, &output_texture_id);
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
		// glBindTexture(GL_TEXTURE_2D, 0);
	}
	glBindFramebuffer(GL_FRAMEBUFFER, FBO);
	glClearColor(0.f, 0.f, 0.f, 0.f);
	glClear(GL_COLOR_BUFFER_BIT);
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL render VidHdWindowBeam setup error: " << glerr << std::endl;
	}
	// std::cerr << "VRAMTEX " << VRAMTEX << " VAO " << VAO << " FBO " << FBO << std::endl;

	shader.use();
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL VidHdWindowBeam glUseProgram error: " << glerr << std::endl;
		return UINT32_MAX;
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
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 40, 192, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, vram_text);
	}
	else {	// texture doesn't exist, create it with glTexImage2D()
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8UI, 40, 192, 0, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, vram_text);
		bVramTextureExists = true;
	}

	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "VidHdWindowBeam::Render error: " << glerr << std::endl;
	}

	shader.setInt("ticks", SDL_GetTicks());
	if (bModeDidChange)
	{
		bModeDidChange = false;
		shader.setInt("VRAMTEX", _TEXUNIT_DATABUFFER_RGBA8UI - GL_TEXTURE0);
		shader.setInt("vidhdMode", video_mode);
		shader.setInt("xwidth", mode_width);
		shader.setInt("yheight", mode_height);
		shader.setInt("fontTex", fontTex);
		shader.setVec2u("glyphSize", glyphSize);
		shader.setVec2("fontScale", fontScale);
		shader.setVec2("border", border);
	}

	glDrawArrays(GL_TRIANGLES, 0, (GLsizei)this->vertices.size());
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "VidHdWindowBeam render error: " << glerr << std::endl;
	}
	return output_texture_id;
}

void VidHdWindowBeam::DisplayImGuiWindow(bool* p_open)
{
	// Begin the ImGui window. If the window is collapsed, end immediately.
	if (!ImGui::Begin("VidHdWindowBeam Control", p_open))
	{
		ImGui::End();
		return;
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
	}

	// Display the character at the given position
	uint8_t current_char = ReadCharacter(static_cast<uint8_t>(x_pos), static_cast<uint8_t>(y_pos));
	// Only display printable characters; otherwise show a dot.
	char display_char = (current_char >= (128+32) && current_char < 255) ? static_cast<char>(current_char-128) : '.';
	ImGui::Text("Character at (%d, %d): '%c' (ASCII: %d)", x_pos, y_pos, display_char, current_char);

	// --- Video Mode Selection ---
	static int current_mode_index = 0;
	const char* video_modes[] = { "TEXT 40x24", "TEXT 80x24", "TEXT 80x45", "TEXT 120x67", "TEXT 240x135", "None" };
	if (ImGui::Combo("Video Mode", &current_mode_index, video_modes, IM_ARRAYSIZE(video_modes)))
	{
		VidHdMode_e new_mode;
		switch (current_mode_index)
		{
			case 0: new_mode = VIDHDMODE_TEXT_40X24; break;
			case 1: new_mode = VIDHDMODE_TEXT_80X24; break;
			case 2: new_mode = VIDHDMODE_TEXT_80X45; break;
			case 3: new_mode = VIDHDMODE_TEXT_120X67; break;
			case 4: new_mode = VIDHDMODE_TEXT_240X135; break;
			default: new_mode = VIDHDMODE_NONE; break;
		}
		SetVideoMode(new_mode);
	}

	// --- Button to Clear the Entire VRAM Text Buffer ---
	if (ImGui::Button("Clear VRAM Text"))
	{
		memset(vram_text, 0, _VIDHDMODES_TEXT_WIDTH * _VIDHDMODES_TEXT_HEIGHT);
	}

	// --- Display a Preview of the VRAM Text Buffer ---
	// For debugging, show the first several rows of the buffer.
	ImGui::Separator();
	ImGui::Text("VRAM Text Preview:");
	// Limit the preview to a maximum of 24 rows (or less if mode_height is lower).
	int preview_rows = (mode_height < 24) ? mode_height : 24;
	for (int row = 0; row < preview_rows; ++row)
	{
		// Assume a maximum line width of 256 characters for display purposes.
		char line[256] = { 0 };
		int width = (mode_width < 256) ? mode_width : 256;
		for (int col = 0; col < width; ++col)
		{
			uint8_t ch = ReadCharacter(static_cast<uint8_t>(col), static_cast<uint8_t>(row));
			// Replace non-printable characters with a dot.
			ch -= 128;
			line[col] = (ch >= 32 && ch < 127) ? static_cast<char>(ch) : '.';
		}
		line[width] = '\0';
		ImGui::Text("%s", line);
	}

	ImGui::End();
}
