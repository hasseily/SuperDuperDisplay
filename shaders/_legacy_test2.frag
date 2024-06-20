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
Apple 2 video beam shader for legacy modes (not SHR).

This shader expects as input a VRAMTEX texture that has the following features:
- Type GL_RGB8UI, which is 4 bytes for each texel
- Color R is the MAIN memory byte
- Color G is the AUX memory byte
- Color B is 8 bits of state, including the graphics mode and soft switches
- Color A is the fore and background colors, as specified in the C022 softswitch
- 40 pixels wide, where MAIN and AUX are interleaved, starting with AUX
- 192 lines high, which is the Apple 2 scanlines

The flags byte (Color B) is:
	bits 0-2: mode (TEXT, DTEXT, LGR, DLGR, HGR, DHGR, DHGRMONO, BORDER)
	bit 3: ALT charset for TEXT
	bits 4-7: BORDER color (like in the 2gs)

The colors byte (Color A) is:
	bits 0-3: background color
	bits 4-7: foreground color

The shader goes through the following phases:
- The fragment determines which VRAMTEX texel it's part of, including the x offset
	to the start of the texel (there is only y offset for TEXT and LGR modes)
- It grabs the texel and determines the video mode to use
- It runs the video mode code on that byte and chooses the correct fragment

NOTE:	The special BORDER graphics mode is set only on border bytes. It only considers
		bits 4-7 as the border color, picked from the LGR color palette.
*/

// Global uniforms
uniform int ticks;						// ms since start
uniform int hborder;					// horizontal border in cycles
uniform int vborder;					// vertical border in scanlines
uniform usampler2D VRAMTEX;				// Video RAM texture
uniform sampler2D a2ModesTex0;			// font 14x16 normal
uniform sampler2D a2ModesTex1;			// font 14x16 alternate
uniform sampler2D a2ModesTex2;			// LGR
uniform sampler2D a2ModesTex3;			// HGR
uniform sampler2D a2ModesTex4;			// DHGR

// Special modes mask is
// enum A2VideoSpecialMode_e
// {
// 		A2_VSM_NONE 			= 0b0000,
// 		A2_VSM_DHGRCOL140Mixed 	= 0b0001,
//		A2_VSM_HGRSPEC1			= 0b0010,
//		A2_VSM_HGRSPEC2		 	= 0b0100,
// };
uniform int specialModesMask;

// Monitor color type
// enum A2VideoMonitorType_e
// {
// 		A2_MON_COLOR = 0,
// 		A2_MON_WHITE,
// 		A2_MON_GREEN,
// 		A2_MON_AMBER,
//		A2_MON_TOTAL_COUNT
// };

uniform int monitorColorType;
// colors for monitor color types
const vec4 monitorcolors[5] = vec4[5](
    vec4(0.000000,	0.000000,	0.000000,	1.000000)	/*BLACK, -- this is a color monitor */
    ,vec4(1.000000,	1.000000,	1.000000,	1.000000)	/*WHITE PHOSPHOR,*/
    ,vec4(0.290196,	1.000000,	0.000000,	1.000000)	/*GREEN PHOSPHOR,*/
    ,vec4(1.000000,	0.717647,	0.000000,	1.000000)	/*AMBER PHOSPHOR,*/
    ,vec4(1.000000,	0.000000,	0.500000,	1.000000)	/*PINK, -- this option shouldn't exist */
);
									  
// Colors for foreground and background
const vec4 tintcolors[16] = vec4[16](
	 vec4(0.000000,	0.000000,	0.000000,	1.000000)	/*BLACK,*/
	,vec4(0.674510,	0.070588,	0.298039,	1.000000)	/*DEEP_RED,*/
	,vec4(0.000000,	0.027451,	0.513725,	1.000000)	/*DARK_BLUE,*/
	,vec4(0.666667,	0.101961,	0.819608,	1.000000)	/*MAGENTA,*/
	,vec4(0.000000,	0.513725,	0.184314,	1.000000)	/*DARK_GREEN,*/
	,vec4(0.623529,	0.592157,	0.494118,	1.000000)	/*DARK_GRAY,*/
	,vec4(0.000000,	0.541176,	0.709804,	1.000000)	/*BLUE,*/
	,vec4(0.623529,	0.619608,	1.000000,	1.000000)	/*LIGHT_BLUE,*/
	,vec4(0.478431,	0.372549,	0.000000,	1.000000)	/*BROWN,*/
	,vec4(1.000000,	0.447059,	0.278431,	1.000000)	/*ORANGE,*/
	,vec4(0.470588,	0.407843,	0.498039,	1.000000)	/*LIGHT_GRAY,*/
	,vec4(1.000000,	0.478431,	0.811765,	1.000000)	/*PINK,*/
	,vec4(0.435294,	0.901961,	0.172549,	1.000000)	/*GREEN,*/
	,vec4(1.000000,	0.964706,	0.482353,	1.000000)	/*YELLOW,*/
	,vec4(0.423529,	0.933333,	0.698039,	1.000000)	/*AQUA,*/
	,vec4(1.000000,	1.000000,	1.000000,	1.000000)	/*WHITE,*/
);

in vec2 vFragPos;       // The fragment position in pixels
out vec4 fragColor;


// Perform left rotation on a 4-bit nibble (for DLGR AUX memory)
// Why? I don't know, can't find any docs, but AppleWin does it and it is Correct
uint ROL_NIB(uint x)
{
        return ((x << 1) & 0xFu) | ((x >> 3) & 0x1u);
}

void main()
{
	uvec2 uFragPos = uvec2(vFragPos);
	uvec4 targetTexel = texelFetch(VRAMTEX, ivec2(uFragPos.x / 14u, uFragPos.y / 2u), 0).rgba;
	fragColor = vec4(float(targetTexel.r)/255.f, float(targetTexel.g)/255.f,
					float(targetTexel.b)/255.f, float(targetTexel.a)/255.f);
}
