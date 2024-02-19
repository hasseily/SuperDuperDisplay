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
- Type GL_RGB8UI, which is 3 bytes for each texel
- Color R is the MAIN memory byte
- Color G is the AUX memory byte
- Color B is 8 bits of state, including the graphics mode and soft switches
- 40 pixels wide, where MAIN and AUX are interleaved, starting with AUX
- 192 lines high, which is the Apple 2 scanlines

The shader goes through the following phases:
- The fragment determines which VRAMTEX texel it's part of, including the x offset
	to the start of the texel (there is no y offset, each byte is on one scanline).
- It grabs the texel and determines the video mode to use
- It runs the video mode code on that byte and chooses the correct fragment

*/

// Global uniforms
uniform int ticks;						// ms since start
uniform usampler2D VRAMTEX;				// Video RAM texture
uniform sampler2D a2ModesTextures[5];	// 2 font textures + lgr, hgr, dhgr
uniform vec4 colorTint;					// text color tint (extra from 2gs)

// mode -> texture
// modes are: TEXT, DTEXT, LGR, DLGR, HGR, DHGR
// For text and dtext modes, texture 0 is normal and 1 is alternate
const int modeToTexture[6] = int[6](0, 0, 2, 2, 3, 4);

in vec2 vFragPos;       // The fragment position in pixels
out vec4 fragColor;

void main()
{
	// first determine which VRAMTEX texel this fragment is part of, including
	// the x and y offsets from the origin
	// REMINDER: we're working on dots, with 560 dots per line. And lines are doubled
	uvec2 uFragPos = uvec2(vFragPos);
	uvec3 targetTexel =  texelFetch(VRAMTEX, ivec2(uFragPos.x / 14u, uFragPos.y / 2u), 0);
	uvec2 fragOffset = vec2(uFragPos.x % 14u, uFragPos.y % 16);
	// The fragOffsets are:
	// x is 0-14
	// y is 0-16

	// Extract the lower 3 bits to determine which mode to use
	uint a2mode = targetTexel.b & 7u;	// 7 = 0b111 to mask lower 3 bits
    uint textureIndex = modeToTexture[a2mode];	// this is the texture to use
	
	switch (a2mode) {
		case 0u:	// TEXT
		case 1u:	// DTEXT
		{
			// Get the character value
			// In DTEXT mode, the first 7 dots are AUX, last 7 are MAIN.
			// In TEXT mode, all 14 dots are from MAIN
			uint charVal = (targetTexel.g * (fragOffset.x / 7u) + targetTexel.r * (1u - (fragOffset.x / 7u))) * a2mode
							+ (targetTexel.r * fragOffset.x) * (1 - a2mode);
			float vCharVal = float(charVal);
			
			// if ALTCHARSET (bit 4), use the alt texture
			uint isAlt = ((targetTexel.b >> 4) & 1u);
			textureIndex += 1u * isAlt;
			
			// Determine from char which font glyph to use
			// and if we need to flash
			// Determine if it's inverse when the char is below 0x40
			// And then if the char is below 0x80 and not inverse, it's flashing,
			// but only if it's the regular charset
			float a_inverse = 1.0 - step(float(0x40), vCharVal);
			float a_flash = (1.0 - step(float(0x80), vCharVal)) * (1.0 - a_inverse) * (1.0 - float(isAlt));
			
			ivec2 textureSize2d = textureSize(textureIndex,0);
			// what's our character's starting origin in the character map?
			uvec2 charOrigin = uvec2(charVal & 0xFu, charVal >> 4) * uvec2(14, 16);	// each glyph is 14x16
			
			// Now get the texture color
			// When getting from the texture color, in DTEXT multiply the x value by 2 because we're taking
			// 1/2 of each column in 80 col mode.
			vec4 tex = texture(textureIndex, (vec2(charOrigin) + (fragOffset * uvec2(1u + a2mode, 1u)) / vec2(textureSize2d)) * colorTint;
			
			float isFlashing =  a_flash * float((ticks / 310) % 2);    // Flash every 310ms
																	   // get the color of flashing or the one above
			fragColor = ((1.f - tex) * isFlashing) + (tex * (1.f - isFlashing));
			break;
		}
		case 2u:	// LGR
			break;
		case 3u:	// DLGR
			break;
		case 4u:	// HGR
			break;
		case 5u:	// DHGR
			break;
		default:
			// should never happen! Set to pink for visibility
			fragColor = vec4(1.0f, 0f, 0.5f, 1.f);
			return;
			break;
	}
	
	if (a2mode < 2u)	// TEXT or DTEXT mode
	{
		// Get the character value
		// If we're in DTEXT mode, the first 7 pixels are AUX, last 7 are MAIN.
		// When getting from the texture, multiply the x value by 2.
		// If in TEXT mode, all 14 pixels are MAIN. The x value is what it is
		uint charVal = a2mode;
		float vCharVal = float(charVal);
		
		// if ALTCHARSET (bit 4), use the alt texture
		textureIndex += 1u * ((targetTexel.b >> 4) & 1u);
		
		// Determine from char which font glyph to use
		// and if we need to flash
		// Determine if it's inverse when the char is below 0x40
		// And then if the char is below 0x80 and not inverse, it's flashing
		float a_inverse = 1.0 - step(float(0x40), vCharVal);
		float a_flash = (1.0 - step(float(0x80), vCharVal)) * (1.0 - a_inverse) * hasFlashing;
	}


	// TODO: check for DHGRMONO

	vec2 sampleCoord = vec2(texCoord) / vec2(textureSize(selectorTexture, 0));
	fragColor = texture(textures[textureIndex], sampleCoord);

///////////////////////// OLD CODE /////////////////////////////
	if ((isMixed * vFragPos.y) >= float(tileSize.y * 160u))
	{
		// we're in mixed mode, the bottom 4 rows of text (4*8=32 pixels) are transparent
		fragColor = vec4(0.0);
		return;
	}
	
	// first figure out which mosaic tile (byte) this fragment is part of
	// Calculate the position of the fragment in tile intervals
	vec2 fTileColRow = vFragPos / vec2(tileSize);
	// Row and column number of the tile containing this fragment
	ivec2 tileColRow = ivec2(floor(fTileColRow));
	// Fragment offset to tile origin, in pixels
	vec2 fragOffset = ((fTileColRow - vec2(tileColRow)) * vec2(tileSize));
	
	// Next grab the data for that tile from the tilesBuffer
	// No need to rescale values because we're using GL_R8UI
	// The "texture" is split by 1kB-sized rows
	// In double mode, the even bytes are pulled from aux mem,
	// and the odd bytes from main mem
	int offset;
	uint byteVal1 = 0u;
	uint byteVal4 = 0u;
	// The bytes from main: 1 and 3
	offset = hgrRow[tileColRow.y] + tileColRow.x;
	uint byteVal3 = texelFetch(DBTEX, ivec2(offset % 1024, offset / 1024), 0).r;
	if (tileColRow.x > 0)	// Not at start of row, byteVal1 is valid
	{
		byteVal1 = texelFetch(DBTEX, ivec2((offset % 1024) - 1, offset / 1024), 0).r;
	}
	// The bytes from aux: 2 and 4
	offset = offset + 0xC000;
	uint byteVal2 = texelFetch(DBTEX, ivec2(offset % 1024, offset / 1024), 0).r;
	if (tileColRow.x < 39)	// Not at end of row, byteVal4 is valid
	{
		byteVal4 = texelFetch(DBTEX, ivec2((offset % 1024) + 1, offset / 1024), 0).r;
	}
	
	ivec2 textureSize2d = textureSize(a2ModeTexture,0);
	
	// Calculate the column offset in the texture
	int wordVal = (int(byteVal1) & 0x70) | ((int(byteVal2) & 0x7F) << 7) |
		((int(byteVal3) & 0x7F) << 14) | ((int(byteVal4) & 0x07) << 21);
	int vColor = (tileColRow.x*14 + int(fragOffset.x)) & 3;
	int vValue = (wordVal >> (4 + int(fragOffset.x) - vColor));
	int xVal = 10 * ((vValue >> 8) & 0xFF) + vColor;
	int yVal = vValue & 0xFF;
	vec4 tex = texture(a2ModeTexture, (vec2(0.5, 0.5) + vec2(xVal, yVal)) / vec2(textureSize2d));
	
	fragColor = tex;
	// fragColor = vec4(vColor, 1.f);   // for debugging
}
