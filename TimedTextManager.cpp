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
	CreateShader();
	texts.resize(100);
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
	glDeleteProgram(shader);
}

void TimedTextManager::AddText(const std::string& text, int x, int y, int durationFrames) {
	// default white
	texts.push_back({ text, x, y, durationFrames, 1,1,1,1 });
}

void TimedTextManager::AddText(const std::string& text, int x, int y, int durationFrames,
							   float r, float g, float b, float a) {
	texts.push_back({ text, x, y, durationFrames, r,g,b,a });
}

void TimedTextManager::UpdateAndRender(int windowW, int windowH) {
	// 1) Update size & viewport
	winW = windowW;
	winH = windowH;
	glViewport(0, 0, winW, winH);

	// 2) disable depth, disable depth writes
	glDepthMask(GL_FALSE);
	glDisable(GL_DEPTH_TEST);

	// 3) use text shader + VAO
	glUseProgram(shader);
	glBindVertexArray(vao);

	// 4) bind atlas into unit 0 if using custom font & tell shader
	if (useDefaultFont) {
		glUniform1i(locTex, _TEXUNIT_IMAGE_FONT_ROM_DEFAULT - GL_TEXTURE0);
	} else {
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, atlasTex);
		glUniform1i(locTex, 0);
	}

	// 5) enable alpha blending
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// 6) upload projection
	float proj[16] = {
		2.0f / winW, 0,            0, 0,
		0,           2.0f / winH,  0, 0,
		0,           0,           -1, 0,
	   -1,          -1,            0, 1
	};
	glUniformMatrix4fv(locProj, 1, GL_FALSE, proj);
	//glUniformMatrix4fv(glGetUniformLocation(shader, "uProj"), 1, GL_FALSE, proj);

	// 7) build & draw each string
	for (int i = int(texts.size()) - 1; i >= 0; --i) {
		auto &t = texts[i];
		if (t.framesLeft-- <= 0) {
			texts.erase(texts.begin() + i);
			continue;
		}

		// set this string’s color
		glUniform4f(locColor, t.r, t.g, t.b, t.a);

		float penX = float(t.x);
		float penY = float(t.y);

		std::vector<float> verts;
		verts.reserve(t.text.size() * 6 * 4);

		float x0, y0, x1, y1;	// position
		float s0, t0, s1, t1;	// tex uv

		if (useDefaultFont)
		{
			for (unsigned char c : t.text) {
				if (c < 32 || c >= 128) continue;
				c += 128;	// high ASCII
				// calculate origin of char in texture
				// texture is 16x16 characters, each 14x16 pixels
				int texS = (c % 16) * 14;	// x origin in pixels
				int texT = (c / 16) * 16;	// y origin

				x0 = penX;
				y0 = penY;
				x1 = x0 + (use80ColDefaultFont ? 7 : 14);
				y1 = y0 + 16;

				s0 = texS / (float)(14 * 16);
				t0 = texT / (float)(16 * 16);
				s1 = s0 + 1.f/16.f;
				t1 = t0 + 1.f/16.f;

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

				x0 = penX + bc.xoff;
				y0 = penY + bc.yoff;
				x1 = x0 + (bc.x1 - bc.x0);
				y1 = y0 + (bc.y1 - bc.y0);

				s0 = bc.x0 / 512.0f;
				t0 = bc.y0 / 512.0f;
				s1 = bc.x1 / 512.0f;
				t1 = bc.y1 / 512.0f;

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

	// 8) restore depth writes & test for next passes
	glDepthMask(GL_TRUE);
//	glEnable(GL_DEPTH_TEST);

	// 9) cleanup
	glBindVertexArray(0);
	glUseProgram(0);}


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

void TimedTextManager::CreateShader() {
	const char* vsSrc = R"(#version 330 core
		layout(location = 0) in vec2 aPos;
		layout(location = 1) in vec2 aTex;
		uniform mat4 uProj;
		out vec2 vTex;
		void main() {
			gl_Position = uProj * vec4(aPos.xy, 0.0, 1.0);
			vTex = aTex;
		})";

	const char* fsSrc = R"(#version 330 core
		in vec2 vTex;
		uniform sampler2D uTex;
		uniform vec4 uColor;
		out vec4 FragColor;
		void main() {
			float a = texture(uTex, vTex).r;
			FragColor = vec4(uColor.rgb, uColor.a * a);
		})";

	GLuint vs = glCreateShader(GL_VERTEX_SHADER);
	GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(vs, 1, &vsSrc, nullptr);
	glShaderSource(fs, 1, &fsSrc, nullptr);
	glCompileShader(vs); CheckShader(vs, "vertex");
	glCompileShader(fs); CheckShader(fs, "fragment");

	shader = glCreateProgram();
	glAttachShader(shader, vs);
	glAttachShader(shader, fs);
	glLinkProgram(shader); CheckLink(shader);

	// cache uniform locations
	locProj  = glGetUniformLocation(shader, "uProj");
	locTex   = glGetUniformLocation(shader, "uTex");
	locColor = glGetUniformLocation(shader, "uColor");

	glDeleteShader(vs);
	glDeleteShader(fs);
}

void TimedTextManager::CheckShader(GLuint shader, const std::string& name) {
	GLint status = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if (!status) {
		char log[512];
		glGetShaderInfoLog(shader, 512, nullptr, log);
		std::cerr << "Shader compile error (" << name << "): " << log << "\n";
		throw std::runtime_error("Shader compile failed");
	}
}

void TimedTextManager::CheckLink(GLuint prog) {
	GLint status = 0;
	glGetProgramiv(prog, GL_LINK_STATUS, &status);
	if (!status) {
		char log[512];
		glGetProgramInfoLog(prog, 512, nullptr, log);
		std::cerr << "Shader link error: " << log << "\n";
		throw std::runtime_error("Shader link failed");
	}
}
