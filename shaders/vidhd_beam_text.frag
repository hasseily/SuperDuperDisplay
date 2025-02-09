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
 TEXT_40X24,	// 28x32 font (transparent H borders 400px, V borders 156px)
 TEXT_80X24,	// 14x32 font (transparent H borders 400px, V borders 156px)
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
- If there is a border for a mode, it's always transparent and used to center the text

The colors (Color G) and transparency (Color A) are:
	bits 0-3: background color
	bits 4-7: foreground color
*/

// Global uniforms
uniform int ticks;			// ms since start
uniform usampler2D VRAMTEX;	// Video RAM texture
uniform int vidhdMode;		// VidHdMode_e 1:TEXT_40X24, 2:TEXT_80X24, ...
uniform int xwidth;			// How many characters in width?
uniform int yheight;		// How many characters in height?
uniform sampler2D fontTex;	// font texture to use for the given mode
uniform uvec2 glyphSize;	// Size of each glyph in the font texture
uniform uvec2 fontScale;	// How much to increase the size of the font
uniform vec2 border;		// number of pixels in transparent border

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
	// REMINDER: we're working on 1920x1080 fixed

	vec2 outputSize = vec2(1920.0,1080.0);
	// vec2 fragPixel = gl_FragCoord.xy;
	vec4 vidhdColor = vec4(0.0);	// transparency by default
	bool isInBounds = true;

	if (yheight < 25)
	{
		// This is the standard Apple 2 modes, we'll align them exactly to the legacy viewport
		// Compute first the legacy viewport size
		vec2 imageCenter = outputSize * 0.5;
		vec2 legacyOffset = vec2(560.0, 384.0);		// exact legacy size
		if (vFragPos.x < imageCenter.x - legacyOffset.x || vFragPos.x >= imageCenter.x + legacyOffset.x ||
			vFragPos.y < imageCenter.y - legacyOffset.y || vFragPos.y >= imageCenter.y + legacyOffset.y)
		{
			isInBounds = false;
		}
	}
	if (isInBounds)
	{
		uint uCharW = fontScale.x*glyphSize.x;
		uint uCharH = fontScale.y*glyphSize.y;
		uvec2 uFragPos = uvec2(vFragPos - border);
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
		vidhdColor = (tex * tintcolors[(targetTexel.g & 0xF0u) >> 4])			// foreground (dot is on)
						+ ((1.f - tex) * tintcolors[targetTexel.g & 0x0Fu]);	// background (dot is off)

		// Do the transparency
		vidhdColor.a = (tex.r * float((targetTexel.a & 0xF0u) >> 4) / 15.0)		// foreground (dot is on)
		+ ((1.f - tex.r) * float(targetTexel.a & 0x0Fu) / 15.0);				// background (dot is off)

	}	// isInBounds


	// Overlay on top of the input texture, which will be integer scaled and centered
	// We scale it 2x to align it to how VidHD displays the legacy and SHR video.
	if (length(inputSize) > 0.0) {
		vec2 scaleFactor = vec2(2.0, 2.0);

//		// Compute the maximum integer scale factor that fits nativeSize into outputSize.
//		float sf = floor(min(outputSize.x / inputSize.x, outputSize.y / inputSize.y));
//		if (sf < 0.1)
//			sf = 0.5;
//		scaleFactor = vec2(sf);

		// Compute the displayed image size and offset (in pixels) to center it.
		vec2 displaySize = inputSize * scaleFactor;
		vec2 offset = (outputSize - displaySize) * 0.5;

		 // If this fragment lies outside the centered display area, transparent.
		vec4 mergedColor;
		
		 if (vFragPos.x < offset.x || vFragPos.x >= offset.x + displaySize.x ||
			 vFragPos.y < offset.y || vFragPos.y >= offset.y + displaySize.y) {
			 mergedColor = vec4(0.0);
		 } else {
			 // Compute the local pixel coordinate inside the display area,
			 // then convert to a normalized [0,1] coordinate relative to nativeSize.
			 vec2 localPixel = (vFragPos - offset) / scaleFactor;
			 vec2 mergedTexCoord = localPixel / inputSize;

			 // --- Now use mergedTexCoord instead of vTexCoords ---
			 mergedColor = texture(inputTex, mergedTexCoord);
		 }

		// Blend with the VidHD overlay.
		fragColor = mix(mergedColor, vidhdColor, vidhdColor.a);
	} else {
		fragColor = vidhdColor;
	}
}
