#version 330 core

precision mediump float;

// Global shdr uniforms assigned in SDHRManager
uniform mat4 transform; // Mesh transform from model to world space (also in vertex shader)
uniform sampler2D tilesTexture[16];
uniform int iDebugNoTextures;
uniform vec2 vTexelSize;        // render output size divided by SDHR resolution size

// Window-level uniforms assigned in SDHRWindow
uniform vec2 windowBottomLeft;  // The stencil window coordinates in SDHR (pixel) space
uniform vec2 windowTopRight;

// Mesh-level uniforms assigned in MosaicMesh
uniform uvec2 meshSize;          // mesh size in pixels
uniform uvec2 tileCount;         // Count of tiles (cols, rows)
uniform vec3 topLeftVertexPos;  // the position of the top left vertex
uniform samplerBuffer TBTEX;

vec4 vStencilFailColor = vec4(0);   // Change this to show the stencil cutouts in color

in vec3 vFragPos;       // The fragment position in world coordinates
in vec4 vTintColor;     // The mixed vertex colors for tinting
in vec3 vColor;         // DEBUG color, a mix of all 3 vertex colors

out vec4 fragColor;

layout(pixel_center_integer) in vec4 gl_FragCoord;

/*
struct MosaicTile {
    vec2 uv;            // x and y
    float uvscale;      // z
    float texIdx;       // w
};
*/

// return 1 if the fragment v of the MosaicMesh is inside the Window, return 0 otherwise
// Essentially a stencil test
float isInsideWindow(vec2 v, vec2 bottomLeft, vec2 topRight) {
    vec2 s = step(bottomLeft, v) - step(topRight, v);
    return s.x * s.y;
}

void main()
{
    //fragColor = vec4(1);
    //return;
    uvec2 tileSize = meshSize / tileCount;
    // first figure out which mosaic tile this fragment is part of
        // Calculate the position of the fragment in tile intervals
    vec2 fTileColRow = (vFragPos).xy / tileSize;
        // Row and column number of the tile containing this fragment
    uvec2 tileColRow = uvec2(uint(fTileColRow.x), uint(fTileColRow.y));
        // Actual tile index for calculating the buffer offsets for texelFetch()
    int tileIdx = int(tileColRow.y * tileCount.x + tileColRow.x);

    // Next grab the data for that tile from the tilesBuffer
    vec4 mosaicTile = texelFetch(TBTEX, tileIdx);

    // We've got the texture index
    int texIdx = int(mosaicTile.w);

    // Fragment offset to tile origin, in pixels
    vec2 fragOffset = (fTileColRow - tileColRow) * tileSize;

    // Now get the texture color, using the tile uv origin and this fragment's offset (with scaling)
    // TODO: WHY TEXIDX+1? Should be TexIdx
    ivec2 textureSize2d = textureSize(tilesTexture[texIdx+1],0);
    vec4 tex = texture(tilesTexture[texIdx+1], mosaicTile.xy + (fragOffset * mosaicTile.z) / textureSize2d);

    // TODO: Understand if the mesh size is correct
    // Understand every single other parameter
    //tex.r = fTileColRow.x - tileColRow.x;
    //tex.g = fTileColRow.y - tileColRow.y;
    //tex.b = texIdx;
    //tex.a = 1;
    if(tex.a < 0.1)
        discard;

    // Check if the fragment is inside the window
    float t = isInsideWindow(
        gl_FragCoord.xy,
        windowBottomLeft * vTexelSize,
        windowTopRight * vTexelSize
    );
    t = 1.0;    // XXX Disabled culling
    fragColor = (t * tex * vTintColor + (1 - t) * vStencilFailColor) * (1 - iDebugNoTextures)
                                         + vec4(vColor, 1.f) * iDebugNoTextures;
    
}