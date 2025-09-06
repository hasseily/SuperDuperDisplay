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
 DHGR160 shader
 For each pixel, determine which memory byte it is part of,
 and save the x offset from the origin of the byte.

 The even bytes are from AUX, the odd bytes are from MAIN.
 Each byte draws 2 pixels. Each pixel uses a nibble (4 bits)
 to select 1 color from 16. Leftmost pixel is the high nibble.

 The Apple 2 memory passed in should start at 0x2000 in MAIN
 */

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
uniform COMPAT_PRECISION float isMixed;		// Are we in mixed mode?
uniform vec2 borderTopLeft;
uniform vec2 borderBottomRight;
uniform vec4 borderColor;

uniform usampler2D APPLE2MEMORYTEX; // Apple 2e's memory, starting at 0x2000 in MAIN for DHGR
								 // Unsigned int sampler!
uniform int memstart;			// where to start in memory

in vec2 vFragPos;       // The fragment position in pixels
out vec4 fragColor;

// determines if a point is inside a rect
bool insideAARect_MinMax(vec2 p, vec2 mn, vec2 mx)
{
	// Inclusive edges; add a tiny epsilon to avoid precision glitches.
	const float eps = 1e-6;
	return all(greaterThanEqual(p + eps, mn)) && all(lessThanEqual(p - eps, mx));
}

void main()
{
	if (!insideAARect_MinMax(vFragPos, borderTopLeft, borderBottomRight)) {
		fragColor = borderColor;
		return;
	}

	vec2 vPos = vFragPos - borderTopLeft;	// relative position inside the borders

	if ((isMixed * vPos.y) >= float(2 * 160))
	{
		// we're in mixed mode, the bottom 4 rows of text (4*8=32 pixels) are transparent
		fragColor = vec4(0.0);
		return;
	}
	
	// first figure out which byte this fragment is part of
	// Calculate the position of the fragment in byte pair (aux+main) intervals
	// 640x384 -> 40x192
	vec2 fTileColRow = vPos / vec2(16,2);
	// Row and column number of the tile containing this fragment
	ivec2 tileColRow = ivec2(floor(fTileColRow));
	
	// Next grab the data for that tile from the tilesBuffer
	// No need to rescale values because we're using GL_R8UI
	// The "texture" is split by 1kB-sized rows
	// In double mode, the even bytes are pulled from aux mem,
	// and the odd bytes from main mem
	int offset;
	uint byteVal = 0u;
	offset = memstart + hgrRow[tileColRow.y] + tileColRow.x;
	int pixelIndex = (int(vPos.x/4.0) & 3);	// 4 pixels at a time
	if (pixelIndex < 2)	// AUX
		offset = offset + 0xC000;	// _A2_MEMORY_SHADOW_END 0xC000
	byteVal = texelFetch(APPLE2MEMORYTEX, ivec2(offset % 1024, offset / 1024), 0).r;

	// Now we have a byte that has the color indexes for 2 pixels
	if ((pixelIndex & 0x1) == 1)	// left pixel is high nibble
		fragColor = tintcolors[byteVal >> 4];
	else
		fragColor = tintcolors[byteVal & 0xFu];

}
