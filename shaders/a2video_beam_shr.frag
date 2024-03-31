#ifdef GL_ES
#define COMPAT_PRECISION mediump
precision mediump float;
precision highp usampler2D;
precision highp int;
#else
#define COMPAT_PRECISION
layout(pixel_center_integer) in vec4 gl_FragCoord;
#endif

/*
 Apple 2gs video beam shader for SHR.
 TODO: Fix based on new vram format
 
 This shader expects as input a VRAMTEX texture that is a GL_R8UI byte buffer.
 It is a series of 193 byte lines.
 On each line:
 - the first byte is the SCB of the line
 - the next 32 bytes is the color palette (16 colors of 2 bytes each)
 - the last 160 bytes are the SHR scanline byte

 The SCB and palette are loaded at the start of the line by the beam generator,
 so they are always fixed for the line. This has been verified on original hardware
 by a number of people.
 
 As a reminder, the SCB has the following bits:
 Bits 0-3 Palette number
 Bit 4 Reserved (defaults to 0)
 Bit 5 Mode fill (1=on, 0=off)
 Bit 6 Interrupt (1=requested, 0=not requested)
 Bit 7 Horizontal pixel count (0=320px, 1=640px)
 
 We set bit 4 of the SCB in the GPU to state if the line is unused.
 If so, then we make the whole line transparent.
 
 Also note that the colorfill situation has been precalculated when generating the VRAM.
 
 The shader goes through the following phases:
 - The fragment determines which VRAMTEX texel it's part of, including the x offset
 to the start of the texel (there is no y offset, each byte is on one scanline).
 - It grabs the texel and determines the video mode to use
 - It runs the video mode code on that byte and chooses the correct fragment

 */


// Global uniforms assigned in A2VideoManager
uniform int ticks;              // ms since start
uniform int hborder;			// horizontal border in cycles
uniform int vborder;			// vertical border in scanlines
uniform usampler2D VRAMTEX;		// Video RAM texture

in vec2 vFragPos;       // The fragment position in pixels
out vec4 fragColor;

bool is640Mode = false;
bool isColorFill = false;	// unused, colorfill is handled by the CPU code
uint paletteColorB1 = 0u;	// first byte of the palette color
uint paletteColorB2 = 0u;	// second byte of the palette color

// This is reversed because we reversed the fragOffset for
// faster calculation of color values
const uint palette640[16] = uint[16](
									 4u,5u,6u,7u,
									 0u,1u,2u,3u,
									 12u,13u,14u,15u,
									 8u,9u,10u,11u
								   );

const vec4 bordercolors[16] = vec4[16] (
    vec4(0.00, 0.00, 0.00, 1.0), // BLACK
    vec4(0.67, 0.07, 0.30, 1.0), // DEEP_RED
    vec4(0.00, 0.03, 0.51, 1.0), // DARK_BLUE
    vec4(0.67, 0.10, 0.82, 1.0), // MAGENTA
    vec4(0.00, 0.51, 0.18, 1.0), // DARK_GREEN
    vec4(0.62, 0.59, 0.49, 1.0), // DARK_GRAY
    vec4(0.00, 0.54, 0.71, 1.0), // BLUE
    vec4(0.62, 0.62, 1.00, 1.0), // LIGHT_BLUE
    vec4(0.48, 0.37, 0.00, 1.0), // BROWN
    vec4(1.00, 0.45, 0.28, 1.0), // ORANGE
    vec4(0.47, 0.41, 0.49, 1.0), // LIGHT_GRAY
    vec4(1.00, 0.48, 0.81, 1.0), // PINK
    vec4(0.43, 0.90, 0.17, 1.0), // GREEN
    vec4(1.00, 0.96, 0.48, 1.0), // YELLOW
    vec4(0.42, 0.93, 0.70, 1.0), // AQUA
    vec4(1.00, 1.00, 1.00, 1.0)  // WHITE
);

vec4 ConvertIIgs2RGB(uint gscolor)
{
	float _red = float((gscolor & 0x0F00u) >> 8);		// 0000 1111 0000 0000
	float _green = float((gscolor & 0x00F0u) >> 4);		// 0000 0000 1111 0000
	float _blue = float(gscolor & 0x000Fu);				// 0000 0000 0000 1111
	float _alpha = 1.0; 								// Fully opaque
	
	// They're 4 bits. Normalize them to 1.0
	_red /= 16.0;
	_green /= 16.0;
	_blue /= 16.0;
	return vec4(_red, _green, _blue, _alpha);
}

void main()
{
	uint scanline = uint(vFragPos.y) / 2u;

	// first do the borders
	if ((vFragPos.y < float(vborder*2)) || (vFragPos.y >= float(vborder*2+400)) || 
		(vFragPos.x < float(hborder*16)) || (vFragPos.x >= float(640+hborder*16)))
	{
		fragColor = bordercolors[texelFetch(VRAMTEX, ivec2(33u + uint(float(vFragPos.x) / 4.0), scanline), 0).r & 0x0Fu];
		return;
	}

	// grab the the scb
	uint scb = texelFetch(VRAMTEX, ivec2(0, scanline), 0).r;
	
	// Parse the useful scb information
	is640Mode = bool(scb & 0x80u);
	// isColorFill = bool(scb & 0x20u);	// unused, already handled when generating VRAM
	if (bool(scb & 0x10u))		// if the CPU told us this line is unused, set the pixel to transparent
	{
		fragColor = vec4(0.0,0.0,0.0,0.0);
		return;
	}
	
	// Determine the byte and the pixel for this byte
	uint xpos = uint(vFragPos.x);
	uint fragOffset = 3u - (xpos % 4u);			// reversed so that palette calc is easier
	
	// Grab the scanline color byte value
	// The scanline color byte value gives the color for either 4 dots in 640 mode,
	// or 2 doubled dots in 320 mode
	// REMINDER: we're working on dots, with 640 dots per line. And lines are doubled
	uint byteVal = texelFetch(VRAMTEX, ivec2(33u + uint(float(xpos) / 4.0), scanline), 0).r;

	uint colorIdx = 0u;
	
	if (is640Mode)
	{
		colorIdx = palette640[(fragOffset * 4u) + ((byteVal >> (2u * fragOffset)) & 0x3u)];
	}
	else
	{
		colorIdx = (byteVal >> (4u * (fragOffset/2u))) & 0xFu;
	}
	// Get the palette color
	paletteColorB1 = texelFetch(VRAMTEX, ivec2(1u + colorIdx*2u, scanline), 0).r;
	paletteColorB2 = texelFetch(VRAMTEX, ivec2(1u + colorIdx*2u + 1u, scanline), 0).r;
	fragColor = ConvertIIgs2RGB((paletteColorB2 << 8) + paletteColorB1);
}
