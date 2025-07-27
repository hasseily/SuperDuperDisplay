//
//  TimedTextManager.cpp
//  SuperDuperDisplay
//
//  Created by Henri Asseily on 17/07/2025.
//

#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <cstring>

#include "glad/glad.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "TimedTextManager.h"

void TimedTextManager::Initialize()
{
	useDefaultFont = true;
	CreateGLObjects();
	shader.Build(_SHADER_VERTEX_BASIC_TRANSFORM, "shaders/overlay_text.frag");
	texts.resize(100);
	idCounter = 0;
}

void TimedTextManager::Initialize(const std::string& ttfPath, float pixelHeight)
{
	this->Initialize();
	useDefaultFont = false;
	LoadFont(ttfPath, pixelHeight);
}

TimedTextManager::~TimedTextManager() {
	glDeleteTextures(1, &atlasTex);
	glDeleteBuffers(1, &vbo);
	glDeleteVertexArrays(1, &vao);
}

const size_t TimedTextManager::AddText(const std::string& text, int x, int y, uint64_t durationTicks,
							   float r, float g, float b, float a) {
	texts.push_back({ ++idCounter, text, x, y, SDL_GetTicks64() + durationTicks, r,g,b,a });
	return idCounter;
}

bool TimedTextManager::DeleteText(const size_t id) {
	auto it = std::find_if(texts.begin(), texts.end(),
						   [id](const TimedText& t) { return t.id == id; });
	if (it != texts.end()) {
		texts.erase(it);
		return true;
	}
	return false;
}

void TimedTextManager::UpdateAndRender(bool shouldFlipY) {
	if (texts.empty())
		return;

	if (!shader.isReady)
		return;

	GLenum glerr;

	// Check for High DPI scaling and adjust the font size accordingly
	auto window = SDL_GL_GetCurrentWindow();
	int winW, winH;
	SDL_GetWindowSize(window, &winW, &winH);
	int fbW, fbH;
	SDL_GL_GetDrawableSize(window, &fbW, &fbH);
	// DPI (pixel) multiplier in X and Y
	float dpiScaleX = static_cast<float>(fbW) / winW;
	float dpiScaleY = static_cast<float>(fbH) / winH;

	// disable depth, disable depth writes
	// necessary to have an overlay and not a black screen
	GLboolean depthMaskState = GL_FALSE;
	glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMaskState);
	GLboolean depthTestState = GL_FALSE;
	glGetBooleanv(GL_DEPTH_TEST, &depthTestState);

	if (depthMaskState == GL_TRUE)
		glDepthMask(GL_FALSE);
	if (depthTestState == GL_TRUE)
		glDisable(GL_DEPTH_TEST);

	shader.Use();
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "TimedTextManager glUseProgram error: " << glerr << std::endl;
		return;
	}

	glBindVertexArray(vao);

	if (useDefaultFont) {
		shader.SetUniform("uTex", _TEXUNIT_IMAGE_FONT_ROM_DEFAULT - GL_TEXTURE0);
	} else {
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, atlasTex);
		shader.SetUniform("uTex", GL_TEXTURE0 - GL_TEXTURE0);
	}

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// upload projection
	glm::mat4 proj = {
		2.0f / fbW, 0,            0, 0,
		0,           2.0f / fbH,  0, 0,
		0,           0,           -1, 0,
		-1,          -1,            0, 1
	};
	shader.SetUniform("uTransform", proj);

	// build & draw each string
	for (int i = int(texts.size()) - 1; i >= 0; --i) {
		auto &t = texts[i];
		if (t.ticksFinish < SDL_GetTicks64()) {
			texts.erase(texts.begin() + i);
			continue;
		}

		// set this string’s color
		shader.SetUniform("uColor", glm::vec4(t.r, t.g, t.b, t.a));

		// penX and penY are before HIGH DPI scaling
		float penX = float(t.x);
		float penY = float(t.y);

		std::vector<float> verts;
		verts.reserve(t.text.size() * 6 * 4);

		float x0, y0, x1, y1;	// position
		float s0, t0, s1, t1;	// tex uv

		if (useDefaultFont)
		{
			for (unsigned char c : t.text) {
				// Kind of map ascii to the apple charset
				// Not perfect, but that's what you get for reusing a free texture
				if (c < 0x20 || c > 0x7F) continue;
				if (c < 0x40) 		// punctuation
					c += 0x80;
				else if (c < 0x60)	// caps
					c += 0x40;
				else 				// lowecase
					c += 0x80;

				// calculate origin of char in texture
				// texture is 16x16 characters, each 14x16 pixels
				int texS = (c % 16) * 14;	// x origin in pixels
				int texT = (c / 16) * 16;	// y origin

				x0 = penX * dpiScaleX;
				y0 = penY * dpiScaleY;
				x1 = x0 + (use80ColDefaultFont ? 7 : 14) * dpiScaleX;
				y1 = y0 + 16 * dpiScaleY;

				s0 = texS / (float)(14 * 16);
				t0 = texT / (float)(16 * 16);
				s1 = s0 + 1.f/16.f;
				t1 = t0 + 1.f/16.f;

				if (shouldFlipY)
					std::swap(t0, t1);

				penX += (use80ColDefaultFont ? 7 : 14);

				// 2 tris (one quad) per character
				verts.insert(verts.end(), {
					x0,y0,s0,t0,
					x1,y0,s1,t0,
					x1,y1,s1,t1,

					x0,y0,s0,t0,
					x1,y1,s1,t1,
					x0,y1,s0,t1
				});
			}
		} else {	// custom font, using stb_truetype
			penY += ascent;
			for (unsigned char c : t.text) {
				if (c < 32 || c >= 128) continue;
				auto &bc = bakedChars[c - 32];

				x0 = (penX + bc.xoff) * dpiScaleX;
				y0 = penY * dpiScaleY;
				if (!shouldFlipY)
					y0 += bc.yoff * dpiScaleY;
				x1 = x0 + (bc.x1 - bc.x0) * dpiScaleX;
				y1 = y0 + (bc.y1 - bc.y0) * dpiScaleY;

				s0 = bc.x0 / 512.0f;
				t0 = bc.y0 / 512.0f;
				s1 = bc.x1 / 512.0f;
				t1 = bc.y1 / 512.0f;

				if (shouldFlipY)
					std::swap(t0, t1);

				penX += bc.xadvance;

				// 2 tris (one quad) per character
				verts.insert(verts.end(), {
					x0,y0,s0,t0,
					x1,y0,s1,t0,
					x1,y1,s1,t1,

					x0,y0,s0,t0,
					x1,y1,s1,t1,
					x0,y1,s0,t1
				});
			}
		}

		if (!verts.empty()) {
			glBindBuffer(GL_ARRAY_BUFFER, vbo);
			glBufferData(GL_ARRAY_BUFFER,
						 verts.size() * sizeof(float),
						 verts.data(),
						 GL_DYNAMIC_DRAW);
			GLsizei count = GLsizei(verts.size() / 4);
			glDrawArrays(GL_TRIANGLES, 0, count);
		}
	}

	// restore depth writes & test for next passes
	if (depthMaskState == GL_TRUE)
		glDepthMask(GL_TRUE);
	if (depthTestState == GL_TRUE)
		glEnable(GL_DEPTH_TEST);

	// 9) cleanup
	glBindVertexArray(0);
	shader.Release();

	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "TimedTextManager UpdateAndRender error: " << glerr << std::endl;
	}
}

/*
 PRIVATE METHODS
 */

void TimedTextManager::LoadFont(const std::string& path, float pixelHeight) {
	std::ifstream f(path, std::ios::binary | std::ios::ate);
	if (!f) throw std::runtime_error("Font file open failed");
	std::streamsize sz = f.tellg();
	f.seekg(0);
	fontBuffer.resize(sz);
	f.read((char*)fontBuffer.data(), sz);

	if (!stbtt_InitFont(&fontInfo, fontBuffer.data(), 0))
		throw std::runtime_error("Font init failed");

	unsigned char atlas[512 * 512] = {0};
	stbtt_BakeFontBitmap(fontBuffer.data(), 0, pixelHeight, atlas, 512, 512, 32, 96, bakedChars);
	float scale = stbtt_ScaleForPixelHeight(&fontInfo, pixelHeight);
	stbtt_GetFontVMetrics(&fontInfo, &ascent, nullptr, nullptr);
	ascent = static_cast<int>(ascent * scale);

	glGenTextures(1, &atlasTex);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, atlasTex);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, 512, 512, 0, GL_RED, GL_UNSIGNED_BYTE, atlas);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
}

void TimedTextManager::CreateGLObjects() {
	glGenVertexArrays(1, &vao);
	glGenBuffers(1, &vbo);
	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glEnableVertexAttribArray(0); // vec2 position
	glEnableVertexAttribArray(1); // vec2 texcoord
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
	glBindVertexArray(0);
}
