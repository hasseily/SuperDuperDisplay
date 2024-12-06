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
	A2VIDEOBEAM_FORCED_TEXT1,
	A2VIDEOBEAM_FORCED_TEXT2,
	A2VIDEOBEAM_FORCED_HGR1,
	A2VIDEOBEAM_FORCED_HGR2,
	A2VIDEOBEAM_TOTAL_COUNT
};

// Special less compatible modes from lesser known cards
// like Chat Mauve RGB cards, or Apple RGB card
// and the new SHR4 modes for VidHD and Appletini
enum A2VideoSpecialMode_e
{
	A2_VSM_NONE 			= 0b0000'0000,
	A2_VSM_DHGRCOL140Mixed 	= 0b0000'0001,	// Mode that mixes 560 wide B/W alongside 160 wide DHGR color
	A2_VSM_HGRSPEC1			= 0b0000'0010,	// Mode that forces black in middle pixel of 11011 pattern in HGR
	A2_VSM_HGRSPEC2		 	= 0b0000'0100,	// Mode that forces white in middle pixel of 00100 pattern in HGR
	
	A2_VSM_SHR4SHR			= 0b0001'0000,	// New SHR4 modes - default SHR but with 'magic bytes' active
	A2_VSM_SHR4RGGB			= 0b0010'0000,	// New SHR4 modes - RGGB   (see shader for details)
	A2_VSM_SHR4PAL256		= 0b0100'0000,	// New SHR4 modes - PAL256 (see shader for details)
	A2_VSM_SHR4R4G4B4		= 0b1000'0000,	// New SHR4 modes - r4G4B4 (see shader for details)
};

// Monitor color type
enum A2VideoMonitorType_e
{
	A2_MON_COLOR = 0,
	A2_MON_WHITE,
	A2_MON_GREEN,
	A2_MON_AMBER,
	A2_MON_TOTAL_COUNT
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

	std::vector<A2BeamVertex> vertices;		// Vertices with XYRelative and XYPixels
	unsigned int VAO = UINT_MAX;			// Vertex Array Object (holds buffers that are vertex related)
	unsigned int VBO = UINT_MAX;			// Vertex Buffer Object (holds vertices)
	
	int specialModesMask = A2_VSM_NONE;		// Or'ed A2VideoSpecialMode_e
	int overrideSHR4Mode = 0;				// Debugging to override the SHR4 modes in the shader
	int interlaceSHRMode = 0;				// Interlacing? 0: off, 1: on
	int monitorColorType = A2_MON_COLOR;	// Monitor color type A2VideoMonitorType_e

private:
	bool vramTextureExists = false;						// true if the VRAM texture exists and only needs an update
	A2VideoModeBeam_e video_mode = A2VIDEOBEAM_LEGACY;	// Which video mode is used
	Shader shader = Shader();							// Shader used
	uXY screen_count = {0,0};				// width,height in pixels of visible screen area of window

	unsigned int VRAMTEX = UINT_MAX;		// GL_R8UI VRAM buffer texture. Format depends on legacy or SHR mode
	unsigned int PAL256TEX = UINT_MAX;		// GL_R16UI Special VRAM for SHR4 PAL256 mode

	uint32_t border_width_cycles = 0;
	uint32_t border_height_scanlines = 0;

	GLuint output_texture_id = UINT_MAX;	// the output texture for this object
	GLuint FBO = UINT_MAX;					// the framebuffer for this object

	void UpdateVertexArray();
};

#endif // A2WINDOWBEAM_H
