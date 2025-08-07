#include "A2WindowRGB.h"
#include "common.h"
#include <SDL_timer.h>
#include "A2VideoManager.h"
#include "MemoryManager.h"
#include "SDHRManager.h"

A2WindowRGB::A2WindowRGB() {
	memStart = 0x0000;
	memAux = false;
	shader = Shader();
	screen_count = {_A2VIDEO_LEGACY_WIDTH, _A2VIDEO_LEGACY_HEIGHT};
	quad = { -1.f, 1.f, 2.f, -2.f };

	GLint glerr;
	glGenVertexArrays(1, &VAO);
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL A2WindowRGB glGenVertexArrays error: " << glerr << std::endl;
		return;
	}
	glGenBuffers(1, &VBO);
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL A2WindowRGB glGenBuffers error: " << glerr << std::endl;
		return;
	}

	glGenFramebuffers(1, &FBO);
	glGenTextures(1, &texture_id);
	glBindFramebuffer(GL_FRAMEBUFFER, FBO);
	glBindTexture(GL_TEXTURE_2D, texture_id);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, _A2VIDEO_LEGACY_WIDTH, _A2VIDEO_LEGACY_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_id, 0);

	GLenum _statusFBO = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (_statusFBO != GL_FRAMEBUFFER_COMPLETE)
	{
		std::cerr << "A2WindowRGB Framebuffer is not complete: " << _statusFBO << std::endl;
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	bImguiWindowIsOpen = true;
	doubleMode = false;
	this->UpdateVertexArray();
	this->SetVideoMode(A2VIDEORGB_TEXT);
	A2VideoManager::GetInstance()->ForceBeamFullScreenRender();
}

A2WindowRGB::~A2WindowRGB()
{
	glDeleteFramebuffers(1, &FBO);
	glDeleteTextures(1, &texture_id);
	glDeleteVertexArrays(1, &VAO);
	glDeleteBuffers(1, &VBO);
	bImguiWindowIsOpen = false;
}

void A2WindowRGB::SetVideoMode(A2VideoModeRGB_e _videoMode)
{
	if ((_videoMode == videoMode) && shader.isReady)
		return;
	videoMode = _videoMode;
	doubleMode = false;
	switch (videoMode) {
		case A2VIDEORGB_TEXT:
			shader.Build(_SHADER_A2_VERTEX_DEFAULT, _SHADER_RGB_TEXT_FRAGMENT);
			break;
		case A2VIDEORGB_DTEXT:
			shader.Build(_SHADER_A2_VERTEX_DEFAULT, _SHADER_RGB_TEXT_FRAGMENT);
			doubleMode = true;
			break;
		case A2VIDEORGB_LGR:
			shader.Build(_SHADER_A2_VERTEX_DEFAULT, _SHADER_RGB_LGR_FRAGMENT);
			break;
		case A2VIDEORGB_DLGR:
			shader.Build(_SHADER_A2_VERTEX_DEFAULT, _SHADER_RGB_LGR_FRAGMENT);
			doubleMode = true;
			break;
		case A2VIDEORGB_HGR:
			shader.Build(_SHADER_A2_VERTEX_DEFAULT, _SHADER_RGB_HGR_FRAGMENT);
			break;
		case A2VIDEORGB_DHGR:
			shader.Build(_SHADER_A2_VERTEX_DEFAULT, _SHADER_RGB_DHGR_FRAGMENT);
			doubleMode = true;
			break;
		default:
			shader.Build(_SHADER_A2_VERTEX_DEFAULT, _SHADER_RGB_TEXT_FRAGMENT);
			break;
	}
}

void A2WindowRGB::UpdateVertexArray()
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

void A2WindowRGB::Render()
{
	if (!bImguiWindowIsOpen)
		return;
	if (!shader.isReady)
		return;
	if (vertices.size() == 0)
		return;

	GLenum glerr;

	glBindFramebuffer(GL_FRAMEBUFFER, FBO);
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "A2WindowRGB::Render glBindFramebuffer error: " << glerr << std::endl;
	}

	glViewport(0, 0, _A2VIDEO_LEGACY_WIDTH, _A2VIDEO_LEGACY_HEIGHT);
	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT);

	shader.Use();
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL A2WindowRGB glUseProgram error: " << glerr << std::endl;
		return;
	}
	
	glBindVertexArray(VAO);
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "A2WindowRGB::Render glBindVertexArray error: " << glerr << std::endl;
	}

	// Always reload the vertices
	// because compatibility with GL-ES on the rPi

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

	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "A2WindowRGB::Render error: " << glerr << std::endl;
	}

	shader.SetUniform("hasFlashing", 0.f);
	shader.SetUniform("isMixed", 0.f);			// don't use
	shader.SetUniform("ticks", SDL_GetTicks());
	shader.SetUniform("memstart", memAux ? (int)(memStart + _A2_MEMORY_SHADOW_END) : (int)memStart);
	// point the uniform at the Apple 2 memory texture
	shader.SetUniform("APPLE2MEMORYTEX", _TEXUNIT_APPLE2MEMORY_R8UI - GL_TEXTURE0);
	switch (videoMode) {
		case A2VIDEORGB_TEXT:
			shader.SetUniform("a2ModeTexture", _TEXUNIT_IMAGE_FONT_ROM_DEFAULT - GL_TEXTURE0);
			shader.SetUniform("isDouble", 0.f);
			shader.SetUniform("tileSize", glm::uvec2(14,16));
			shader.SetUniform("colorTint", glm::vec4(1,1,1,1));
			break;
		case A2VIDEORGB_LGR:
			shader.SetUniform("a2ModeTexture", _TEXUNIT_IMAGE_COMPOSITE_LGR - GL_TEXTURE0);
			shader.SetUniform("isDouble", 0.f);
			shader.SetUniform("tileSize", glm::uvec2(14,16));
			break;
		case A2VIDEORGB_HGR:
			shader.SetUniform("a2ModeTexture", _TEXUNIT_IMAGE_COMPOSITE_HGR - GL_TEXTURE0);
			shader.SetUniform("isDouble", 0.f);
			shader.SetUniform("tileSize", glm::uvec2(14,2));
			break;
		case A2VIDEORGB_DTEXT:
			shader.SetUniform("a2ModeTexture", _TEXUNIT_IMAGE_FONT_ROM_DEFAULT - GL_TEXTURE0);
			shader.SetUniform("isDouble", 1.f);
			shader.SetUniform("tileSize", glm::uvec2(7,16));
			shader.SetUniform("colorTint", glm::vec4(1,1,1,1));
			break;
		case A2VIDEORGB_DLGR:
			shader.SetUniform("a2ModeTexture", _TEXUNIT_IMAGE_COMPOSITE_LGR - GL_TEXTURE0);
			shader.SetUniform("isDouble", 1.f);
			shader.SetUniform("tileSize", glm::uvec2(7,16));
			break;
		case A2VIDEORGB_DHGR:
			shader.SetUniform("a2ModeTexture", _TEXUNIT_IMAGE_COMPOSITE_DHGR - GL_TEXTURE0);
			shader.SetUniform("isDouble", 1.f);
			shader.SetUniform("tileSize", glm::uvec2(14,2));
			break;
		default:
			break;
	}
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "A2WindowRGB render shader error: " << glerr << std::endl;
	}

	glDrawArrays(GL_TRIANGLES, 0, (GLsizei)this->vertices.size());
	glBindVertexArray(0);
	glUseProgram(0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "A2WindowRGB render error: " << glerr << std::endl;
	}
	return;
}

void A2WindowRGB::DisplayImGuiWindow() {
	if (!bImguiWindowIsOpen)
		return;

	ImGui::SetNextWindowSizeConstraints(ImVec2(_A2VIDEO_LEGACY_WIDTH/2, _A2VIDEO_LEGACY_HEIGHT/2 + 50), ImVec2(FLT_MAX, FLT_MAX));
	// Make each window title unique with the texture_id
	// ImGui::PushID() doesn't work on ImGui::Begin()
	std::string _title = "RAM RGB Renderer - texId ";
	char tex_str[12];
	SDL_itoa(texture_id, tex_str, 10);
	_title += tex_str;
	ImGui::Begin(_title.c_str(), &bImguiWindowIsOpen);
	ImGui::PushItemWidth(100);
	const char* videoType[] = { "TEXT", "LGR", "HGR", "DTEXT", "DLGR", "DHGR" };
	int _currMode = static_cast<int>(videoMode);
	if (ImGui::Combo("Renderer", &_currMode, videoType, IM_ARRAYSIZE(videoType)))
	{
		this->SetVideoMode(static_cast<A2VideoModeRGB_e>(_currMode));
		A2VideoManager::GetInstance()->ForceBeamFullScreenRender();
	}
	ImGui::SameLine(); ImGui::Spacing(); ImGui::SameLine();
	float dragSpeed = (ImGui::GetIO().KeyCtrl ? 16.f : 1.f );
	if (ImGui::DragInt("Memory Start", &memStart, dragSpeed, 0, 0xFFFF, "%04X"))
		A2VideoManager::GetInstance()->ForceBeamFullScreenRender();
	if (memStart > 0xFFFF)
		memStart = 0xFFFF;
	if (memStart < 0)
		memStart = 0;
	doubleMode = (_currMode > 2);
	if (doubleMode)
	{
		memAux = false;
	} else {
		ImGui::SameLine();
		if (ImGui::Checkbox("AUX Bank", &memAux))
			A2VideoManager::GetInstance()->ForceBeamFullScreenRender();
	}
	ImGui::Image(reinterpret_cast<void*>(texture_id),
//				 ImVec2(_A2VIDEO_LEGACY_WIDTH, _A2VIDEO_LEGACY_HEIGHT),
				 ImGui::GetContentRegionAvail(),
				 ImVec2(0, 0), ImVec2(1, 1));
	ImGui::End();
}

