#pragma once
#ifndef A2WINDOW_H
#define A2WINDOW_H

#include <vector>
#include "shader.h"

#define _A2_TEXT40_CHAR_WIDTH 7
#define _A2_TEXT40_CHAR_HEIGHT 8

struct A2Vertex {
	glm::vec2 RelPos;		// Relative position of the vertex
	glm::vec2 PixelPos;		// Pixel position of the vertex in the Apple 2 screen
};

class A2Window
{
public:
	bool enabled;
	unsigned int DBTEX = UINT_MAX;		// Data Buffer Texture (holds Apple 2 memory data)
	bool bNeedsGPUVertexUpdate = false;	// Update the GPU if the vertex data has changed
	bool bNeedsGPUDataUpdate = false;	// Update the GPU if the underlying data has changed

	A2Window()
		: enabled(false)
		, index(UINT8_MAX)
		, screen_count(uXY({ 0,0 }))
		, tile_dim(uXY({ 0,0 }))
		, tile_count(uXY({ 0,0 }))
		, data(nullptr)
		, datasize(0)
		, shaderProgram(nullptr)
	{
		// Assign the vertex array.
		// The first 2 values are the relative XY, bound from -1 to 1.
		// The A2Window always covers the whole screen, so from -1 to 1 on both axes
		// The second pair of values is the actual pixel value on screen (280x192, etc...)
		// Set them to 0, they'll be defined later from the video manager
		vertices.push_back(A2Vertex({glm::vec2(-1,  1), glm::ivec2(0, 0)}));	// top left
		vertices.push_back(A2Vertex({glm::vec2( 1, -1), glm::ivec2(0, 0)}));	// bottom right
		vertices.push_back(A2Vertex({glm::vec2( 1,  1), glm::ivec2(0, 0)}));	// top right
		vertices.push_back(A2Vertex({glm::vec2(-1,  1), glm::ivec2(0, 0)}));	// top left
		vertices.push_back(A2Vertex({glm::vec2(-1, -1), glm::ivec2(0, 0)}));	// bottom left
		vertices.push_back(A2Vertex({glm::vec2( 1, -1), glm::ivec2(0, 0)}));	// bottom right
	};

	void Define(uint8_t _index, uXY _screen_count,
		uXY _tile_dim, uXY _tile_count,
		uint8_t* _data, uint32_t _datasize,
		Shader* _shaderProgram);
	void Update();
	void Render();

	Shader* GetShaderProgram() { return shaderProgram; };
	void SetShaderProgram(Shader* _shader) { shaderProgram = _shader; };

	bool IsEmpty() { return (tile_count.x == 0 || tile_count.y == 0); };

	uint8_t Get_index() const { return index; }
	uXY Get_screen_count() const { return screen_count; }
	uXY Get_tile_dim() const { return tile_dim; }
	uXY Get_tile_count() const { return tile_count; }

private:
	void Reset();

	uint8_t index;			// index of window (is also the z-value: higher is closer to camera)
	uXY screen_count;		// width,height in pixels of visible screen area of window
	uXY tile_dim;			// xy dimension, in pixels, of tiles in the window.
	uXY tile_count;			// xy dimension, in tiles, of the tile array
	Shader* shaderProgram;	// Shader used

	// TODO: Allow for 2 regions to be uploaded for DTEXT, DLORES, DHGR
	uint8_t* data;		// The underlying data that will be used by the shader
	uint32_t datasize;	// Data size in bytes

	std::vector<A2Vertex> vertices;		// Vertices with XYRelative and XYPixels
	unsigned int VAO = UINT_MAX;		// Vertex Array Object (holds buffers that are vertex related)
	unsigned int VBO = UINT_MAX;		// Vertex Buffer Object (holds vertices)
};

#endif // A2WINDOW_H