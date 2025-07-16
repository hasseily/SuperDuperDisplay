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
 HGR shader
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
 
 The Apple 2 memory passed in should start at 0x2000 or 0x4000 in MAIN.
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
uniform COMPAT_PRECISION float hasFlashing;
uniform COMPAT_PRECISION float isMixed;		// Are we in mixed mode?
uniform COMPAT_PRECISION float isDouble;	// Are we in double res?

// Mesh-level uniforms assigned in MosaicMesh
uniform uvec2 tileSize;
uniform usampler2D APPLE2MEMORYTEX; // Apple 2e's memory, starting at 0x2000 in MAIN for HGR1 and 0x4000 for HGR2
								 // Unsigned int sampler!
uniform int memstart = 0;		// where to start in memory

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
	int offset = memstart + hgrRow[tileColRow.y] + tileColRow.x;
	
	// the byte value is just the r component
	uint byteVal = texelFetch(APPLE2MEMORYTEX, ivec2(offset % 1024, offset / 1024), 0).r;
	// Grab the other bytes that matter
	uint byteValPrev = 0u;
	uint byteValNext = 0u;
	if (tileColRow.x > 0)	// Not at start of row, byteValPrev is valid
	{
		byteValPrev = texelFetch(APPLE2MEMORYTEX, ivec2((offset % 1024) - 1, offset / 1024), 0).r;
	}
	if (tileColRow.x < 39)	// Not at end of row, byteValNext is valid
	{
		byteValNext = texelFetch(APPLE2MEMORYTEX, ivec2((offset % 1024) + 1, offset / 1024), 0).r;
	}
	
	ivec2 textureSize2d = textureSize(a2ModeTexture,0);
	
	// Calculate the column offset in the texture
	int texXOffset = (int((byteValPrev & 0xE0u) << 2) | int((byteValNext & 0x03u) << 5)) + (tileColRow.x & 1) * 16;
	
	// Now get the texture color. We know the X offset as well as the fragment's offset on top of that.
	// The y value is just the byte's value
	vec4 tex = texture(a2ModeTexture, vec2(texXOffset + int(fragOffset.x), byteVal) / vec2(textureSize2d));
	
	fragColor = tex;
	// fragColor = vec4(vColor, 1.f);   // for debugging
}
