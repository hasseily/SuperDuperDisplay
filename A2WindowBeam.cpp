#include "A2WindowBeam.h"
#include "common.h"
#include <SDL_timer.h>
#include "A2VideoManager.h"
#include "SDHRManager.h"

void A2WindowBeam::Reset()
{
	SetEnabled(false);
}

A2WindowBeam::~A2WindowBeam()
{
	if (VRAMTEX != UINT_MAX)
		glDeleteTextures(1, &VRAMTEX);

	if (VAO != UINT_MAX)
	{
		glDeleteVertexArrays(1, &VAO);
		glDeleteBuffers(1, &VBO);
	}
}

void A2WindowBeam::Define(A2VideoModeBeam_e _video_mode, Shader* _shaderProgram)
{
	this->Reset();
	video_mode = _video_mode;
	shaderProgram = _shaderProgram;

	switch (video_mode) {
	case A2VIDEOBEAM_LEGACY:
		screen_count = uXY({ 560u , 384u });
		break;
	case A2VIDEOBEAM_SHR:
		screen_count = uXY({ 640u , 400u });
		break;
	default:
		break;

	}
	vertices[0].PixelPos = glm::vec2(0				, screen_count.y);	// top left
	vertices[1].PixelPos = glm::vec2(screen_count.x	, 0				);	// bottom right
	vertices[2].PixelPos = glm::vec2(screen_count.x	, screen_count.y);	// top right
	vertices[3].PixelPos = glm::vec2(0				, screen_count.y);	// top left
	vertices[4].PixelPos = glm::vec2(0				, 0				);	// bottom left
	vertices[5].PixelPos = glm::vec2(screen_count.x	, 0				);	// bottom right

	bNeedsGPUVertexUpdate = true;

}

void A2WindowBeam::Render()
{
	if (!IsEnabled())
		return;
	if (shaderProgram == nullptr)
		return;
	if (vertices.size() == 0)
		return;

	GLenum glerr;
	if (VRAMTEX == UINT_MAX)
		glGenTextures(1, &VRAMTEX);

	if (VAO == UINT_MAX)
	{
		glGenVertexArrays(1, &VAO);
		glGenBuffers(1, &VBO);
	}

	glBindVertexArray(VAO);

	if (bNeedsGPUVertexUpdate)
	{
		// load data into vertex buffers
		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(A2BeamVertex), &vertices[0], GL_STATIC_DRAW);

		// set the vertex attribute pointers
		// vertex relative Positions: position 0, size 2
		// (vec4 values x and y)
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(A2BeamVertex), (void*)0);
		// vertex pixel Positions: position 1, size 2
		// (vec4 values z and w)
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(A2BeamVertex), (void*)offsetof(A2BeamVertex, PixelPos));
	}

	// Associate the texture VRAMTEX in GL_TEXTURE0+_SDHR_TBO_TEXUNIT with the buffer
	// This is the apple 2's memory which is mapped to a "texture"
	// Always update that buffer in the GPU
	glActiveTexture(GL_TEXTURE0 + _SDHR_TBO_TEXUNIT);
	glBindTexture(GL_TEXTURE_2D, VRAMTEX);
	switch (video_mode) {
		case A2VIDEOBEAM_LEGACY:
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8UI, 40, 192, 0, GL_RGB_INTEGER, GL_UNSIGNED_BYTE, A2VideoManager::GetInstance()->GetLegacyVRAMPtr());
			break;
		case A2VIDEOBEAM_SHR:
			glTexImage2D(GL_TEXTURE_2D, 0, GL_R8UI, (1+32+160), 200, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, A2VideoManager::GetInstance()->GetSHRVRAMPtr());
			break;
		default:
			break;
	}
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glBindTexture(GL_TEXTURE_2D, 0);


	// reset the binding
	glBindVertexArray(0);

	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "A2WindowBeam::Update error: " << glerr << std::endl;
	}

	bNeedsGPUVertexUpdate = false;

	shaderProgram->use();
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL A2Video glUseProgram error: " << glerr << std::endl;
		return;
	}

	glBindVertexArray(VAO);

	shaderProgram->setInt("ticks", SDL_GetTicks());

	// point the uniform at the VRAM texture
	glActiveTexture(GL_TEXTURE0 + _SDHR_TBO_TEXUNIT);
	glBindTexture(GL_TEXTURE_2D, VRAMTEX);
	shaderProgram->setInt("VRAMTEX", _SDHR_TBO_TEXUNIT);
	// back to the output buffer to draw our scene
	glActiveTexture(GL_TEXTURE0);
	glDrawArrays(GL_TRIANGLES, 0, (GLsizei)this->vertices.size());
	glBindVertexArray(0);
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "A2WindowBeam render error: " << glerr << std::endl;
	}
}


