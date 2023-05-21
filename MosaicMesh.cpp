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
					glm::fvec2(0, 0),	// UV (not set yet, waiting for SDHR_CMD_UPDATE_WINDOW...)
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
			_v.Position = glm::vec3(tile_xdim * i, tile_ydim * j, z_val);	// bottom left
			this->vertices.push_back(_v);
			this->texIndexes.push_back(0);	// Texture index (not set yet, waiting for SDHR_CMD_UPDATE_WINDOW...)
			_v.Position = glm::vec3(tile_xdim * (i + 1), tile_ydim * (j + 1), z_val);	// top right
			this->vertices.push_back(_v);
			this->texIndexes.push_back(0);	// Texture index (not set yet, waiting for SDHR_CMD_UPDATE_WINDOW...)
			_v.Position = glm::vec3(tile_xdim * i, tile_ydim * (j + 1), z_val);	// top left
			this->vertices.push_back(_v);
			this->texIndexes.push_back(0);	// Texture index (not set yet, waiting for SDHR_CMD_UPDATE_WINDOW...)
			// Second triangle
			_v.Position = glm::vec3(tile_xdim * (i + 1), tile_ydim * j, z_val);	// bottom right
			this->vertices.push_back(_v);
			this->texIndexes.push_back(0);	// Texture index (not set yet, waiting for SDHR_CMD_UPDATE_WINDOW...)
			_v.Position = glm::vec3(tile_xdim * (i + 1), tile_ydim * (j + 1), z_val);	// top right
			this->vertices.push_back(_v);
			this->texIndexes.push_back(0);	// Texture index (not set yet, waiting for SDHR_CMD_UPDATE_WINDOW...)
			_v.Position = glm::vec3(tile_xdim * i, tile_ydim * j, z_val);	// bottom left
			this->vertices.push_back(_v);
			this->texIndexes.push_back(0);	// Texture index (not set yet, waiting for SDHR_CMD_UPDATE_WINDOW...)

			++t_idx;
		}
	};

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
	auto _iaw = (float)ia.image_xcount;	// image width and height, as floats so everything is floats
	auto _iah = (float)ia.image_ycount;

	auto _idx = mosaic_index * 6;	// index of the first vertex of the mosaic tile
	// The passed-in U and V are the non-normalized pixel coordinates of the first vertex of the tile
	auto &_v0 = this->vertices.at(_idx);		// reference to the bottom left vertex. Update in place
	auto _vx = _v0.Position.x;		// X coord of the top left of the mosaic tile
	auto _vy = _v0.Position.y;		// Y coord of the top left of the mosaic tile

	bool isDebug = false;
	// isDebug = (_idx >= 9000 && _idx < 9300);
	if (isDebug)
		std::cout << std::dec << "--- MOSAIC START ---" << std::endl;
	// Update each vertex in sequence
	// Bottom left
	_v0.TexCoords = glm::fvec2(u / _iaw, v / _iah);
	this->texIndexes[_idx] = texture_index;
	if (isDebug)
		std::cout << "Vertex BL " << _v0.Position.x << " x " << _v0.Position.y << " : " << _v0.TexCoords.x << " x " << _v0.TexCoords.y << std::endl;
	// Top right
	auto& _v1 = this->vertices.at(_idx + 1);
	_v1.TexCoords = glm::fvec2((u + _v1.Position.x - _vx) / _iaw, (v + _v1.Position.y - _vy) / _iah);
	this->texIndexes[_idx + 1] = texture_index;
	if (isDebug)
		std::cout << "Vertex TR " << _v1.Position.x << " x " << _v1.Position.y << " : " << _v1.TexCoords.x << " x " << _v1.TexCoords.y << std::endl;
	// Top left
	auto& _v2 = this->vertices.at(_idx + 2);
	_v2.TexCoords = glm::fvec2(u / _iaw, (v + _v2.Position.y - _vy) / _iah);
	this->texIndexes[_idx + 2] = texture_index;
	if (isDebug)
		std::cout << "Vertex TL " << _v2.Position.x << " x " << _v2.Position.y << " : " << _v2.TexCoords.x << " x " << _v2.TexCoords.y << std::endl;
	// Bottom right
	auto& _v3 = this->vertices.at(_idx + 3);
	_v3.TexCoords = glm::fvec2((u + _v3.Position.x - _vx) / _iaw, v / _iah);
	this->texIndexes[_idx + 3] = texture_index;
	if (isDebug)
		std::cout << "    Vertex BR " << _v3.Position.x << " x " << _v3.Position.y << " : " << _v3.TexCoords.x << " x " << _v3.TexCoords.y << std::endl;
	// Top right (again)
	auto& _v4 = this->vertices.at(_idx + 4);
	_v4.TexCoords = glm::fvec2((u + _v4.Position.x - _vx) / _iaw, (v + _v4.Position.y - _vy) / _iah);
	this->texIndexes[_idx + 4] = texture_index;
	if (isDebug)
		std::cout << "   Vertex TR " << _v4.Position.x << " x " << _v4.Position.y << " : " << _v4.TexCoords.x << " x " << _v4.TexCoords.y << std::endl;
	// Bottom left (again)
	auto& _v5 = this->vertices.at(_idx + 5);
	_v5.TexCoords = glm::fvec2(u / _iaw, v / _iah);
	this->texIndexes[_idx + 5] = texture_index;
	if (isDebug)
		std::cout << "    Vertex BL " << _v5.Position.x << " x " << _v5.Position.y << " : " << _v5.TexCoords.x << " x " << _v5.TexCoords.y << std::endl;

/*
	// XXX Test stuff to change a single tile
	if (mosaic_index == 256 * 128 + 127)	// center of big map
	{
		auto _v = Vertex({
				glm::vec3(0, 0, 0),	// vertex position (z is the reverse window index)
				glm::fvec2(0, 0),	// UV
			});

		_v.Position = glm::fvec3(_v0.Position.x, _v0.Position.y, _v0.Position.z);
		_v.TexCoords = glm::fvec2(0, 0);
		this->vertices[_idx] = _v;
		this->texIndexes[_idx] = 0; // bl

		_v.Position = glm::vec3(_v1.Position.x + 500, _v1.Position.y + 500, _v1.Position.z);
		_v.TexCoords = glm::fvec2(1, 1);
		this->vertices[_idx + 1] = _v;
		this->texIndexes[_idx + 1] = 0; // tr

		_v.Position = glm::vec3(_v2.Position.x, _v2.Position.y + 500, _v2.Position.z);
		_v.TexCoords = glm::fvec2(0, 1);
		this->vertices[_idx + 2] = _v;
		this->texIndexes[_idx + 2] = 0; // tl

		_v.Position = glm::vec3(_v3.Position.x + 500, _v3.Position.y, _v3.Position.z);
		_v.TexCoords = glm::fvec2(1, 0);
		this->vertices[_idx + 3] = _v;
		this->texIndexes[_idx + 3] = 0;	// br

		_v.Position = glm::vec3(_v4.Position.x + 500, _v4.Position.y + 500, _v4.Position.z);
		_v.TexCoords = glm::fvec2(1, 1);
		this->vertices[_idx + 4] = _v;
		this->texIndexes[_idx + 4] = 0;	// tr

		_v.Position = glm::vec3(_v5.Position.x, _v5.Position.y, _v5.Position.z);
		_v.TexCoords = glm::fvec2(0, 0);
		this->vertices[_idx + 5] = _v;
		this->texIndexes[_idx + 5] = 0;	// bl
	}
	*/

	bNeedsGPUUpdate = true;

}

void MosaicMesh::SetWorldCoordinates(int32_t x, int32_t y)
{
	this->world_x = x;
	this->world_y = y;
	// Update the model->world transform matrix, to translate the model into the world space
	this->mat_trans = glm::translate(glm::mat4(1.0f), glm::vec3(world_x, world_y, 0.0f));

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
	if (VTO == UINT_MAX)
		glGenBuffers(1, &VTO);

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
	// Use the texIndexes array here
	glBindBuffer(GL_ARRAY_BUFFER, VTO);
	glBufferData(GL_ARRAY_BUFFER, texIndexes.size() * sizeof(uint8_t), &texIndexes[0], GL_STATIC_DRAW);
	glEnableVertexAttribArray(2);
	glVertexAttribIPointer(2, 1, GL_BYTE, GL_FALSE, (void*)0);

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
