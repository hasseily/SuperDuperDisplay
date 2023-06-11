#version 330 core

precision mediump float;

// Global uniforms assigned in A2VideoManager
uniform sampler2D tilesTexture;
uniform int ticks;      // ms since start

// Window-level uniforms assigned in SDHRWindow
uniform vec2 windowTopLeft;    // Corners of window in model coordinates (pixels)
uniform vec2 windowBottomRight;

// Mesh-level uniforms assigned in MosaicMesh
uniform float maxTextures;
uniform float maxUVScale;
uniform uvec2 meshSize;          // mesh size in model coordinates (pixels)
uniform uvec2 tileCount;         // Count of tiles (cols, rows)
uniform sampler2D TBTEX;

in vec3 vFragPos;       // The fragment position in model coordinates (pixels)

out vec4 fragColor;

layout(pixel_center_integer) in vec4 gl_FragCoord;

/*
struct MosaicTile {
    vec2 uv;            // x and y
    float uvscale;      // z
    float attribs;       // w: char attributes: Normal, Inverse, Flashing
};
attribs are:
    0: normal
    1: inverse
    2: flashing
*/

void main()
{
    uvec2 tileSize = meshSize / tileCount;
    // first figure out which mosaic tile this fragment is part of
        // Calculate the position of the fragment in tile intervals
    vec2 fTileColRow = (vFragPos).xy / tileSize;
        // Row and column number of the tile containing this fragment
    ivec2 tileColRow = ivec2(floor(fTileColRow));
        // Fragment offset to tile origin, in pixels
    vec2 fragOffset = ((fTileColRow - tileColRow) * tileSize);

    // Next grab the data for that tile from the tilesBuffer
    // Make sure to rescale all values back from 0-1 to their original values
    vec4 mosaicTile = texture(TBTEX, tileColRow / vec2(tileCount));
    int attribs = int(round(mosaicTile.w * maxTextures));
    ivec2 textureSize2d = textureSize(tilesTexture,0);
    float scale = mosaicTile.z * maxUVScale;
    // no need to rescale the uvVals because we'll use them normalized

    // Now get the texture color, using the tile uv origin and this fragment's offset (with scaling)
    vec4 tex = texture(tilesTexture, mosaicTile.xy + (fragOffset * scale) / textureSize2d);

    int a_flash = int(attribs == 2);    // Set to 1 if flashing, 0 otherwise
    int a_norm = int(attribs == 0);     // Set to 1 if normal, 0 otherwise

    int isFlashing =  (ticks / 500) % 2;    // Flash every half second
    // get the color of normal or inverse
    fragColor = (a_norm * tex) + ((1 - a_norm) * (vec4(1) - tex));
    // get the color of flashing or the one above
    fragColor = (tex * a_flash * isFlashing) + (fragColor * (1 - a_flash));
}