#pragma once
#ifndef BASICQUAD_H
#define BASICQUAD_H

#include <vector>
#include "shader.h"
#include <unordered_map>
#include <variant>

/**
 * @brief Basic rendering class that gets quad dimensions and shader information
 * and renders the quad using the shader, in a previously specified framebuffer.
 * Hence glBindFramebuffer() and glFramebufferTexture2D() need to be called
 * before calling this class's Render() function.
 */

// Dictionary of shader attributes to pass in to Render()
// All the below ShaderValue types are supported
using ShaderValue = std::variant<bool, int, float, glm::vec2*>;
using ShaderDictionary = std::unordered_map<std::string, ShaderValue>;

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
	void Render(uint64_t frame_idx, const ShaderDictionary& shaderDict = ShaderDictionary{});

	Shader* GetShader() { return &shader; };
	void SetShaderPrograms(const char* shaderVertexPath, const char* shaderFragmentPath);

	GLuint GetInputTextureUnit() const { return inputTextureUnit; }
	void SetInputTextureUnit(GLuint val) { inputTextureUnit = val; }

	std::vector<BasicQuadVertex> vertices;	// Vertices with XYRelative and XYPixels
	unsigned int VAO = UINT_MAX;			// Vertex Array Object (holds buffers that are vertex related)
	unsigned int VBO = UINT_MAX;			// Vertex Buffer Object (holds vertices)

private:
	Shader shader = Shader();				// Shader used
	uXY screen_count = {0,0};				// width,height in pixels of visible screen area of window
	GLint inputTextureUnit = GL_TEXTURE0;	// Texture unit to use as input

	SDL_FRect quad = { -1.f, 1.f, 2.f, -2.f };	// x, y, width, height

	void UpdateVertexArray();
};

#endif // BASICQUAD_H
