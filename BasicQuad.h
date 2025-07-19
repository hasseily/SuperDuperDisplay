#pragma once
#ifndef BASICQUAD_H
#define BASICQUAD_H

#include <vector>
#include "shader.h"
#include <map>
#include <variant>

/**
 * @brief Basic rendering class that gets quad dimensions and shader information
 * and renders the quad using the shader, in a previously specified framebuffer.
 * Hence glBindFramebuffer() and glFramebufferTexture2D() need to be called
 * before calling this class's Render() function.
 *
 * NOTE: any shader used is expected to have ticks, frameIsOdd and TEXIN as uniforms
 *
 * To set custom uniforms for the shader, before calling Render() you MUST call use() on the shader.
 * The older OpenGL (before 4.1) require it:
 *    auto _s = myBasicQuad->shader;
 *    _s.use();
 *    _s.setUniform("NTSC_COMB_STR", p_f_ntscCombStrength);
 *    _s.setUniform("TEST", p_i_test);
 *       ...
 *    myBasicQuad->Render(current_frame_idx);
 */

struct BasicQuadVertex {
	glm::vec2 RelPos;		// Relative position of the vertex
	glm::vec2 PixelPos;		// Pixel position of the vertex in the Apple 2 screen
};

class BasicQuad
{
public:

	BasicQuad(const char* shaderVertexPath, const char* shaderFragmentPath);
	~BasicQuad();
	uint32_t GetWidth() const { return screen_count.x; };
	uint32_t GetHeight() const { return screen_count.y; }
	void SetScreenCount(uint32_t _x, uint32_t _y) { 
		screen_count.x = _x; 
		screen_count.y = _y; 
		this->UpdateVertexArray();
	}

	void SetQuadRelativeBounds(SDL_FRect bounds);
	SDL_FRect GetQuadRelativeBounds() const { return quad; };
	void Render(uint64_t frame_idx);

	Shader* GetShader() { return &shader; };
	void SetShaderPrograms(const char* shaderVertexPath, const char* shaderFragmentPath);

	GLuint GetInputTextureUnit() const { return inputTextureUnit; }
	void SetInputTextureUnit(GLuint val) { inputTextureUnit = val; }

	std::vector<BasicQuadVertex> vertices;	// Vertices with XYRelative and XYPixels
	unsigned int VAO = UINT_MAX;			// Vertex Array Object (holds buffers that are vertex related)
	unsigned int VBO = UINT_MAX;			// Vertex Buffer Object (holds vertices)
	Shader shader = Shader();				// Shader used

private:
	uXY screen_count = {0,0};				// width,height in pixels of visible screen area of window
	GLint inputTextureUnit = GL_TEXTURE0;	// Texture unit to use as input

	// Obligatory uniform IDs used in any shader for BasicQuad
	GLint u_ticks = -1;
	GLint u_frameIsOdd = -1;
	GLint u_TEXIN = 1;

	SDL_FRect quad = { -1.f, 1.f, 2.f, -2.f };	// x, y, width, height

	void UpdateVertexArray();
};

#endif // BASICQUAD_H
