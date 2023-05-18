#include "MosaicMesh.h"

#include "glm/gtc/type_ptr.hpp"
#include "OpenGLHelper.h"
#include "SDHRManager.h"

MosaicMesh::MosaicMesh(uint64_t tile_xcount, uint64_t tile_ycount, uint64_t tile_xdim, uint64_t tile_ydim, uint8_t win_index) {
	this->vertices.reserve(tile_xcount * tile_ycount * 6);	// total # of unique vertices

	size_t t_idx = 0;	// 1D tile index
	cols = tile_xcount;	// number of columns
	rows = tile_ycount;	// number of rows

	// This is a default vertex that we'll use as base to insert into vertices later
	// Because push_back() makes a copy, it's faster not to recreate the vertex every time
	auto _v = Vertex({
					glm::vec3(0, 0, 0),	// vertex position (z is the reverse window index)
					glm::vec2(0, 0),	// UV (not set yet, waiting for SDHR_CMD_UPDATE_WINDOW...)
					(uint8_t)0			// Texture index (not set yet, waiting for SDHR_CMD_UPDATE_WINDOW...)
		});

	// Create all the vertices for each tile
	float fcols = (float)cols;
	float frows = (float)rows;
	float z_val = (float)(~win_index);	// z plane is 0-255. Window index 0 is the furthest away
	for (size_t j = 0; j < rows; j++)
	{
		for (size_t i = 0; i < cols; i++)
		{
			// First triangle
			_v.Position = glm::vec3(tile_xdim * i, tile_ydim * j, z_val);	// top left
			this->vertices.push_back(_v);
			_v.Position = glm::vec3(tile_xdim * (i + 1), tile_ydim * j, z_val);	// top right
			this->vertices.push_back(_v);
			_v.Position = glm::vec3(tile_xdim * i, tile_ydim * (j + 1), z_val);	// bottom left
			this->vertices.push_back(_v);
			// Second triangle
			_v.Position = glm::vec3(tile_xdim * i, tile_ydim * (j + 1), z_val);	// bottom left
			this->vertices.push_back(_v);
			_v.Position = glm::vec3(tile_xdim * (i + 1), tile_ydim * j, z_val);	// top right
			this->vertices.push_back(_v);
			_v.Position = glm::vec3(tile_xdim * (i + 1), tile_ydim * (j + 1), z_val);	// bottom right
			this->vertices.push_back(_v);
			++t_idx;
		}
	};

	this->vertices[0].Position = glm::vec3(-1100, 1900, 0.2);
	this->vertices[1].Position = glm::vec3(-1000, 1900, 0.2);
	this->vertices[2].Position = glm::vec3(-1000, 2000, 0.2);
	this->vertices[3].Position = glm::vec3(-1300, 2900, 0.2);
	this->vertices[4].Position = glm::vec3(-1300, 1700, 0.2);
	this->vertices[5].Position = glm::vec3(-1200, 3000, 0.2);

	this->vertices[0].TexCoords = glm::vec2(0.1, 0.1);
	this->vertices[1].TexCoords = glm::vec2(0.5, 0.1);
	this->vertices[2].TexCoords = glm::vec2(0.3, 0.5);
	this->vertices[3].TexCoords = glm::vec2(0.2, 0.6);
	this->vertices[4].TexCoords = glm::vec2(0.4, 0.6);
	this->vertices[5].TexCoords = glm::vec2(0.3, 0.8);

	this->vertices[0].TexIndex = 0;
	this->vertices[1].TexIndex = 0;
	this->vertices[2].TexIndex = 0;
	this->vertices[3].TexIndex = 0;
	this->vertices[4].TexIndex = 0;
	this->vertices[5].TexIndex = 1;

	bNeedsGPUUpdate = true;
}

// Update the UV data of all the vertices of a single mosaic tile (using xy positioning)
void MosaicMesh::UpdateMosaicUV(uint64_t xpos, uint64_t ypos, uint64_t u, uint64_t v, uint8_t texture_index)
{
	UpdateMosaicUV(xpos + ypos * cols, u, v, texture_index);
}

// Update the UV data of all the vertices of a single mosaic tile (using index positioning)
void MosaicMesh::UpdateMosaicUV(uint64_t mosaic_index, uint64_t u, uint64_t v, uint8_t texture_index)
{
	const auto ia = SDHRManager::GetInstance()->image_assets[texture_index];
	auto _idx = mosaic_index * 6;	// index of the first vertex of the mosaic tile
	for (size_t i = 0; i < 6; i++)
	{
		auto& _v = this->vertices.at(_idx + i);		// reference to the vertex. Update in place
		_v.TexCoords = glm::vec2((float)u / ia.image_xcount, (float)v / ia.image_ycount);
		_v.TexIndex = texture_index;
	}
	bNeedsGPUUpdate = true;
}

void MosaicMesh::SetWorldCoordinates(int32_t x, int32_t y)
{
	this->world_x = x;
	this->world_y = y;
}

// Anytime the underlying mesh data is changed, it needs to be updated on the GPU
// This method takes care of sending over both vertex buffers and attributes
// // NOTE: This (and any methods with OpenGL calls) must be called from the main thread
// TODO: Allow for only attributes to be sent over when the vertices don't change
void MosaicMesh::updateMesh()
{
	if (!bNeedsGPUUpdate)
		return;				// mesh doesn't need updating on the GPU

	// create buffers/arrays
	if (VAO == UINT_MAX)
		glGenVertexArrays(1, &VAO);
	if (VBO == UINT_MAX)
		glGenBuffers(1, &VBO);

	glBindVertexArray(VAO);
	// load data into vertex buffers
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), &vertices[0], GL_STATIC_DRAW);

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

	// Update the model->world transform matrix, to translate the model into the world space
	this->mat_trans = glm::translate(glm::mat4(1.0f), glm::vec3(world_x, world_y, 0.0f));

	// reset the binding
	glBindVertexArray(0);
	bNeedsGPUUpdate = false;
}

// render the mesh
// NOTE: This (and any methods with OpenGL calls) must be called from the main thread
// NOTE: It assumes the textures have been already bound to GL_TEXTURE0... GL_TEXTURE16
void MosaicMesh::Draw(const glm::mat4& mat_camera, const glm::mat4& mat_proj)
{
	glUseProgram(shaderProgram->ID);
	glBindVertexArray(VAO);
	glm::mat4 mat_final = mat_proj * mat_camera * this->mat_trans;
	shaderProgram->setMat4("transform", mat_final);
	glDrawArrays(GL_TRIANGLES, 0, (GLsizei)this->vertices.size());
	glBindVertexArray(0);
}
