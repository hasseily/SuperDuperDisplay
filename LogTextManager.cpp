//
//  LogTextManager.cpp
//  SuperDuperDisplay
//
//  Created by Henri Asseily on 28/07/2025.
//

#include "LogTextManager.h"

// below because "The declaration of a static data member in its class definition is not a definition"
LogTextManager* LogTextManager::s_instance;

void LogTextManager::AddLog(const std::string &text, glm::vec4 tC) {
	auto remainingText = std::string(text);
	auto window = SDL_GL_GetCurrentWindow();
	int winW, winH;
	SDL_GetWindowSize(window, &winW, &winH);

	while (!text.empty()) {
		size_t pos = 0;
		std::string line;
		size_t splitPos = 0;

		// Build up 'line' word by word until adding the next word would exceed winW
		while (pos < remainingText.size()) {
			// Skip leading whitespace
			pos = remainingText.find_first_not_of(" \t\n", pos);
			if (pos == std::string::npos)
				break;

			// Find end of this word
			size_t next = remainingText.find_first_of(" \t\n", pos);
			std::string word = remainingText.substr(pos, next - pos);

			// Test adding this word to the line
			std::string test = line.empty() ? word : line + " " + word;
			if (MeasureTextWidth(test, LOGTEXT_FONTSIZE) > (winW-LOGTEXT_PADDING.x*2)) {
				if (line.empty()) {
					// First word is already too wide: force it through
					line = word;
					splitPos = (next == std::string::npos ? remainingText.size() : next);
				}
				break;
			}

			// Accept this word
			line = std::move(test);
			splitPos = (next == std::string::npos ? remainingText.size() : next);
			pos = next;
		}

		if (line.empty()) {
			// No more words to process
			break;
		}

		// Process the accumulated line
		texts.push_back({ ++idCounter, line, 0, 0, SDL_GetTicks64() + logDurationMS, tC.r, tC.g, tC.b, tC.a });

		// Remove processed text (and any whitespace) from remainingText
		remainingText.erase(0, splitPos);
	}
}

void LogTextManager::UpdateAndRender(bool shouldFlipY) {
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
		std::cerr << "LogTextManager glUseProgram error: " << glerr << std::endl;
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

	// Determine starting position (before HIGH DPI scaling)
	// Where "20" allows for the ImGUI menu height
	float yOffset = LOGTEXT_PADDING.y;
	if (logPosition == TTLogPosition_e::TOP_LEFT)
		yOffset = (winH - LOGTEXT_PADDING.y - LOGTEXT_FONTSIZE - 20);

	// prepare the dark overlay rectangle over which the
	// log text will appear, so that it's always properly readable
	float maxWidthX  = 0.f;
	float maxHeightY = 0.f;

	float x0 = 0, y0 = 0, x1 = 0, y1 = 0;	// position
	float s0 = 0, t0 = 0, s1 = 0, t1 = 0;	// tex uv

	// precompute
	constexpr float invAtlasW = 1.0f / float(TT_ATLAS_W);
	constexpr float invAtlasH = 1.0f / float(TT_ATLAS_H);

	// build & draw each string
	auto _ticks = SDL_GetTicks64();
	for (int i = int(texts.size()) - 1; i >= 0; --i) {
		auto &t = texts[i];
		if (t.ticksFinish < _ticks) {
			texts.erase(texts.begin() + i);
			continue;
		}
		// if the log duration changed, make sure nothing lasts longer than the new value
		if ((t.ticksFinish - _ticks) > logDurationMS)
			t.ticksFinish = _ticks + logDurationMS;

		// set this string’s color
		shader.SetUniform("uColor", glm::vec4(t.r, t.g, t.b, t.a));

		// penX and penY are before HIGH DPI scaling
		float penX = LOGTEXT_PADDING.x;
		float penY = yOffset;

		switch (logPosition) {
			case TTLogPosition_e::TOP_LEFT:
				yOffset -= (LOGTEXT_FONTSIZE + 2);
				break;
			default:
				yOffset += (LOGTEXT_FONTSIZE + 2);
				break;
		}

		// Don't draw if out of bounds
		if (yOffset > winH || yOffset < 0) {
			std::cerr << "didn't draw " << t.text <<std::endl;
			continue;
		}

		penY += ascent;
		size_t j = 0;
		while (j < t.text.size()) {
			uint32_t c;
			size_t bytes = DecodeUTF8(t.text, j, c);

			// `codepoint` is now the Unicode value of the next glyph.
			// e.g. pass it to stbtt_GetCodepointHMetrics(&font, codepoint, &advance, &lsb);

			j += bytes;

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

			maxWidthX = std::max(maxWidthX, x1);
			maxHeightY = std::max(maxHeightY, y1);

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

	if (!verts.empty()) {

		// Now add the log overlay
		// Increase the height to account for the ascents etc...
		x0 = 0; x1 = maxWidthX + (5 * dpiScaleX);
		y0 = 0; y1 = maxHeightY + (5 * dpiScaleY);
		s0 = -1; s1 = -1;	// this defines the log overlay in the shader
		t0 = -1; t1 = -1;
		if (logPosition == TTLogPosition_e::TOP_LEFT) {
			y0 = (fbH - 20 * dpiScaleY) - y1 + (LOGTEXT_FONTSIZE + 2) * dpiScaleY;
			y1 = (fbH - 20 * dpiScaleY);
		}
		verts.insert(verts.begin(), {
			x0,y0,s0,t0,
			x1,y0,s1,t0,
			x1,y1,s1,t1,

			x0,y0,s0,t0,
			x1,y1,s1,t1,
			x0,y1,s0,t1
		});

		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER,
					 verts.size() * sizeof(float),
					 verts.data(),
					 GL_DYNAMIC_DRAW);
		GLsizei count = GLsizei(verts.size() / 4);
		glDrawArrays(GL_TRIANGLES, 0, count);
	}

	// restore depth writes & test for next passes
	if (depthMaskState == GL_TRUE)
		glDepthMask(GL_TRUE);
	if (depthTestState == GL_TRUE)
		glEnable(GL_DEPTH_TEST);

	// cleanup
	glBindVertexArray(0);
	shader.Release();
	verts.clear();
	glViewport(last_viewport[0], last_viewport[1], (GLsizei)last_viewport[2], (GLsizei)last_viewport[3]);

	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "LogTextManager UpdateAndRender error: " << glerr << std::endl;
	}

}

nlohmann::json LogTextManager::SerializeState()
{
	nlohmann::json jsonState = {
		{"log_duration_ms", logDurationMS},
		{"log_position", (int)logPosition},
	};
	return jsonState;
}

void LogTextManager::DeserializeState(const nlohmann::json& jsonState)
{
	logDurationMS = jsonState.value("log_duration_ms", logDurationMS);
	logPosition = (TTLogPosition_e)jsonState.value("log_position", (int)logPosition);
}
