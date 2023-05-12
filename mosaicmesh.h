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

using namespace std;

struct Vertex {
	// position
	glm::vec3 Position;     // the z position will always be the window's id that the mesh is linked to
	// texCoords
	glm::vec2 TexCoords;
	// texture index (max 16)
	uint8_t TexIndex;
};

class MosaicMesh
{
public:
	// mesh Data
	vector<Vertex> vertices;
	uint64_t cols = 0;			// # of mosaic tiles horizontally
	uint64_t rows = 0;			// # of mosaic tiles vertically
	unsigned int VAO;

	MosaicMesh(uint64_t tile_xcount, uint64_t tile_ycount, uint64_t tile_xdim, uint64_t tile_ydim, uint8_t win_index);
	MosaicMesh() = delete; // Disallow default constructor

	void UpdateMosaicUV(uint64_t xpos, uint64_t ypos, uint64_t u, uint64_t v, uint8_t texture_index);
	void UpdateMosaicUV(uint64_t mosaic_index, uint64_t u, uint64_t v, uint8_t texture_index);

	// render the mesh
	void Draw(Shader& shader);

private:
	// render data
	unsigned int VBO;       // Vertex Buffer Object (holds vertices)

	bool bNeedsGPUUpdate = true;	// the mesh data was updated, it needs to be pushed to the GPU

	// updates all the buffer objects/arrays
	void updateMesh();
};

#endif // !MOSAICMESH_H
