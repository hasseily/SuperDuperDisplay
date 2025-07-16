#include "BasicQuad.h"
#include "common.h"
#include <SDL_timer.h>
#include "A2VideoManager.h"
#include "SDHRManager.h"

BasicQuad::BasicQuad(const char* shaderVertexPath, const char* shaderFragmentPath)
{
	UpdateVertexArray();
	shader = Shader();
	shader.build(shaderVertexPath, shaderFragmentPath);
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

void BasicQuad::Render(uint64_t frame_idx, const ShaderDictionary& shaderDict)
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

	shader.setInt("ticks", SDL_GetTicks());
	shader.setInt("frameIsOdd", (int)(frame_idx & 1));
	shader.setInt("TEXIN", inputTextureUnit - GL_TEXTURE0);

	// And set the shader parameters that were passed in
	for (const auto& [key, value] : shaderDict) {
		std::visit([&](auto&& arg) {
			using T = std::decay_t<decltype(arg)>;
			if constexpr (std::is_same_v<T, bool>) {
				shader.setBool(key, arg);
			}
			else if constexpr (std::is_same_v<T, int>) {
				shader.setInt(key, arg);
			}
			else if constexpr (std::is_same_v<T, float>) {
				shader.setFloat(key, arg);
			}
			else if constexpr (std::is_same_v<T, glm::vec2*>) {
				if (arg)
					shader.setVec2(key, *arg);
				else
					std::cerr << "Warning: Basic Quad shaderDict nullptr for key " << key << "\n";
			}
		}, value);
	}
	
	glDrawArrays(GL_TRIANGLES, 0, (GLsizei)this->vertices.size());
	glActiveTexture(GL_TEXTURE0);
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "BasicQuad::Render 2 error: " << glerr << std::endl;
	}
	return;
}
