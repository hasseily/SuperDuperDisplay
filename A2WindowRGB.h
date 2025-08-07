#pragma once
#ifndef A2WINDOWRGB_H
#define A2WINDOWRGB_H

#include <vector>
#include "common.h"
#include "shader.h"

// Objects of this class render memory using legacy RGB shaders
// Included is an ImGui interface for visualizing the render and setting parameters
// Call the Render() method from the main rendering loop
// Call DisplayImGuiWindow() from the ImGui area of the main loop

enum A2VideoModeRGB_e
{
	A2VIDEORGB_TEXT,
	A2VIDEORGB_LGR,
	A2VIDEORGB_HGR,
	A2VIDEORGB_DTEXT,
	A2VIDEORGB_DLGR,
	A2VIDEORGB_DHGR,
	A2VIDEORGB_COUNT
};

class A2WindowRGB
{
public:
	const A2VideoModeRGB_e GetVideoMode() { return videoMode; };
	void SetVideoMode(A2VideoModeRGB_e _videoMode);
	void Render();
	void DisplayImGuiWindow();
	A2WindowRGB();
	~A2WindowRGB();

	int memStart;							// Where to start in memory
	bool memAux;							// Use AUX mem instead of MAIN (disabled when doubleMode == true)
	bool bImguiWindowIsOpen;				// for ImGUI, window is open
private:
	A2VideoModeRGB_e videoMode;				// Video mode to use
	Shader shader;							// Shader used
	bool doubleMode = false; 				// DTEXT, DLGR, DHGR
	uXY screen_count = { 0,0 };				// width,height in pixels of visible screen area of window

	SDL_FRect quad;							// x, y, width, height

	std::vector<A2RenderVertex> vertices;	// Vertices with XYRelative and XYPixels
	unsigned int VAO = 0;	// Vertex Array Object (holds buffers that are vertex related)
	unsigned int VBO = 0;	// Vertex Buffer Object (holds vertices)
	GLuint FBO = 0;
	GLuint texture_id = 0;

	void UpdateVertexArray();
};

#endif // A2WINDOWRGB_H
