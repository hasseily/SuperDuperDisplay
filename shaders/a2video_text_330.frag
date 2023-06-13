#version 330 core

precision mediump float;

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
const int textRow[24]= {
0x0000, 0x0080, 0x0100, 0x0180, 0x0200, 0x0280, 0x0300, 0x0380, 
0x0028, 0x00A8, 0x0128, 0x01A8, 0x0228, 0x26A8, 0x0328, 0x03A8, 
0x0050, 0x00D0, 0x0150, 0x01D0, 0x0250, 0x02D0, 0x0350, 0x03D0
};

// Global uniforms assigned in A2VideoManager
uniform sampler2D tilesTexture;
uniform int ticks;      // ms since start

// Window-level uniforms assigned in SDHRWindow
vec2 windowTopLeft = vec2(0);    // Corners of window in model coordinates (pixels)
uniform vec2 windowBottomRight;

// Mesh-level uniforms assigned in MosaicMesh
// TODO: Check which are still useful
uniform float maxTextures;
uniform float maxUVScale;
uniform uvec2 tileCount;         // Count of tiles (cols, rows)
uniform uvec2 tileSize;
uniform sampler2D DBTEX;

in vec3 vFragPos;       // The fragment position in model coordinates (pixels)

out vec4 fragColor;

layout(pixel_center_integer) in vec4 gl_FragCoord;

void main()
{
    // first figure out which mosaic tile this fragment is part of
        // Calculate the position of the fragment in tile intervals
    vec2 fTileColRow = (vFragPos).xy / tileSize;
        // Row and column number of the tile containing this fragment
    ivec2 tileColRow = ivec2(floor(fTileColRow));
        // Fragment offset to tile origin, in pixels
    vec2 fragOffset = ((fTileColRow - tileColRow) * tileSize);

    // Next grab the data for that tile from the tilesBuffer
    // No need to rescale values because we're using GL_R8UI
    vec4 vChar = texture(DBTEX, vec2(textRow[tileColRow.y] + tileColRow.x, 1));
    int char = int(vChar.x);    // the char byte value is just the x component

    // TODO: Check 0xC007 to switch to alternate charset

    // Determine from char which font glyph to use
    // and whether to inverse or flash
    // Determine if it's inverse when the char is below 0x40
    // And then if the char is below 0x80 and not inverse, it's flashing
    float a_inverse = 1.0 - step(0x40, char);
    float a_flash = (1.0 - step(0x80, char)) * (1.0 - a_inverse);

    ivec2 textureSize2d = textureSize(tilesTexture,0);
    // no need to rescale the uvVals because we'll use them normalized

    // Now get the texture color, using the tile uv origin and this fragment's offset (with scaling)
    vec4 tex = texture(tilesTexture, fragOffset);

    float isFlashing =  a_flash * ((ticks / 500) % 2);    // Flash every half second
    // get the color of flashing or the one above
    fragColor = ((1 - tex) * isFlashing) + (tex * (1 - isFlashing));
}