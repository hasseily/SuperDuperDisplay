#pragma once
#ifndef A2WINDOWBEAM_H
#define A2WINDOWBEAM_H

#include <vector>
#include "shader.h"

#define _A2_TEXT80_CHAR_WIDTH 7
#define _A2_TEXT80_CHAR_HEIGHT 16
#define _A2_TEXT40_CHAR_WIDTH _A2_TEXT80_CHAR_WIDTH*2
#define _A2_TEXT40_CHAR_HEIGHT _A2_TEXT80_CHAR_HEIGHT

enum A2VideoModeBeam_e
{
	A2VIDEOBEAM_LEGACY,
	A2VIDEOBEAM_SHR,
	A2VIDEOBEAM_TOTAL_COUNT
};

struct A2BeamVertex {
	glm::vec2 RelPos;		// Relative position of the vertex
	glm::vec2 PixelPos;		// Pixel position of the vertex in the Apple 2 screen
};

class A2WindowBeam
{
public:

	A2WindowBeam(A2VideoModeBeam_e _video_mode, const char* shaderVertexPath, const char* shaderFragmentPath);
	~A2WindowBeam();
	const uint32_t GetWidth();
	const uint32_t GetHeight();
	void SetBorder(uint32_t cycles_horizontal, uint32_t scanlines_vertical);
	GLuint GetOutputTextureId();
	GLuint Render(bool shouldUpdateDataInGPU);	// returns the output texture id

	Shader* GetShader() { return &shader; };
	void SetShaderPrograms(const char* shaderVertexPath, const char* shaderFragmentPath);
	A2VideoModeBeam_e Get_video_mode() const { return video_mode; }

private:
	bool vramTextureExists = false;						// true if the VRAM texture exists and only needs an update
	A2VideoModeBeam_e video_mode = A2VIDEOBEAM_LEGACY;	// Which video mode is used
	Shader shader = Shader();							// Shader used
	uXY screen_count = {0,0};				// width,height in pixels of visible screen area of window

	unsigned int VRAMTEX = UINT_MAX;		// VRAM buffer texture. Holds R as MAIN, G and AUX, B as flags

	std::vector<A2BeamVertex> vertices;		// Vertices with XYRelative and XYPixels
	unsigned int VAO = UINT_MAX;			// Vertex Array Object (holds buffers that are vertex related)
	unsigned int VBO = UINT_MAX;			// Vertex Buffer Object (holds vertices)

	uint32_t border_width_cycles = 0;
	uint32_t border_height_scanlines = 0;

	GLuint output_texture_id = UINT_MAX;	// the output texture for this object
	GLuint FBO = UINT_MAX;					// the framebuffer for this object

	void UpdateVertexArray();
};

#endif // A2WINDOWBEAM_H
