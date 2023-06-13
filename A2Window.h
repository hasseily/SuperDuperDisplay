#pragma once
#ifndef A2WINDOW_H
#define A2WINDOW_H

#include <vector>
#include "shader.h"

class A2Window
{
public:
	bool enabled;
	unsigned int DBTEX = UINT_MAX;		// Data Buffer Texture (holds Apple 2 memory data)

	A2Window()
		: enabled(false)
		, index(UINT8_MAX)
		, screen_count(uXY({ 0,0 }))
		, tile_dim(uXY({ 0,0 }))
		, tile_count(uXY({ 0,0 }))
		, data(nullptr)
		, datasize(0)
		, shaderProgram(nullptr)
	{};

	void Define(uint8_t _index, uXY _screen_count,
		uXY _tile_dim, uXY _tile_count,
		uint8_t* _data, uint32_t _datasize,
		Shader* _shaderProgram);
	void Update();
	void Render(const glm::mat4& mat_camera, const glm::mat4& mat_proj);

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

	bool bNeedsGPUVertexUpdate = false;	// Update the GPU if the vertex data has changed
	bool bNeedsGPUDataUpdate = false;	// Update the GPU if the underlying data has changed

	std::vector<glm::vec4> vertices;	// Vertices with XY and UV	(2 vectors of floats)
	unsigned int VAO = UINT_MAX;		// Vertex Array Object (holds buffers that are vertex related)
	unsigned int VBO = UINT_MAX;		// Vertex Buffer Object (holds vertices)
};

#endif // A2WINDOW_H