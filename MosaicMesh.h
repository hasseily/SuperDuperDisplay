#pragma once
#ifndef MOSAICMESH_H
#define MOSAICMESH_H

/**
 * This MosaicMesh generates a flat mesh in a single quad
 * The constructor needs the following:
 *	- number of tiles in the x and y axes
 *	- width and height of tiles in pixels
 *	- the window index this mesh belongs to
 * 
 * Determining the tiles themselves is done exclusively in the shader program.
 * The shader programe will look at the position of the pixel inside the mesh
 * and determine the tile it belongs to and grab the relative pixel color from
 * the tileset image.
 */

#include "common.h"
#include <string>
#include <vector>

#include "shader.h"
#include "glm/gtc/matrix_transform.hpp"

using namespace std;

// There will be 6 vertices, 2 triangles, 1 quad (rectangle) for the whole MosaicMesh
// the z position will always be the window's id that the mesh is linked to
// and the w position is 1.0 for the top left corner, 0.0 for the others
struct Vertex {
	// position
	glm::vec4 Position;     
	glm::vec4 Tint;			// the vertex's color which will be a tint on the fragment color
};

// For each tile inside, specify the texture index, the uv of its top left corner, and the uv scale
// NOTE: Everything must be bound in the 0-1 space because of the 2D Texture specifications
struct alignas(4) MosaicTile {
	float x;			// uv coords in 0-1 space
	float y;
	float uvscale;		// Scale should default to 1 for SDHR.
	float texIdx;		// It's the texture id, effectively an int
};

class MosaicMesh
{
public:
	// mesh Data
	vector<Vertex> vertices;		// Vertices with XY and UV	(2 vectors of floats)
	uint32_t cols = 0;			// # of mosaic tiles horizontally
	uint32_t rows = 0;			// # of mosaic tiles vertically
	uint32_t width = 0;			// width and height of the mesh in pixels
	uint32_t height = 0;

	// A vector of uvec3 for each tile, specifying texture index + uv coords of each tile
	vector<MosaicTile> mosaicTiles;

	float pixelSize = 1.f;					// For pixelization effect of pixelization shader
	Shader* shaderProgram = NULL;		// Shader program for the mesh. Starts with a default shader

	unsigned int TBTEX = UINT_MAX;		// MosaicTile Buffer Texture

	// Texture samplers of the 16 textures the meshe will use
	// That's just 16 consecutive integers starting at _SDHR_START_TEXTURES
	GLint texSamplers[_SDHR_MAX_TEXTURES];

	MosaicMesh(uint32_t tile_xcount, uint32_t tile_ycount, uint32_t tile_xdim, uint32_t tile_ydim, uint8_t win_index);
	MosaicMesh() = delete; // Disallow default constructor
	~MosaicMesh();

	void UpdateMosaicUV(uint32_t xpos, uint32_t ypos, uint32_t u, uint32_t v, uint8_t texture_index);
	void UpdateMosaicUV(uint32_t mosaic_index, uint32_t u, uint32_t v, uint8_t texture_index);

	void SetWorldCoordinates(int32_t x, int32_t y);
	glm::vec2 GetWorldCoordinates() { return glm::vec2(world_x, world_y); };

	// updates all the buffer objects/arrays
	void updateMesh();

	// call before Draw to activate shader and bind vertices
	void SetupDraw();
	// render the mesh
	void Draw(const glm::mat4& mat_camera, const glm::mat4& mat_proj);

private:
	// render data
	unsigned int VAO = UINT_MAX;		// Vertex Array Object (holds buffers that are vertex related)
	unsigned int VBO = UINT_MAX;       // Vertex Buffer Object (holds vertices)
	float world_x = 0.f;		// top-left position in the world space
	float world_y = 0.f;		// which is also the view (camera) space
	glm::mat4 mat_trans = glm::mat4(1.0f);	// Model->World translation matrix. Changes when the mesh is moved in the world

	uint32_t ticks_since_first_render;
	bool bIsFirstDraw = true;		// Resets when mesh data is updated

	bool bNeedsGPUUpdate = true;	// the mesh data was updated, it needs to be pushed to the GPU
};

#endif // !MOSAICMESH_H
