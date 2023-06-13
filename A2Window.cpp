#include "A2Window.h"

void A2Window::Reset()
{
	enabled = 0;
	tile_dim = uXY({ 0,0 });
	tile_count = uXY({ 0,0 });
}

void A2Window::Define(uint8_t _index, uXY _screen_count, 
	uXY _tile_dim, uXY _tile_count, 
	uint8_t* _data, uint32_t _datasize, 
	Shader* _shaderProgram)
{
	this->Reset();
	index = _index;
	screen_count = _screen_count;
	tile_dim = _tile_dim;
	tile_count = _tile_count;
	data = _data;
	datasize = _datasize;
	shaderProgram = _shaderProgram;

	// Create the vertices that make up a rectangle of the complete screen
	auto width = _tile_count.x * _tile_dim.x;
	auto height = _tile_count.y * _tile_dim.y;

	float z_val = (float)(index);	// z plane is 0-255.
	this->vertices.push_back(glm::vec4(0, height, z_val, 1));
	this->vertices.push_back(glm::vec4(width, 0, z_val, 0));
	this->vertices.push_back(glm::vec4(width, height, z_val, 0));
	this->vertices.push_back(glm::vec4(0, height, z_val, 1));
	this->vertices.push_back(glm::vec4(0, 0, z_val, 0));
	this->vertices.push_back(glm::vec4(width, 0, z_val, 0));

	bNeedsGPUVertexUpdate = true;
	bNeedsGPUDataUpdate = true;
}

void A2Window::Update()
{
	if (vertices.size() == 0)
		return;
	if (!(bNeedsGPUVertexUpdate || bNeedsGPUDataUpdate))
		return;				// doesn't need updating on the GPU

	GLenum glerr;

	if (VAO == UINT_MAX)
	{
		glGenVertexArrays(1, &VAO);
		glGenBuffers(1, &VBO);
		glGenTextures(1, &DBTEX);
	}

	glBindVertexArray(VAO);

	if (bNeedsGPUVertexUpdate)
	{
		// load data into vertex buffers
		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec4), &vertices[0], GL_STATIC_DRAW);

		// set the vertex attribute pointers
		// vertex Positions: position 0, size 3
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), (void*)0);
	}

	if (bNeedsGPUDataUpdate)
	{
		// Associate the texture DBTEX in GL_TEXTURE0+_SDHR_TBO_TEXUNIT with the buffer
		// This is the apple 2's memory which is mapped to a "texture"
		glActiveTexture(GL_TEXTURE0 + _SDHR_TBO_TEXUNIT);
		glBindTexture(GL_TEXTURE_2D, DBTEX);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R8UI,
			datasize % 1024,		// GL_R8UI is essentially an array of regular bytes (unbounded)
			1+ (datasize / 1024),	// Split it by 1kB-sized rows
			0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, data);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	// reset the binding
	glBindVertexArray(0);

	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "A2Window::Update error: " << glerr << std::endl;
	}

	bNeedsGPUVertexUpdate = false;
	bNeedsGPUDataUpdate = false;
}

void A2Window::Render(const glm::mat4& mat_camera, const glm::mat4& mat_proj)
{
	if (!enabled)
		return;
	GLenum glerr;
	glUseProgram(shaderProgram->ID);
	glBindVertexArray(VAO);

	shaderProgram->setVec2("windowBottomRight", glm::vec2(screen_count.x, screen_count.y));
	shaderProgram->setVec2u("tileCount", tile_count.x, tile_count.y);
	shaderProgram->setVec2u("tileSize", tile_dim.x, tile_dim.y);

	glm::mat4 mat_final = mat_proj * mat_camera;
	shaderProgram->setMat4("transform", mat_final);

	// point the uniform at the tiles data texture (GL_TEXTURE0 + _SDHR_TBO_TEXUNIT)
	glActiveTexture(GL_TEXTURE0 + _SDHR_TBO_TEXUNIT);
	glBindTexture(GL_TEXTURE_2D, DBTEX);
	shaderProgram->setInt("DBTEX", _SDHR_TBO_TEXUNIT);
	// back to the output buffer to draw our scene
	glActiveTexture(GL_TEXTURE0);
	glDrawArrays(GL_TRIANGLES, 0, (GLsizei)this->vertices.size());
	glBindVertexArray(0);
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "MosaicMesh render error: " << glerr << std::endl;
	}
}


