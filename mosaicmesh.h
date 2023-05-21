#pragma once
#ifndef MOSAICMESH_H
#define MOSAICMESH_H

/**
 * MosaicMesh generates a flat mesh comprised
 * of tile quads. The constructor needs the following:
 *	- number of tiles in the x and y axes
 *	- width and height of tiles in pixels
 *	- a pointer to a C-style array of indexes of tilesets
 *	- a pointer to a C-style array of indexes of tiles
 * 
 * The last 2 arrays are of size (numquads_x * numquads_y). Each quad gets therefore
 * its own entry of which tileset it belongs to, and within this tileset, which tile it is
 */

#include "common.h"
#include <string>
#include <vector>

#include "shader.h"
#include "glm/gtc/matrix_transform.hpp"

using namespace std;

struct Vertex {
	// position
	glm::vec3 Position;     // the z position will always be the window's id that the mesh is linked to
	// texCoords
	glm::fvec2 TexCoords;
};

class MosaicMesh
{
public:
	// mesh Data
	vector<Vertex> vertices;		// Vertices with XY and UV	(2 vectors of floats)
	vector<uint8_t> texIndexes;		// Texture index of each vertex (integers)
	uint64_t cols = 0;			// # of mosaic tiles horizontally
	uint64_t rows = 0;			// # of mosaic tiles vertically

	unsigned int VAO = UINT_MAX;
	Shader* shaderProgram = NULL;		// Shader program for the mesh. Starts with a default shader

	MosaicMesh(uint64_t tile_xcount, uint64_t tile_ycount, uint64_t tile_xdim, uint64_t tile_ydim, uint8_t win_index);
	MosaicMesh() = delete; // Disallow default constructor

	void UpdateMosaicUV(uint64_t xpos, uint64_t ypos, uint64_t u, uint64_t v, uint8_t texture_index);
	void UpdateMosaicUV(uint64_t mosaic_index, uint64_t u, uint64_t v, uint8_t texture_index);

	void SetWorldCoordinates(int32_t x, int32_t y);

	// updates all the buffer objects/arrays
	void updateMesh();

	// render the mesh
	void Draw(const glm::mat4& mat_camera, const glm::mat4& mat_proj);

private:
	// render data
	unsigned int VBO = UINT_MAX;       // Vertex Buffer Object (holds vertices)
	unsigned int VTO = UINT_MAX;	   // Vertex Texture Buffer Object (holds texture indexes)
	float world_x = 0.f;		// top-left position in the world space
	float world_y = 0.f;		// which is also the view (camera) space
	glm::mat4 mat_trans = glm::mat4(1.0f);	// Model->World translation matrix. Changes when the mesh is moved in the world

	bool bNeedsGPUUpdate = true;	// the mesh data was updated, it needs to be pushed to the GPU
};

#endif // !MOSAICMESH_H
