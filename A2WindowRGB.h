#pragma once
#ifndef A2WINDOWRGB_H
#define A2WINDOWRGB_H

#include <vector>
#include "common.h"
#include "shader.h"

// Objects of this class render memory using RGB shaders
// Included is an ImGui interface for visualizing the render and setting parameters
// Call the Render() method from the main rendering loop
// Call DisplayImGuiWindow() from the ImGui area of the main loop

// There's no beam racing here. When render is called, the instance uses the Apple 2 Memory Texture
// to read the memory snapshot and generates the image. It's up to the caller to update the A2MT
// in _TEXUNIT_APPLE2MEMORY_R8UI before calling Render()

enum A2VideoModeRGB_e
{
	A2VIDEORGB_TEXT,
	A2VIDEORGB_LGR,
	A2VIDEORGB_HGR,
	A2VIDEORGB_DTEXT,
	A2VIDEORGB_DLGR,
	A2VIDEORGB_DHGR,
	A2VIDEORGB_DHGR160,
	A2VIDEORGB_SHR,
	A2VIDEORGB_COUNT
};

class A2WindowRGB
{
public:
	A2WindowRGB(bool useFBO = true);		// Set to true to render to the main backbuffer instead of a FBO
	A2WindowRGB() : A2WindowRGB(true) {}	// Defaults to using an FBO
	void SetBorder(uint32_t cycles_horizontal, uint32_t scanlines_vertical);
	uXY GetBorder();
	uint32_t GetWidth() const;
	uint32_t GetHeight() const;
	const A2VideoModeRGB_e GetVideoMode() { return videoMode; };
	void SetVideoMode(A2VideoModeRGB_e _videoMode);
	void Render();
	void DisplayImGuiWindow();
	~A2WindowRGB();

	int memStart;							// Where to start in memory
	bool memAux;							// Use AUX mem instead of MAIN (disabled when doubleMode == true)
	glm::vec4 borderColor = glm::vec4(0);
	bool bImguiWindowIsOpen;				// for ImGUI, window is open
private:
	A2VideoModeRGB_e videoMode;				// Video mode to use
	Shader shader;							// Shader used
	bool doubleMode = false; 				// DTEXT, DLGR, DHGR
	uXY screen_count = { 0,0 };				// width,height in pixels of visible screen area of window
	uint32_t border_lr_pixels = 0;			// # of pixels of left/right border
	uint32_t border_tb_pixels = 0;		// # of pixels of top/bottom border

	SDL_FRect quad;							// x, y, width, height

	std::vector<A2RenderVertex> vertices;	// Vertices with XYRelative and XYPixels
	unsigned int VAO = 0;	// Vertex Array Object (holds buffers that are vertex related)
	unsigned int VBO = 0;	// Vertex Buffer Object (holds vertices)
	GLuint FBO = 0;
	GLuint texture_id = 0;

	void UpdateVertexArray();
};

#endif // A2WINDOWRGB_H
