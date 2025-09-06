#pragma once
#ifndef A2WINDOWBEAM_H
#define A2WINDOWBEAM_H

#include <vector>
#include "common.h"
#include "shader.h"

#define _A2_TEXT80_CHAR_WIDTH 7
#define _A2_TEXT80_CHAR_HEIGHT 16
#define _A2_TEXT40_CHAR_WIDTH _A2_TEXT80_CHAR_WIDTH*2
#define _A2_TEXT40_CHAR_HEIGHT _A2_TEXT80_CHAR_HEIGHT

// Any of the below has its own instance of the class
// because the canvas sizes are unique for each
enum A2VideoModeBeam_e
{
	A2VIDEOBEAM_LEGACY,			// 560x384 + borders
	A2VIDEOBEAM_SHR,			// 640x400 + borders
	A2VIDEOBEAM_TOTAL_COUNT
};

// Special less compatible modes from lesser known cards
// like Chat Mauve RGB cards, or Apple RGB card.
// Those are "mods" to the modes, such as using the ALT charset for TEXT
// Use enum instead of enum class because this stuff is used all the time in integer context.
// NOTE: the special modes need to be in the second nibble, to match the VRAM flag byte high nibble
enum A2ESpecialMode_e	// 8 bits only, 4 upper bits used
{
	A2ESM_NONE 			= 0b0000'0000,
	A2ESM_TEXTALT 		= 0b0001'0000,	// ALT charset for TEXT
	A2ESM_HGRSPEC1		= 0b0001'0000,	// Mode that forces black in middle pixel of 11011 pattern in HGR
	A2ESM_HGRSPEC2		= 0b0010'0000,	// Mode that forces white in middle pixel of 00100 pattern in HGR
	A2ESM_DHGRCOL140M 	= 0b0001'0000,	// Mode that mixes 560 wide B/W alongside 140 wide DHGR color
};

// SHR special modes. These are unique to modern cards such as the Appletini
// where video processing of the modes in real time is very costly (if not impossible)
// on original hardware. The hardware only needs to put the correct data in memory,
// and SDD will generate the improved image through its shader. Typically the original
// hardware may output a simplified grayscale version of the image through judiciously
// chosen grayscale palettes.
// Use enum instead of enum class because this stuff is used all the time in integer context.
// NOTE: the SHR4 modes need to be in the second nibble, to match the SHR palette 2nd byte's high nibble
enum A2SHRSpecialMode_e
{
	A2SM_NONE 				= 0b0000,
	A2SM_SHR3200			= 0b0001,		// SHR 3200 mode ("Brooks-3200")

	A2SM_SHR4SHR			= 0b0001'0000,	// New SHR4 modes - default SHR but with 'magic bytes' active
	A2SM_SHR4RGGB			= 0b0011'0000,	// New SHR4 modes - RGGB   (see shader for details)
	A2SM_SHR4PAL256			= 0b0101'0000,	// New SHR4 modes - PAL256 (see shader for details)
	A2SM_SHR4R4G4B4			= 0b1001'0000,	// New SHR4 modes - R4G4B4 (see shader for details)
};

// Special paged modes that use both E0 and E1 banks
// For SHR, it uses E1$2000 for the main page, and E0$2000 for the alternate page
// For legacy, it uses $2000 for the main page, and $4000 for the alternate page
enum DoubleMode_e
{
	DOUBLE_NONE = 0,
	DOUBLE_INTERLACE,
	DOUBLE_PAGEFLIP,
	DOULBE_TOTAL_COUNT
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

class A2WindowBeam
{
public:

	A2WindowBeam(A2VideoModeBeam_e _video_mode, const char* shaderVertexPath, const char* shaderFragmentPath);
	~A2WindowBeam();
	uint32_t GetWidth() const;
	uint32_t GetHeight() const;
	void SetBorder(uint32_t cycles_horizontal, uint32_t scanlines_vertical);
	void SetQuadRelativeBounds(SDL_FRect bounds);
	SDL_FRect GetQuadRelativeBounds() const { return quad; };
	void Render(uint64_t frame_idx);

	Shader* GetShader() { return &shader; };
	void SetShaderPrograms(const char* shaderVertexPath, const char* shaderFragmentPath);
	A2VideoModeBeam_e Get_video_mode() const { return video_mode; }

	std::vector<A2RenderVertex> vertices;	// Vertices with XYRelative and XYPixels
	unsigned int VAO = UINT_MAX;			// Vertex Array Object (holds buffers that are vertex related)
	unsigned int VBO = UINT_MAX;			// Vertex Buffer Object (holds vertices)

	bool bIsMergedMode = false;				// Activate if this frame has both SHR and Legacy. Shader will use OFFSETEX
	bool bForceSHRWidth = false;			// Request to force SHR width for legacy

	int specialModesMask = A2SM_NONE;		// Or'ed A2SHRSpecialMode_e
	int doubleSHR4 = DOUBLE_NONE;
	int pagingMode = DOUBLE_NONE;			// Override of paging for legacy
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

	SDL_FRect quad = { -1.f, 1.f, 2.f, -2.f };	// x, y, width, height

	void UpdateVertexArray();
#ifdef DEBUG
	void DebugTextureBindings(GLuint program);
#endif
};

#endif // A2WINDOWBEAM_H
