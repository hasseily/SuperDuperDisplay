#pragma once
#ifndef A2WINDOWBEAM_H
#define A2WINDOWBEAM_H

#include <vector>
#include "shader.h"

#define _A2_TEXT80_CHAR_WIDTH 7
#define _A2_TEXT80_CHAR_HEIGHT 16
#define _A2_TEXT40_CHAR_WIDTH _A2_TEXT80_CHAR_WIDTH*2
#define _A2_TEXT40_CHAR_HEIGHT _A2_TEXT80_CHAR_HEIGHT

// Those could be anywhere up to 6 or 7 cycles for horizontal borders
// and a lot more for vertical borders. We just decided on a size
// But SHR starts VBLANK just like legacy modes, at scanline 192. Hence
// it has 8 less bottom border scanlines than legacy.
#define _A2_BORDER_WIDTH_CYCLES 5
#define _A2_BORDER_HEIGHT_SCANLINES 70

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

	bool bNeedsGPUVertexUpdate = true;	// Update the GPU if the vertex data has changed
	A2WindowBeam(A2VideoModeBeam_e _video_mode, Shader* _shaderProgram);


	~A2WindowBeam();
	const uint32_t GetWidth();
	const uint32_t GetHeight();
	void SetBorder(uint32_t cycles_horizontal, uint32_t scanlines_vertical);
	GLuint GetOutputTextureId();
	void Render(bool shouldUpdateDataInGPU);

	Shader* GetShaderProgram() { return shaderProgram; };
	void SetShaderProgram(Shader* _shader) { shaderProgram = _shader; };
	A2VideoModeBeam_e Get_video_mode() const { return video_mode; }

private:
	bool vramTextureExists = false;						// true if the VRAM texture exists and only needs an update
	A2VideoModeBeam_e video_mode = A2VIDEOBEAM_LEGACY;	// Which video mode is used
	Shader* shaderProgram = nullptr;					// Shader used
	uXY screen_count = {0,0};				// width,height in pixels of visible screen area of window

	unsigned int VRAMTEX = UINT_MAX;		// VRAM buffer texture. Holds R as MAIN, G and AUX, B as flags

	std::vector<A2BeamVertex> vertices;		// Vertices with XYRelative and XYPixels
	unsigned int VAO = UINT_MAX;			// Vertex Array Object (holds buffers that are vertex related)
	unsigned int VBO = UINT_MAX;			// Vertex Buffer Object (holds vertices)

	uint32_t border_width_cycles = 0;
	uint32_t border_height_scanlines = 0;

	GLuint output_texture_id;	// the output texture for this object
	GLuint FBO = UINT_MAX;		// the framebuffer for this object

	void UpdateVertexArray();
};

#endif // A2WINDOWBEAM_H
