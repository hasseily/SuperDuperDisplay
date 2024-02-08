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
 LGR shader
 For each pixel, determine which memory byte it is part of,
 and save the x and y offsets from the origin of the byte.
 Then based on the value of that byte, determine the origin
 inside the LGR texture. Finally, find the color value
 of the pixel based on the xy offsets to the origin of the byte.
 
 When using color, this is overkill as LGR colors are filled.
 But when in b/w, we can use dithering for LGR so each pixel may
 be different within a "color".
*/

// Apple 2 text row offsets in memory. The rows aren't contiguous in Apple 2 RAM.
// They're interlaced because WOZ chip optimization.
const int textRow[24]= int[24](
0x0000, 0x0080, 0x0100, 0x0180, 0x0200, 0x0280, 0x0300, 0x0380, 
0x0028, 0x00A8, 0x0128, 0x01A8, 0x0228, 0x02A8, 0x0328, 0x03A8, 
0x0050, 0x00D0, 0x0150, 0x01D0, 0x0250, 0x02D0, 0x0350, 0x03D0
);

// Global uniforms assigned in A2VideoManager
uniform sampler2D a2ModeTexture;
uniform int ticks;                  // ms since start
uniform COMPAT_PRECISION float hasFlashing;
uniform COMPAT_PRECISION float isMixed;		// Are we in mixed mode?
uniform COMPAT_PRECISION float isDouble;	// Are we in double res?

// Mesh-level uniforms assigned in MosaicMesh
uniform uvec2 tileCount;         // Count of tiles (cols, rows)
uniform uvec2 tileSize;
uniform usampler2D DBTEX;        // Apple 2e's memory, starting at 0x400 for TEXT1 and 0x800 for TEXT2
                                 // Unsigned int sampler!

in vec2 vFragPos;       // The fragment position in pixels
// in vec3 vColor;         // DEBUG color, a mix of all 3 vertex colors

out vec4 fragColor;

void main()
{
	if ((isMixed * vFragPos.y) >= float(tileSize.y * 20u))
	{
		// we're in mixed mode, the bottom 4 rows are transparent
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
	// In 80-col mode, the even bytes are pulled from aux mem,
	// and the odd bytes from main mem
	int offset;
	if (isDouble > 0.0)
		offset = (textRow[tileColRow.y] + tileColRow.x / 2) + (0xC000 * (1 - (tileColRow.x & 1)));
	else
		offset = textRow[tileColRow.y] + tileColRow.x;
    // the byte value is just the r component
    uint byteVal = texelFetch(DBTEX, ivec2(offset % 1024, offset / 1024), 0).r;

    ivec2 textureSize2d = textureSize(a2ModeTexture,0);

	// What's our byte's starting origin in the character map?
	// An LGR byte is split in 2. There's a 4-bit color in the low bits
	// at the top of the 16x16 dot square, and another 4-bit color in
	// the high bits at the bottom of the 16x16 dot square
	uvec2 byteOrigin;
	if ((fragOffset.y * 2.0) < float(tileSize.y))
	{
		// This is a top pixel, it uses the color of the 4 low bits
		byteOrigin = uvec2(0u, (byteVal & 0xFu) * 16u);
	} else {
		// It's a bottom pixel, it uses the color of the 4 high bits
		byteOrigin = uvec2(0u, (byteVal >> 4) * 16u);
	}

    // Now get the texture color, using the tile uv origin and this fragment's offset
    vec4 tex = texture(a2ModeTexture, (vec2(byteOrigin) + fragOffset) / vec2(textureSize2d));

    fragColor = tex;
	//fragColor = vec4(float(byteOrigin.x)/256.0, float(byteOrigin.y)/256.0, 0, 1);

    // fragColor = vec4(vColor, 1.f);   // for debugging
}
