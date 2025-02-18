//
//  VidHdModesManager.h
//  SuperDuperDisplay
//
//  Created by Henri Asseily on 27/01/2025.
//

// This class renders the VidHD high resolution text modes,
// which always use an entire 1920x1080 display with a 16:9 aspect ratio.
// Each instance has its own self-contained "video memory" buffer to write
// to for displaying of the text.
//
// When these modes are active, the border is disabled. These modes support
// text foreground and background color. Text is written in these modes via
// the custom COUT routine in the VidHD BIOS.
//
// The Appletini implementation does the modifications of CH and CV
// zero-page values in hardware. This class renders based on the state of
// the zero-page values.
//
// When switching between modes, data in the common area is kept. Data in
// the cropped area is deleted. So for example if you switch from TEXT_120X67
// to TEXT_80X24, all lines beyond 24 are erased, and so are columns beyond 80.
// If you later switch back to TEXT_120X67, the erased areas will show up clear.

#pragma once

#ifndef VIDHDWINDOWBEAM_H
#define VIDHDWINDOWBEAM_H

#include "shader.h"
#include <stdio.h>

// The 40X24 and 80X24 modes are special. They're like the standard D/TEXT except
// that they're rendered in the full 1920x1080 frame. So we double their pixel size
// and apply transparent borders of 400px wide and 156px high to center the text
// in the frame.
// VidHD doesn't do it this way and reverts to the standard rendering of D/TEXT but
// having these special modes here allows us to render them as overlays on top
// of the A2Video output.
// TODO: Remove these 2 modes and default to the legacy A2Video when they're active?

// There is a single additional font which is 8x8 and used for 16x16 and 24x24
// The 14x32 just uses the even columns of the 28x32 font, just like the 80COL mode
enum VidHdMode_e
{
	VIDHDMODE_NONE = 0,
	VIDHDMODE_TEXT_40X24,		// 28x32 font (transparent H borders 400px, V borders 156px)
	VIDHDMODE_TEXT_80X24,		// 14x32 font (transparent H borders 400px, V borders 156px)
	VIDHDMODE_TEXT_80X45,		// 24x24 font
	VIDHDMODE_TEXT_120X67,		// 16x16 font
	VIDHDMODE_TEXT_240X135,		// 8x8 font
	VIDHDMODE_TOTAL_COUNT
};

struct VidHdBeamVertex {
	glm::vec2 RelPos;		// Relative position of the vertex
	glm::vec2 PixelPos;		// Pixel position of the vertex
};

// VidHD non-classic text modes uses a fixed 1920x1080 resolution
constexpr uint32_t _VIDHDMODES_PIXEL_WIDTH = 1920;
constexpr uint32_t _VIDHDMODES_PIXEL_HEIGHT = 1080;
constexpr uint32_t _VIDHDMODES_TEXT_WIDTH = 240;
constexpr uint32_t _VIDHDMODES_TEXT_HEIGHT = 135;

// The VidHD modes VRAM should be a 240x135 matrix. We'll use RGBA even though
// that's overkill.
// R will be the 7-bit ASCII value of the text.
// G will be the FG+BG color, as in the Apple 2gs and our legacy VRAM's A byte
// B is reserved for later
// A is used as standard A, transparency

#pragma pack(push, 1)
struct VidHdVramTextEntry {	// 4 byte struct
	uint8_t character;  // Byte 0 (R): Text value

	// Byte 1 (G): Colors
	union {
		uint8_t color;  // Access the whole byte if needed
		struct {
			uint8_t backgroundColor : 4; // Lower 4 bits
			uint8_t foregroundColor : 4; // Upper 4 bits
		};
	};

	uint8_t unused;  // Byte 2 (B): Unused

	// Byte 3 (A): Transparency
	union {
		uint8_t alpha;
		struct {
			uint8_t backgroundAlpha : 4; // Lower 4 bits
			uint8_t foregroundAlpha : 4; // Upper 4 bits
		};
	};
};
#pragma pack(pop)

class VidHdWindowBeam {
public:
	VidHdWindowBeam(VidHdMode_e _video_mode);
	~VidHdWindowBeam();
	void WriteCharacter(uint8_t hpos, uint8_t vpos, uint8_t value);	// COUT
	uint8_t ReadCharacter(uint8_t hpos, uint8_t vpos);				// for completeness
	void SetAlpha(uint8_t alpha);	// Sets future writes' transparency
	uint8_t GetAlpha() { return textAlpha; };
	void SetVideoMode(VidHdMode_e mode);
    VidHdMode_e GetVideoMode() const { return video_mode; }
	uint32_t GetWidth() const;
	uint32_t GetHeight() const;
	void SetQuadRelativeBounds(SDL_FRect bounds);
	SDL_FRect GetQuadRelativeBounds() const { return quad; };
	void Render(GLuint inputTexUnit, glm::vec2 inputSize);
	void DisplayImGuiWindow(bool* p_open);

	std::vector<VidHdBeamVertex> vertices;	// Vertices with XYRelative and XYPixels
	unsigned int VAO = UINT_MAX;			// Vertex Array Object (holds buffers that are vertex related)
	unsigned int VBO = UINT_MAX;			// Vertex Buffer Object (holds vertices)

private:
	bool bVramTextureExists = false;				// true if the VRAM texture exists and only needs an update
	VidHdMode_e video_mode = VIDHDMODE_TOTAL_COUNT;	// Which video mode is used
	Shader shader = Shader();					// Shader used
	uXY screen_count = {_VIDHDMODES_PIXEL_WIDTH,_VIDHDMODES_PIXEL_HEIGHT};	// width,height in pixels of visible screen area of window

	unsigned int VRAMTEX = UINT_MAX;		// GL_R8UI VRAM buffer texture.

	uint32_t* vram_text;					// 240x135 characters
											// byte 0: text value
											// byte 1: fore and background colors, as specified in the C022 softswitch
											// byte 2: unused for now
											// byte 3: fore and background transparency levels (16 levels of transparency)

	uint8_t textAlpha = 0xFF;				// Fore and Back alpha (high and low nibble)
	// Current mode information
	bool bModeDidChange = true;
	glm::ivec2 modeSize = glm::ivec2(0, 0);	// Row and Column count
	int fontTex = _TEXUNIT_IMAGE_ASSETS_START + 0 - GL_TEXTURE0;
	glm::uvec2 glyphSize = glm::uvec2(14,16);
	glm::vec2 fontScale = glm::vec2(2.0,2.0);	// Font size should be 16x16
	SDL_FRect quad = { -1.f, 1.f, 2.f, -2.f };	// x, y, width, height

	void UpdateVertexArray();
};

#endif /* VIDHDWINDOWBEAM_H */
