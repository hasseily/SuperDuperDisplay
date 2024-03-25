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
TODO: Fix based on new vram format

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
uniform usampler2D VRAMTEX;				// Video RAM texture
uniform sampler2D a2ModesTex0;			// font 14x16 normal
uniform sampler2D a2ModesTex1;			// font 14x16 alternate
uniform sampler2D a2ModesTex2;			// LGR
uniform sampler2D a2ModesTex3;			// HGR
uniform sampler2D a2ModesTex4;			// DHGR

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
	// REMINDER: we're working on dots, with 560 dots per line. And lines are doubled
	uvec2 uFragPos = uvec2(vFragPos);
	uvec4 targetTexel = texelFetch(VRAMTEX, ivec2(uFragPos.x / 14u, uFragPos.y / 2u), 0).rgba;
	uvec2 fragOffset = uvec2(uFragPos.x % 14u, uFragPos.y % 16u);
	// The fragOffsets are:
	// x is 0-13
	// y is 0-15

	// Extract the lower 3 bits to determine which mode to use
	uint a2mode = targetTexel.b & 7u;	// 7 = 0b111 to mask lower 3 bits
	
	switch (a2mode) {
		case 0u:	// TEXT
		case 1u:	// DTEXT
		{
			// Get the character value
			// In TEXT mode (1u - a2mode), all 14 dots are from MAIN
			// In DTEXT mode (a2mode), the first 7 dots are AUX, last 7 are MAIN.
			uint charVal = (1u - a2mode) * targetTexel.r
							+ a2mode * (targetTexel.r * (fragOffset.x / 7u) + targetTexel.g * (1u - (fragOffset.x / 7u)));
			float vCharVal = float(charVal);
			
			// if ALTCHARSET (bit 3), use the alt texture
			uint isAlt = ((targetTexel.b >> 3) & 1u);
			
			// Determine from char which font glyph to use
			// and if we need to flash
			// Determine if it's inverse when the char is below 0x40
			// And then if the char is below 0x80 and not inverse, it's flashing,
			// but only if it's the regular charset
			float a_inverse = 1.0 - step(float(0x40), vCharVal);
			float a_flash = (1.0 - step(float(0x80), vCharVal)) * (1.0 - a_inverse) * (1.0 - float(isAlt));
			
			// what's our character's starting origin in the character map?
			// each glyph is 14x16
			uvec2 charOrigin = uvec2(charVal & 0xFu, charVal >> 4) * uvec2(14u, 16u);
			
			// The fragment offset in TEXT is 0-13, properly spanning the whole glyph.
			// The fragment offset in DTEXT is also 0-13. But 0-6 is in one glyph, and 7-13 is in another.
			// And both should span the whole glyph, using the even pixels only
			fragOffset.x = (1u - a2mode) * fragOffset.x		// TEXT mode
							// DTEXT mode : shift by 7 the offset of the MAIN byte, don't touch the offset of AUX, and *2 for even pixels
							+ a2mode * (((fragOffset.x - 7u) * (fragOffset.x / 7u)) + fragOffset.x * (1u - (fragOffset.x / 7u))) * 2u;
			
			// Now get the texture color
			ivec2 textureSize2d;
			vec4 tex;
			if (bool(isAlt))
			{
				textureSize2d = textureSize(a2ModesTex1,0);
				tex = texture(a2ModesTex1, (vec2(charOrigin + fragOffset) + vec2(0.5,0.5)) / vec2(textureSize2d));
			} else {
				textureSize2d = textureSize(a2ModesTex0,0);
				tex = texture(a2ModesTex0, (vec2(charOrigin + fragOffset) + vec2(0.5,0.5)) / vec2(textureSize2d));
			}

			float isFlashing =  a_flash * float((ticks / 310) % 2);    // Flash every 310ms
																	   // get the color of flashing or the one above
			tex = ((1.f - tex) * isFlashing) + (tex * (1.f - isFlashing));
			
			// And provide for tint coloring that the 2gs can do
			fragColor = (tex * tintcolors[(targetTexel.a & 0xF0u) >> 4])		// foreground (dot is on)
			+ ((1.f - tex) * tintcolors[targetTexel.a & 0x0Fu]);	// background (dot is off)
			return;
			break;
		}
		case 2u:	// LGR
		case 3u:	// DLGR
		{
			// Get the color value
			// An LGR byte is split in 2. There's a 4-bit color in the low bits
			// at the top of the 14x16 dot square, and another 4-bit color in
			// the high bits at the bottom of the 14x16 dot square.
			// In DLGR mode, the first 7 dots are AUX, last 7 are MAIN.
			// In LGR mode, all 14 dots are from MAIN

			// Get the byte value depending on MAIN or AUX
			uint byteVal = (1u - (a2mode - 2u)) * targetTexel.r	// LGR
							// DLGR: take .r if offset is 7-13, otherwise take .g if offset is 0-6
							+ (a2mode - 2u) * (targetTexel.r * (fragOffset.x / 7u) + targetTexel.g * (1u - (fragOffset.x / 7u)));
			// get the color depending on vertical position
			uvec2 byteOrigin;
			byteOrigin = (1u - (fragOffset.y / 8u)) * uvec2(0u, (byteVal & 0xFu) * 16u) // Top pixel, color of the 4 low bits
			+ (fragOffset.y / 8u) * uvec2(0u, (byteVal >> 4) * 16u);	// Bottom pixel, color of the 4 high bits

			ivec2 textureSize2d = textureSize(a2ModesTex2,0);
			// if we're in DLGR (a2mode - 2u), get every other column
			fragColor = texture(a2ModesTex2,
								(vec2(byteOrigin) + vec2(fragOffset * uvec2(1u + (a2mode - 2u), 1u)) + vec2(0.5,0.5)) / vec2(textureSize2d));
			return;
			break;
		}
		case 4u:	// HGR
		{
/*
For each pixel, determine which memory byte it is part of,
 and save the x offset from the origin of the byte.

 To get a pixel in the HGR texture, the procedure is as follows:
 Take the byte that the pixel is in.
 Even bytes use even columns, odd bytes use odd columns.
 Also calculate the high bit and last 2 bits from the previous byte
 (i.e. the 3 most significant bits), and the first 2 bits from the
 next byte (i.e. the 3 least significant bits).

 // Lookup Table:
 // y (0-255) * 32 columns of 32 pixels
 // . each column is: high-bit (prev byte) & 2 pixels from previous byte & 2 pixels from next byte
 // . each 32-pixel unit is 2 * 16-pixel sub-units: 16 pixels for even video byte & 16 pixels for odd video byte
 //   . where 16 pixels represent the 7 Apple pixels, expanded to 14 pixels (and the last 2 are discarded)
 //		currHighBit=0: {14 pixels + 2 pad} * 2
 //		currHighBit=1: {1 pixel + 14 pixels + 1 pad} * 2

 high-bit & 2-bits from previous byte, 2-bits from next byte = 2^5 = 32 total permutations
 32 permutations, each with 2 bytes, each 8 bits but doubled: 32 * 2 * 8 * 2 = 1024 pixels wide
 So the col offset is ((prevbyte & 0xE0) >> 3) | (nextbyte & 0x03). But since each column is
 32 pixels, the actual col pixel offset should be *32, which results in:
 ((prevbyte & 0xE0) << 2) | ((nextbyte & 0x03) << 5)
 Then we also need to see which of the 2 subcolumns we will use, depending if it's an even or odd byte:
 ((prevbyte & 0xE0) << 2) | ((nextbyte & 0x03) << 5) + (tileColRow.x & 1) * 16
 The row pixel value is simply the memory byte value of our pixel
 */
 
			// The byte value is just targetTexel.r
			// Grab the other byte values that matter
			uint byteValPrev = 0u;
			uint byteValNext = 0u;
			int xCol = int(uFragPos.x) / 14;
			if (xCol > 0)	// Not at start of row, byteValPrev is valid
			{
				byteValPrev = texelFetch(VRAMTEX, ivec2(xCol - 1, uFragPos.y / 2u), 0).r;
			}
			if (xCol < 39)	// Not at end of row, byteValNext is valid
			{
				byteValNext = texelFetch(VRAMTEX, ivec2(xCol + 1, uFragPos.y / 2u), 0).r;
			}

			// calculate the column offset in the color texture
			int texXOffset = (int((byteValPrev & 0xE0u) << 2) | int((byteValNext & 0x03u) << 5)) + (xCol & 1) * 16;

			// Now get the texture color. We know the X offset as well as the fragment's offset on top of that.
			// The y value is just the byte's value
			ivec2 textureSize2d = textureSize(a2ModesTex3,0);
			fragColor = texture(a2ModesTex3, (vec2(texXOffset + int(fragOffset.x), targetTexel.r) + vec2(0.5,0.5)) / vec2(textureSize2d));
			return;
			break;
		}
		case 5u:	// DHGR
		{
/*
 For each pixel, determine which memory byte it is part of,
 and save the x offset from the origin of the byte.

 There are 256 columns of 10 pixels in the DHGR texture. Acquiring the right data is a lot more complicated.
 It involves taking 20 bits out of 4 memory bytes, then shifting and grabbing different bytes for x and y
 in the texture. See UpdateDHiResCell() in RGBMonitor.cpp of the AppleWin codebase. Take 7 bits each of
 the 2 middle bytes and 3 bits each of the 2 end bytes for a total of 20 bits.
 */
			// In DHGR, as in all double modes, the even bytes are from AUX, odd bytes from MAIN
			// We already have in targetTexel both MAIN and AUX bytes (R and G respectively)
			// We need a previous MAIN byte and a subsequent AUX byte to calculate the colors
			uint byteVal1 = 0u;				// MAIN
			uint byteVal2 = targetTexel.g;	// AUX
			uint byteVal3 = targetTexel.r;	// MAIN
			uint byteVal4 = 0u;				// AUX
			int xCol = int(uFragPos.x) / 14;
			if (xCol > 0)	// Not at start of row, byteVal1 is valid
			{
				byteVal1 = texelFetch(VRAMTEX, ivec2(xCol - 1, uFragPos.y / 2u), 0).r;
			}
			if (xCol < 39)	// Not at end of row, byteVal4 is valid
			{
				byteVal4 = texelFetch(VRAMTEX, ivec2(xCol + 1, uFragPos.y / 2u), 0).g;
			}
			// Calculate the column offset in the color texture
			int wordVal = (int(byteVal1) & 0x70) | ((int(byteVal2) & 0x7F) << 7) |
				((int(byteVal3) & 0x7F) << 14) | ((int(byteVal4) & 0x07) << 21);
			int vColor = (xCol*14 + int(fragOffset.x)) & 3;
			int vValue = (wordVal >> (4 + int(fragOffset.x) - vColor));
			int xVal = 10 * ((vValue >> 8) & 0xFF) + vColor;
			int yVal = vValue & 0xFF;
			ivec2 textureSize2d = textureSize(a2ModesTex4,0);
			fragColor = texture(a2ModesTex4, (vec2(xVal, yVal) + vec2(0.5, 0.5)) / vec2(textureSize2d));
			return;
			break;
		}
		case 6u:	// DHGR MONO
		{
			// Use the .g (AUX) if the dot is one of the first 7, otherwise .r (MAIN)
			// Find out if the related bit is on, and set the color to white or black
			fragColor = vec4(1.0f) * float(clamp(((targetTexel.r << 7) | (targetTexel.g & 0x7Fu)) & (1u << (uFragPos.x % 14u)), 0u, 1u));
			return;
			break;
		}
		case 7u:	// BORDER
		{
			// Special border mode to give to the //e the border features like the 2gs
			// It just looks at the flags byte and grabs the border color in the upper 4 bits
			// then applies the LGR color mapping to it
			uint borderColor = targetTexel.b >> 4;
			ivec2 textureSize2d = textureSize(a2ModesTex2,0);

			// get the color from the LGR texture
			uvec2 byteOrigin = uvec2(0u, borderColor * 16u);
			fragColor = texture(a2ModesTex2,
								(vec2(byteOrigin) + vec2(fragOffset) + vec2(0.5,0.5)) / vec2(textureSize2d));

			return;
			break;
		}
		default:
		{
			// should never happen! Set to pink for visibility
			fragColor = vec4(1.0f, 0.f, 0.5f, 1.f);
			return;
			break;
		}
	}
	// Shouldn't happen either
	fragColor = vec4(0.f, 1.0f, 0.5f, 1.f);
}
