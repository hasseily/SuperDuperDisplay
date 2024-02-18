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

	uvec3 targetTexel =  texelFetch(VRAMTEX, ivec2(vFragPos.x / 40u, vFragPos.y), 0);
	uint xOffset = vFragPos.x % 40u;		// one of 7 pixels

	// Extract the lower 3 bits to determine which mode to use
    uint textureIndex = modeToTexture[selectionValue.b & uint(7)]; // 7 = 0b111 to mask lower 3 bits


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
