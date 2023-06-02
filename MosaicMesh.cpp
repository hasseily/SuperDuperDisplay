#include "MosaicMesh.h"

#include "glm/gtc/type_ptr.hpp"
#include "OpenGLHelper.h"
#include "SDHRManager.h"

MosaicMesh::MosaicMesh(uint32_t tile_xcount, uint32_t tile_ycount, uint32_t tile_xdim, uint32_t tile_ydim, uint8_t win_index) {
	cols = tile_xcount;	// number of columns
	rows = tile_ycount;	// number of rows
	width = tile_xcount * tile_xdim;
	height = tile_ycount * tile_ydim;

	this->mosaicTiles.reserve(cols * rows);	// total # of tiles in the mesh

	// This is a default vertex that we'll use as base to insert into vertices later
	// Because push_back() makes a copy, it's faster not to recreate the vertex every time
	auto _v = Vertex({
					glm::vec4(0),	// vertex position (z is the reverse window index, w is 1 for the top left origin only)
					glm::vec4(1),	// tint (to be potentially assigned later)	// TODO
		});
	auto _t = MosaicTile({ 
		1.f, 1.f,						// uv position
		1.f / _SDHR_MAX_UV_SCALE,		// uv scale
		0.f / _SDHR_MAX_TEXTURES,		// texture index
		});

	// Create all the vertices for each tile
	float z_val = (float)(~win_index);	// z plane is 0-255. Window index 0 is the furthest away

	// NOTE: We use the top left corner for both triangles so that all the fragments later
	// can know the top left corner position to calculate which tile they belong to
	// First triangle
	_v.Position = glm::vec4(0, height, z_val, 1);	// top left
	this->vertices.push_back(_v);
	_v.Position = glm::vec4(width, 0, z_val, 0);	// bottom right
	this->vertices.push_back(_v);
	_v.Position = glm::vec4(width, height, z_val, 0);	// top right
	this->vertices.push_back(_v);
	// Second triangle
	_v.Position = glm::vec4(0, height, z_val, 1);	// top left
	this->vertices.push_back(_v);
	_v.Position = glm::vec4(0, 0, z_val, 0);	// bottom left
	this->vertices.push_back(_v);
	_v.Position = glm::vec4(width, 0, z_val, 0);	// bottom right
	this->vertices.push_back(_v);


	for (size_t j = 0; j < rows; j++)
	{
		for (size_t i = 0; i < cols; i++)
		{
			this->mosaicTiles.push_back(_t);
		}
	};

	bNeedsGPUUpdate = true;
}

MosaicMesh::~MosaicMesh()
{
	if (VAO != UINT_MAX)
	{
		glDeleteTextures(1, &TBTEX);
		glDeleteBuffers(1, &TBO);
		glDeleteBuffers(1, &VBO);
		glDeleteBuffers(1, &VAO);
	}
}

// Update the UV data of a single mosaic tile (using xy positioning)
void MosaicMesh::UpdateMosaicUV(uint32_t xpos, uint32_t ypos, uint32_t u, uint32_t v, uint8_t texture_index)
{
	UpdateMosaicUV(xpos + ypos * cols, u, v, texture_index);
}

// Update the UV data of a single mosaic tile (using index positioning)
void MosaicMesh::UpdateMosaicUV(uint32_t mosaic_index, uint32_t u, uint32_t v, uint8_t texture_index)
{
	const auto ia = SDHRManager::GetInstance()->image_assets[texture_index];
	auto _iaw = (float)ia.image_xcount;	// image width and height, as floats so everything is floats
	auto _iah = (float)ia.image_ycount;

	// Calculate the mosaic tile metadata

	auto &_t0 = this->mosaicTiles.at(mosaic_index);		// Update in place
	_t0.x = u / _iaw;
	_t0.y = v / _iah;
	_t0.texIdx = float(texture_index) / _SDHR_MAX_TEXTURES;

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

	GLenum glerr;

	// Now create the buffers/arrays and tile buffer texture that holds the MosaicTile data.
	// We're using a texture instead of a uniform buffer object because
	// the size of the mosaic object is variable
	if (VAO == UINT_MAX)
	{
		glGenVertexArrays(1, &VAO);
		glGenBuffers(1, &VBO);
		glGenBuffers(1, &TBO);
		glGenTextures(1, &TBTEX);
	}

	glBindVertexArray(VAO);
	// load data into vertex buffers
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), &vertices[0], GL_STATIC_DRAW);

	// set the vertex attribute pointers
	// vertex Positions: position 0, size 3
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
	// vertex tint color: position 1, size 4
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Tint));

	// Associate the texture TBTEX in GL_TEXTURE0+TEXUNIT with the buffer
	glActiveTexture(GL_TEXTURE0 + _SDHR_TBO_TEXUNIT);
	glBindTexture(GL_TEXTURE_2D, TBTEX);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, cols, rows, 0, GL_RGBA, GL_FLOAT, &this->mosaicTiles[0]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);	// Note: Could also use GL_LINEAR, need to test

	// reset the binding
	glBindVertexArray(0);

	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "updateMesh error: " << glerr << std::endl;
	}

	// Update the model->world transform matrix, to translate the model into the world space
	this->mat_trans = glm::translate(glm::mat4(1.0f), glm::vec3(world_x, world_y, 0.0f));

	bNeedsGPUUpdate = false;
}

// render the mesh
// NOTE: This (and any methods with OpenGL calls) must be called from the main thread
// NOTE: It assumes the textures have been already bound to _SDHR_START_TEXTURES forward
void MosaicMesh::Draw(const glm::mat4& mat_camera, const glm::mat4& mat_proj)
{
	GLenum glerr;
	glUseProgram(shaderProgram->ID);
	glBindVertexArray(VAO);

	// Assign the scales so that we can get the proper original
	// values for each mosaic tile
	shaderProgram->setFloat("maxTextures", _SDHR_START_TEXTURES);
	shaderProgram->setFloat("maxUVScale", _SDHR_MAX_UV_SCALE);
	shaderProgram->setVec2u("tileCount", this->cols, this->rows);
	shaderProgram->setVec2u("meshSize", this->width, this->height);

	glm::mat4 mat_final = mat_proj * mat_camera * this->mat_trans;
	shaderProgram->setMat4("transform", mat_final);

	// point the uniform at the tiles data texture (GL_TEXTURE0 + _SDHR_TBO_TEXUNIT)
	glActiveTexture(GL_TEXTURE0 + _SDHR_TBO_TEXUNIT);
	glBindTexture(GL_TEXTURE_2D, TBTEX);
	shaderProgram->setInt("TBTEX", _SDHR_TBO_TEXUNIT);
	// back to the output buffer to draw our scene
	glActiveTexture(GL_TEXTURE0);
	glDrawArrays(GL_TRIANGLES, 0, (GLsizei)this->vertices.size());
	glBindVertexArray(0);
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "MosaicMesh render error: " << glerr << std::endl;
	}

}
