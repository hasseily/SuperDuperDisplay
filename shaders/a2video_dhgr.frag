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
 DHGR shader
 For each pixel, determine which memory byte it is part of,
 and save the x offset from the origin of the byte.

 There are 256 columns of 10 pixels in the DHGR texture. Acquiring the right data is a lot more complicated.
 It involves taking 20 bits out of 4 memory bytes, then shifting and grabbing different bytes for x and y
 in the texture. See UpdateDHiResCell() in RGBMonitor.cpp of the AppleWin codebase. Take 7 bits each of
 the 2 middle bytes and 3 bits each of the 2 end bytes for a total of 20 bits.
 
 The Apple 2 memory passed in should start at 0x2000 in MAIN
 */

// Apple 2 HGR row offsets in memory. The rows aren't contiguous in Apple 2 RAM.
// They're interlaced because WOZ chip optimization.
const int hgrRow[192] = int[192](
	0x0000, 0x0400, 0x0800, 0x0C00, 0x1000, 0x1400, 0x1800, 0x1C00,
	0x0080, 0x0480, 0x0880, 0x0C80, 0x1080, 0x1480, 0x1880, 0x1C80,
	0x0100, 0x0500, 0x0900, 0x0D00, 0x1100, 0x1500, 0x1900, 0x1D00,
	0x0180, 0x0580, 0x0980, 0x0D80, 0x1180, 0x1580, 0x1980, 0x1D80,
	0x0200, 0x0600, 0x0A00, 0x0E00, 0x1200, 0x1600, 0x1A00, 0x1E00,
	0x0280, 0x0680, 0x0A80, 0x0E80, 0x1280, 0x1680, 0x1A80, 0x1E80,
	0x0300, 0x0700, 0x0B00, 0x0F00, 0x1300, 0x1700, 0x1B00, 0x1F00,
	0x0380, 0x0780, 0x0B80, 0x0F80, 0x1380, 0x1780, 0x1B80, 0x1F80,
	0x0028, 0x0428, 0x0828, 0x0C28, 0x1028, 0x1428, 0x1828, 0x1C28,
	0x00A8, 0x04A8, 0x08A8, 0x0CA8, 0x10A8, 0x14A8, 0x18A8, 0x1CA8,
	0x0128, 0x0528, 0x0928, 0x0D28, 0x1128, 0x1528, 0x1928, 0x1D28,
	0x01A8, 0x05A8, 0x09A8, 0x0DA8, 0x11A8, 0x15A8, 0x19A8, 0x1DA8,
	0x0228, 0x0628, 0x0A28, 0x0E28, 0x1228, 0x1628, 0x1A28, 0x1E28,
	0x02A8, 0x06A8, 0x0AA8, 0x0EA8, 0x12A8, 0x16A8, 0x1AA8, 0x1EA8,
	0x0328, 0x0728, 0x0B28, 0x0F28, 0x1328, 0x1728, 0x1B28, 0x1F28,
	0x03A8, 0x07A8, 0x0BA8, 0x0FA8, 0x13A8, 0x17A8, 0x1BA8, 0x1FA8,
	0x0050, 0x0450, 0x0850, 0x0C50, 0x1050, 0x1450, 0x1850, 0x1C50,
	0x00D0, 0x04D0, 0x08D0, 0x0CD0, 0x10D0, 0x14D0, 0x18D0, 0x1CD0,
	0x0150, 0x0550, 0x0950, 0x0D50, 0x1150, 0x1550, 0x1950, 0x1D50,
	0x01D0, 0x05D0, 0x09D0, 0x0DD0, 0x11D0, 0x15D0, 0x19D0, 0x1DD0,
	0x0250, 0x0650, 0x0A50, 0x0E50, 0x1250, 0x1650, 0x1A50, 0x1E50,
	0x02D0, 0x06D0, 0x0AD0, 0x0ED0, 0x12D0, 0x16D0, 0x1AD0, 0x1ED0,
	0x0350, 0x0750, 0x0B50, 0x0F50, 0x1350, 0x1750, 0x1B50, 0x1F50,
	0x03D0, 0x07D0, 0x0BD0, 0x0FD0, 0x13D0, 0x17D0, 0x1BD0, 0x1FD0
);


// Global uniforms assigned in A2VideoManager
uniform sampler2D a2ModeTexture;
uniform int ticks;                  // ms since start
uniform COMPAT_PRECISION float isMixed;		// Are we in mixed mode?

// Mesh-level uniforms assigned in MosaicMesh
uniform uvec2 tileCount;         // Count of tiles (cols, rows)
uniform uvec2 tileSize;
uniform usampler2D DBTEX;        // Apple 2e's memory, starting at 0x2000 in MAIN for DHGR
								 // Unsigned int sampler!

in vec2 vFragPos;       // The fragment position in pixels
// in vec3 vColor;         // DEBUG color, a mix of all 3 vertex colors

out vec4 fragColor;

void main()
{
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
