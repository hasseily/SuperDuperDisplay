#include "BasicQuad.h"
#include "common.h"
#include <SDL_timer.h>
#include "A2VideoManager.h"
#include "SDHRManager.h"

BasicQuad::BasicQuad(const char* shaderVertexPath, const char* shaderFragmentPath)
{
	UpdateVertexArray();
	shader = Shader();
	this->SetShaderPrograms(shaderVertexPath, shaderFragmentPath);
}

BasicQuad::~BasicQuad()
{
	if (VAO != UINT_MAX)
	{
		glDeleteVertexArrays(1, &VAO);
		glDeleteBuffers(1, &VBO);
	}
}

void BasicQuad::SetShaderPrograms(const char* shaderVertexPath, const char* shaderFragmentPath)
{
	this->shader.build(shaderVertexPath, shaderFragmentPath);
	// Required uniforms for any shader used by BasicQuad
	u_ticks = glGetUniformLocation(shader.ID, "ticks");
	u_frameIsOdd = glGetUniformLocation(shader.ID, "frameIsOdd");
	u_TEXIN = glGetUniformLocation(shader.ID, "TEXIN");
}

void BasicQuad::SetQuadRelativeBounds(SDL_FRect bounds)
{
	quad = bounds;
	this->UpdateVertexArray();
}

void BasicQuad::UpdateVertexArray()
{
	// Assign the vertex array.
	// The first 2 values are the relative XY, bound from -1 to 1.
	// The second pair of values is the actual pixel value on screen
	vertices.clear();
	vertices.push_back(BasicQuadVertex({ glm::vec2(quad.x, quad.y), glm::ivec2(0, 1) }));	// top left
	vertices.push_back(BasicQuadVertex({ glm::vec2(quad.x + quad.w, quad.y + quad.h), glm::ivec2(1, 0) }));	// bottom right
	vertices.push_back(BasicQuadVertex({ glm::vec2(quad.x + quad.w, quad.y), glm::ivec2(1, 1) }));	// top right
	vertices.push_back(BasicQuadVertex({ glm::vec2(quad.x, quad.y), glm::ivec2(0, 1) }));	// top left
	vertices.push_back(BasicQuadVertex({ glm::vec2(quad.x, quad.y + quad.h), glm::ivec2(0, 0) }));	// bottom left
	vertices.push_back(BasicQuadVertex({ glm::vec2(quad.x + quad.w, quad.y + quad.h), glm::ivec2(1, 0) }));	// bottom right
}

void BasicQuad::Render(uint64_t frame_idx)
{
	if (!shader.isReady)
		return;
	if (vertices.size() == 0)
		return;

	GLenum glerr;

	if (VAO == UINT_MAX)
	{
		glGenVertexArrays(1, &VAO);
		glGenBuffers(1, &VBO);
	}

	shader.use();
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL BasicQuad glUseProgram error: " << glerr << std::endl;
		std::cerr << "Did you set custom uniforms without first calling shader.use()?" << std::endl;
		return;
	}
	
	glBindVertexArray(VAO);

	// Always reload the vertices
	// because compatibility with GL-ES on the rPi
	{
		// load data into vertex buffers
		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(BasicQuadVertex), &vertices[0], GL_STATIC_DRAW);

		// set the vertex attribute pointers
		// vertex relative Positions: position 0, size 2
		// (vec4 values x and y)
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(BasicQuadVertex), (void*)0);
		// vertex pixel Positions: position 1, size 2
		// (vec4 values z and w)
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(BasicQuadVertex), (void*)offsetof(BasicQuadVertex, PixelPos));
	}

	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "BasicQuad::Render 1 error: " << glerr << std::endl;
	}

	// Required uniforms for any shader used by BasicQuad
	glUniform1i(u_ticks, SDL_GetTicks());
	glUniform1i(u_frameIsOdd, (int)(frame_idx & 1));
	glUniform1i(u_TEXIN, inputTextureUnit - GL_TEXTURE0);

	glDrawArrays(GL_TRIANGLES, 0, (GLsizei)this->vertices.size());
	glActiveTexture(GL_TEXTURE0);
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "BasicQuad::Render 2 error: " << glerr << std::endl;
	}
	return;
}
