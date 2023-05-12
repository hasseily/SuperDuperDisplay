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

#include "glm/gtc/matrix_transform.hpp"

#include "shader.h"
#include "OpenGLHelper.h"
#include <string>
#include <vector>


using namespace std;

struct TileTex {				// Tile texture starting coordinates
	uint32_t upos;					// U (x) starting position
	uint32_t vpos;					// V (y) starting position
	// TODO: Add flags (inverted, mirrored, ...)
};

struct Vertex {
	// position
	glm::vec3 Position;     // the z position will always be the window's id that the mesh is linked to
	// texCoords
	glm::vec2 TexCoords;
	// texture index (max 16)
	GLbyte TexIndex;

};

struct Texture {
	unsigned int id;
	string type;
	string path;
};

class MosaicMesh
{
public:
	// mesh Data
	vector<Vertex>       vertices;
	vector<unsigned int> indices;
	unsigned int VAO;

	MosaicMesh(uint16_t numquads_x, uint16_t numquads_y, 
		uint16_t tilewidth, uint16_t tileheight,
		uint8_t* tileset_indexes, uint8_t* tile_indexes) {
		uint64_t numquads = numquads_x * numquads_y;
		this->vertices.reserve((numquads_x + 1) * (numquads_y + 1));	// total # of unique vertices
		this->indices.reserve(numquads * 6);	// total # of non-unique vertices: 2 triangles of 3 vertices each, per tile

		TilesetRecord* tr;
		for (size_t j = 0; j < numquads_y; j++)
		{
			for (size_t i = 0; i < numquads_x; i++)
			{
				this->vertices.push_back(Vertex({ glm::vec3(), glm::vec2() }));
			}
			// Add the vertices at the right border
		}
		// Add the vertices at the bottom border
		setupMesh();
	}

	// render the mesh
	void Draw(Shader& shader)
	{
		// bind all 16 textures at once
		// TODO: Do the binding at the start of rendering all the window meshes
		//			It'll be more efficent than binding at every draw of each mesh
		
		auto oglh = OpenGLHelper::GetInstance();
		auto vti = oglh->v_texture_ids;
		for (unsigned int i = 0; i < vti.size(); i++) {
			glBindTexture(GL_TEXTURE_2D, vti.at(i));
		}

		// draw mesh
		glBindVertexArray(VAO);
		glDrawElements(GL_TRIANGLES, static_cast<unsigned int>(indices.size()), GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);

		// always good practice to set everything back to defaults once configured.
		glActiveTexture(GL_TEXTURE0);
	}

private:
	// render data 
	unsigned int VBO;       // Vertex Buffer Object (holds vertices)
	unsigned int EBO;       // Element Buffer Object (hold indices for vertices)

	// initializes all the buffer objects/arrays
	void setupMesh()
	{
		// create buffers/arrays
		glGenVertexArrays(1, &VAO);
		glGenBuffers(1, &VBO);
		glGenBuffers(1, &EBO);

		glBindVertexArray(VAO);
		// load data into vertex buffers
		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), &vertices[0], GL_STATIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);

		// set the vertex attribute pointers
		// vertex Positions: position 0, size 3
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
		// vertex texture coords: position 1, size 2
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, TexCoords));
		// vertex texture index: position 2, size 1
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 1, GL_BYTE, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, TexIndex));

		// reset the binding
		glBindVertexArray(0);
	}
};

#endif // !MOSAICMESH_H
