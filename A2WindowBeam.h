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
	bool IsEnabled() const { return enabled; }
	void SetEnabled(bool val) { 
		enabled = val;
	}
	bool IsDefined() const { return defined; }

	bool bNeedsGPUVertexUpdate = true;	// Update the GPU if the vertex data has changed

	A2WindowBeam()
		: enabled(false)
		, defined(false)
		, video_mode(A2VIDEOBEAM_LEGACY)
		, shaderProgram(nullptr)
	{
		// Assign the vertex array.
		// The first 2 values are the relative XY, bound from -1 to 1.
		// The A2WindowBeam always covers the whole screen, so from -1 to 1 on both axes
		// The second pair of values is the actual pixel value on screen (560x384, etc...)
		// Set them to 0, they'll be defined later from the video manager
		vertices.push_back(A2BeamVertex({glm::vec2(-1,  1), glm::ivec2(0, 0)}));	// top left
		vertices.push_back(A2BeamVertex({glm::vec2( 1, -1), glm::ivec2(0, 0)}));	// bottom right
		vertices.push_back(A2BeamVertex({glm::vec2( 1,  1), glm::ivec2(0, 0)}));	// top right
		vertices.push_back(A2BeamVertex({glm::vec2(-1,  1), glm::ivec2(0, 0)}));	// top left
		vertices.push_back(A2BeamVertex({glm::vec2(-1, -1), glm::ivec2(0, 0)}));	// bottom left
		vertices.push_back(A2BeamVertex({glm::vec2( 1, -1), glm::ivec2(0, 0)}));	// bottom right
	};
	~A2WindowBeam();
	void Define(A2VideoModeBeam_e video_mode, Shader* _shaderProgram);
	void Render(bool shouldUpdateDataInGPU);

	Shader* GetShaderProgram() { return shaderProgram; };
	void SetShaderProgram(Shader* _shader) { shaderProgram = _shader; };
	A2VideoModeBeam_e Get_video_mode() const { return video_mode; }

private:
	void Reset();

	bool defined;           		// if not defined, can't be used
    bool enabled;           		// if not enabled, doesn't get rendered
	A2VideoModeBeam_e video_mode;	// Which video mode is used
	Shader* shaderProgram;			// Shader used
	uXY screen_count = {0,0};				// width,height in pixels of visible screen area of window

	unsigned int VRAMTEX = UINT_MAX;		// VRAM buffer texture. Holds R as MAIN, G and AUX, B as flags

	std::vector<A2BeamVertex> vertices;		// Vertices with XYRelative and XYPixels
	unsigned int VAO = UINT_MAX;			// Vertex Array Object (holds buffers that are vertex related)
	unsigned int VBO = UINT_MAX;			// Vertex Buffer Object (holds vertices)
};

#endif // A2WINDOWBEAM_H
