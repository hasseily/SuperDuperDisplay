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
Regular Charset:
$00..$1F Inverse  Uppercase Letters (aka glyphs of ASCII $40..$5F)
$20..$3F Inverse  Symbols/Numbers   (aka glyphs of ASCII $20..$3F)
$40..$5F Flashing Uppercase Letters
$60..$7F Flashing Symbols/Numbers
$80..$9F Normal   Uppercase Letters (make ASCII control codes show up as letters)
$A0..$BF Normal   Symbols/Numbers   (like ASCII + $80)
$C0..$DF Normal   Uppercase Letters (like ASCII + $80)
$E0..$FF Normal   Lowercase Letters (like ASCII + $80)
Alternate Charset (0xC007):
$00..$1F Inverse  Uppercase Letters
$20..$3F Inverse  Symbols/Numbers
$40..$5F Inverse  MouseText
$60..$7F Inverse  Lowercase Letters
$80..$9F Normal   Uppercase Letters
$A0..$BF Normal   Symbols/Numbers   (like ASCII + $80)
$C0..$DF Normal   Uppercase Letters (like ASCII + $80)
$E0..$FF Normal   Lowercase Letters (like ASCII + $80)
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
uniform uint ticks;                  // ms since start
uniform COMPAT_PRECISION float hasFlashing;
uniform COMPAT_PRECISION float isMixed;		// Are we in mixed mode?
uniform COMPAT_PRECISION float isDouble;	// Are we in double res?

// Mesh-level uniforms assigned in MosaicMesh
uniform uvec2 tileSize;
uniform usampler2D APPLE2MEMORYTEX; // R8UI Apple 2e's memory, 1024 bytes wide
                                 // Unsigned int sampler!
uniform int memstart;			 // where to start in memory

uniform vec4 colorTint;

in vec2 vFragPos;       // The fragment position in pixels
// in vec3 vColor;         // DEBUG color, a mix of all 3 vertex colors

out vec4 fragColor;

void main()
{
	/* this could be used as an optimization
	if ((isMixed * vFragPos.y) < float(tileSize.y * 20))
	{
		// we're in mixed mode, then don't bother with the top 20 of 24 rows
		fragColor = vec4(0.0);
		return;
	}
	*/
	
    // first figure out which mosaic tile this fragment is part of
        // Calculate the position of the fragment in tile intervals
    vec2 fTileColRow = vFragPos / vec2(tileSize);
        // Row and column number of the tile containing this fragment
    ivec2 tileColRow = ivec2(floor(fTileColRow));

    // Next grab the data for that tile from the tilesBuffer
    // No need to rescale values because we're using GL_R8UI
    // The "texture" is split by 1kB-sized rows
	// In 80-col mode, the even bytes are pulled from aux mem,
	// and the odd bytes from main mem
	// Using _A2_MEMORY_SHADOW_END 0xC000
	int offset;
	if (isDouble > 0.0001)
		offset = memstart + (textRow[tileColRow.y] + tileColRow.x / 2) + (0xC000 * (1 - ((tileColRow.x) & 1)));
	else
		offset = memstart + textRow[tileColRow.y] + tileColRow.x;
    // the char byte value is just the r component
    uint charVal = texelFetch(APPLE2MEMORYTEX, ivec2(offset % 1024, offset / 1024), 0).r;
    float vCharVal = float(charVal);

    // Determine from char which font glyph to use
    // and if we need to flash
    // Determine if it's inverse when the char is below 0x40
    // And then if the char is below 0x80 and not inverse, it's flashing
    float a_inverse = 1.0 - step(float(0x40), vCharVal);
    float a_flash = (1.0 - step(float(0x80), vCharVal)) * (1.0 - a_inverse) * hasFlashing;

    ivec2 textureSize2d = textureSize(a2ModeTexture,0);
    // what's our character's starting origin in the character map?
	// We need to double the charOrigin and fragOffset when in DTEXT because we're
	// using the same texture for both TEXT and DTEXT. This moves the
	// charOrigin from the 7x16 tile to the correct 14x16 for the TEXT texture
    uvec2 charOrigin = uvec2(charVal & 0xFu, charVal >> 4) * tileSize;
	// Fragment offset to tile origin, in pixels
	vec2 fragOffset = ((fTileColRow - vec2(tileColRow)) * vec2(tileSize));
	if (isDouble > 0.0001)
	{
		charOrigin.x *= 2;
		fragOffset.x *= 2;
	}

    // Now get the texture color, using the tile uv origin and this fragment's offset
    vec4 tex = texture(a2ModeTexture, (vec2(charOrigin) + fragOffset) / vec2(textureSize2d)) * colorTint;

    float isFlashing =  a_flash * float((ticks / 310u) % 2u);    // Flash every 310ms
    // get the color of flashing or the one above
    fragColor = ((1.f - tex) * isFlashing) + (tex * (1.f - isFlashing));
	// fragColor = vec4(vColor, 1.f);   // for debugging
}
