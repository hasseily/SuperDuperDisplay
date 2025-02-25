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
Shader for unique VidHD text modes:
 TEXT_40X24,	// 28x32 font	// standard Apple 2 40COL
 TEXT_80X24,	// 14x32 font	// standard Apple 2 80COL
 TEXT_80X45,	// 24x24 font
 TEXT_120X67,	// 16x16 font
 TEXT_240X135,	// 8x8 font

This shader expects as input a VRAMTEX texture that has the following features:
- Type GL_RGBA8UI, which is 4 bytes for each texel
- Color R is the text byte value
- Color G is the fore and background colors, as specified in the C022 softswitch
- Color B is unused for now
- Color A is the fore and background transparency levels (16 levels of transparency)
- The VRAMTEX is always 240x135. Only the relevant bytes for the resolution will be used.

The colors (Color G) and transparency (Color A) are:
	bits 0-3: background color
	bits 4-7: foreground color
*/

// Global uniforms
uniform int ticks;			// ms since start
uniform usampler2D VRAMTEX;	// Video RAM texture
uniform int vidhdMode;		// VidHdMode_e 1:TEXT_40X24, 2:TEXT_80X24, ...
uniform ivec2 modeSize;		// How many characters in width and height?
uniform sampler2D fontTex;	// font texture to use for the given mode
uniform uvec2 glyphSize;	// Size of each glyph in the font texture
uniform uvec2 fontScale;	// How much to increase the size of the font

// the incoming texture to overlay the vidhd text mode on
// this texture will be scaled to the maximum while centered
uniform sampler2D inputTex;
uniform vec2 inputSize;

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

void main()
{
	// first determine which VRAMTEX texel this fragment is part of, including
	// the x and y offsets from the origin
	// REMINDER: outputSize is 560x384 for classic modes, otherwise 1920x1080

	uint uCharW = glyphSize.x*fontScale.x;
	uint uCharH = glyphSize.y*fontScale.y;
	uvec2 uFragPos = uvec2(vFragPos);
	uvec4 targetTexel = texelFetch(VRAMTEX, ivec2(uFragPos.x / uCharW, uFragPos.y / uCharH), 0).rgba;
	uvec2 fragOffset = uvec2(uFragPos.x % uCharW, uFragPos.y % uCharH);
	// The fragOffsets are:
	// x is 0-uCharW
	// y is 0-uCharH

	// Get the character value
	uint charVal = targetTexel.r;
	float vCharVal = float(charVal);

	// Determine from char which font glyph to use
	// and if we need to flash
	// Determine if it's inverse when the char is below 0x40
	// And then if the char is below 0x80 and not inverse, it's flashing,
	// but only if it's the regular charset
	float a_inverse = 1.0 - step(float(0x40), vCharVal);
	float a_flash = (1.0 - step(float(0x80), vCharVal)) * (1.0 - a_inverse);

	// what's our character's starting origin in the character map?
	uvec2 charOrigin = uvec2(charVal & 0xFu, charVal >> 4) * glyphSize;

	// Now get the texture color of the font atlas
	ivec2 textureSize2d;
	vec4 tex;
	textureSize2d = textureSize(fontTex,0);
	tex = texture(fontTex, (vec2(charOrigin + (fragOffset / fontScale)) + vec2(0.5,0.5)) / vec2(textureSize2d));

	float isFlashing =  a_flash * float((ticks / 310) & 1);    // Flash every 310ms
																// get the color of flashing or the one above
	tex = ((1.f - tex) * isFlashing) + (tex * (1.f - isFlashing));

	// Provide for tint coloring that the 2gs can do
	fragColor = (tex * tintcolors[(targetTexel.g & 0xF0u) >> 4])			// foreground (dot is on)
					+ ((1.f - tex) * tintcolors[targetTexel.g & 0x0Fu]);	// background (dot is off)

	// Do the transparency
	fragColor.a = (tex.r * float((targetTexel.a & 0xF0u) >> 4) / 15.0)		// foreground (dot is on)
	+ ((1.f - tex.r) * float(targetTexel.a & 0x0Fu) / 15.0);				// background (dot is off)

}
