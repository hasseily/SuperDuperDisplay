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
	glGenTextures(1, &atlasTex);
	CreateGLObjects();
	shader.Build(_SHADER_VERTEX_BASIC_TRANSFORM, "shaders/overlay_text.frag");
	texts.resize(100);
	verts.reserve(240 * 6 * 4);	// 40 lines of 240 characters per line
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
	glGetIntegerv(GL_VIEWPORT, last_viewport);	// remember existing viewport to restore it later
	glViewport(0, 0, fbW, fbH);
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
		2.0f / fbW,		0,			0, 0,
		0,				2.0f / fbH,	0, 0,
		0,				0,		   -1, 0,
	   -1,			   -1,			0, 1
	};
	shader.SetUniform("uTransform", proj);

	float x0 = 0, y0 = 0, x1 = 0, y1 = 0;	// position
	float s0 = 0, t0 = 0, s1 = 0, t1 = 0;	// tex uv

	// precompute
	constexpr float invAtlasW = 1.0f / float(TT_ATLAS_W);
	constexpr float invAtlasH = 1.0f / float(TT_ATLAS_H);

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

		// Don't draw if out of bounds
		if (penY > winH || penY < 0) {
			std::cerr << "didn't draw " << t.text <<std::endl;
			continue;
		}

		if (useDefaultFont)
		{
			int fontXW = (use80ColDefaultFont ? 7 : 14);
			int fontYH = 16;

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
				// There's no ascent
				int texS = (c % 16) * 14;	// x origin in pixels
				int texT = (c / 16) * 16;	// y origin

				x0 = penX * dpiScaleX;
				y0 = penY * dpiScaleY;
				x1 = x0 + fontXW * dpiScaleX;
				y1 = y0 + fontYH * dpiScaleY;

				s0 = texS / (float)(14 * 16);
				t0 = texT / (float)(16 * 16);
				s1 = s0 + 1.f/16.f;
				t1 = t0 + 1.f/16.f;

				if (shouldFlipY)
					std::swap(t0, t1);

				penX += fontXW;

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
				if (c < TT_FIRST_CHAR) continue;
				auto &bc = packedChars[c - TT_FIRST_CHAR];

				x0 = (penX + bc.xoff);
				y0 = (penY + bc.yoff);
				x1 = x0 + (bc.x1 - bc.x0);
				y1 = y0 + (bc.y1 - bc.y0);

				// Convert to screen pixels
				x0 *= dpiScaleX;   x1 *= dpiScaleX;
				y0 *= dpiScaleY;   y1 *= dpiScaleY;

				if (shouldFlipY) {
					// flip around the top of the glyph
					float glyphH = y1 - y0;
					y0 = (2*penY)*dpiScaleY - y0 - glyphH;
					y1 = y0 + glyphH;
				}

				s0 = bc.x0 * invAtlasW;
				s1 = bc.x1 * invAtlasW;
				t0 = bc.y0 * invAtlasH;
				t1 = bc.y1 * invAtlasH;

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
		// Draw the string given the uniforms
		if (!verts.empty()) {
			glBindBuffer(GL_ARRAY_BUFFER, vbo);
			glBufferData(GL_ARRAY_BUFFER,
						 verts.size() * sizeof(float),
						 verts.data(),
						 GL_DYNAMIC_DRAW);
			GLsizei count = GLsizei(verts.size() / 4);
			glDrawArrays(GL_TRIANGLES, 0, count);
		}
		verts.clear();
	}

	// restore depth writes & test for next passes
	if (depthMaskState == GL_TRUE)
		glDepthMask(GL_TRUE);
	if (depthTestState == GL_TRUE)
		glEnable(GL_DEPTH_TEST);

	// cleanup
	glBindVertexArray(0);
	shader.Release();
	glViewport(last_viewport[0], last_viewport[1], (GLsizei)last_viewport[2], (GLsizei)last_viewport[3]);

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

	// initialize packer
	stbtt_pack_context pc;
	if (!stbtt_PackBegin(&pc, atlas.data(), TT_ATLAS_W, TT_ATLAS_H, /*stride=*/0, /*padding=*/1, /*alloc_context=*/nullptr))
		throw std::runtime_error("Failed to init packer");

	// pack exactly FIRST_CHAR…FIRST_CHAR+NUM_CHARS‑1
	stbtt_PackFontRange(&pc,
						fontBuffer.data(),    // TTF
						/*font_index=*/0,
						pixelHeight,
						TT_FIRST_CHAR,
						TT_NUM_CHARS,
						packedChars.data()
						);

	stbtt_PackEnd(&pc);

	float scale = stbtt_ScaleForPixelHeight(&fontInfo, pixelHeight);
	stbtt_GetFontVMetrics(&fontInfo, &ascent, nullptr, nullptr);
	ascent = static_cast<int>(ascent * scale);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, atlasTex);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, TT_ATLAS_W, TT_ATLAS_H, 0, GL_RED, GL_UNSIGNED_BYTE, atlas.data());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
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

// Decode the next UTF‑8 codepoint from `s` at byte index `pos`.
// Returns the number of bytes consumed, and writes the Unicode codepoint to `cp`.
size_t TimedTextManager::DecodeUTF8(const std::string& s, size_t pos, uint32_t& cp) {
	unsigned char c = static_cast<unsigned char>(s[pos]);
	if ((c & 0x80) == 0) {
		// 1‑byte ASCII
		cp = c;
		return 1;
	}
	else if ((c & 0xE0) == 0xC0 && pos + 1 < s.size()) {
		// 2‑byte sequence
		cp  = (c & 0x1F) << 6;
		cp |= (static_cast<unsigned char>(s[pos+1]) & 0x3F);
		return 2;
	}
	else if ((c & 0xF0) == 0xE0 && pos + 2 < s.size()) {
		// 3‑byte sequence
		cp  = (c & 0x0F) << 12;
		cp |= (static_cast<unsigned char>(s[pos+1]) & 0x3F) << 6;
		cp |= (static_cast<unsigned char>(s[pos+2]) & 0x3F);
		return 3;
	}
	else if ((c & 0xF8) == 0xF0 && pos + 3 < s.size()) {
		// 4‑byte sequence
		cp  = (c & 0x07) << 18;
		cp |= (static_cast<unsigned char>(s[pos+1]) & 0x3F) << 12;
		cp |= (static_cast<unsigned char>(s[pos+2]) & 0x3F) << 6;
		cp |= (static_cast<unsigned char>(s[pos+3]) & 0x3F);
		return 4;
	}
	// Invalid or truncated sequence: emit replacement char U+FFFD
	cp = 0xFFFD;
	return 1;
}

// Computes pixel width of a UTF-8 string at given pixel height, including kerning.
float TimedTextManager::MeasureTextWidth(const std::string& utf8, const float fontSize) {
	const float scale = stbtt_ScaleForPixelHeight(&fontInfo, fontSize);
	float width = 0.0f;
	int prev_cp = 0;

	for (size_t i = 0; i < utf8.size(); ) {
		uint32_t cp = 0;
		i += DecodeUTF8(utf8, i, cp);

		int advance, lsb;
		stbtt_GetCodepointHMetrics(&fontInfo, cp, &advance, &lsb);
		int kern = stbtt_GetCodepointKernAdvance(&fontInfo, prev_cp, cp);

		width += (advance + kern) * scale;
		prev_cp = cp;
	}

	return width;
}
