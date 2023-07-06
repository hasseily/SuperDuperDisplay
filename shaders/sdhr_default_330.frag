#version 330 core

precision mediump float;

// Global shdr uniforms assigned in SDHRManager
uniform sampler2D tilesTexture[14]; // It's always 2..16 (for GL_TEXTURE2->GL_TEXTURE16)
uniform int iDebugNoTextures;

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
float isInsideWindow(vec2 v, vec2 topLeft, vec2 bottomRight) {
    vec2 s = step(bottomRight, v) - step(topLeft, v);
    return s.x * s.y;
}

void main()
{
    // For testing
    // fragColor = vec4(vColor, 1);
    // return;

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
    int texIdx = int(round(mosaicTile.w * maxTextures));
    float scale = mosaicTile.z * maxUVScale;
    // no need to rescale the uvVals because we'll use them normalized
    // Now get the texture color, using the tile uv origin and this fragment's offset (with scaling)
        ivec2 textureSize2d = ivec2(0);
    vec4 tex = vec4(0);
    switch (texIdx) {
        case 0:
            textureSize2d = textureSize(tilesTexture[0],0);
            tex = texture(tilesTexture[0], mosaicTile.xy + (fragOffset * scale) / vec2(textureSize2d));
            break;
        case 1:
            textureSize2d = textureSize(tilesTexture[1],0);
            tex = texture(tilesTexture[1], mosaicTile.xy + (fragOffset * scale) / vec2(textureSize2d));
            break;
        case 2:
            textureSize2d = textureSize(tilesTexture[2],0);
            tex = texture(tilesTexture[2], mosaicTile.xy + (fragOffset * scale) / vec2(textureSize2d));
            break;
        case 3:
            textureSize2d = textureSize(tilesTexture[3],0);
            tex = texture(tilesTexture[3], mosaicTile.xy + (fragOffset * scale) / vec2(textureSize2d));
            break;
        case 4:
            textureSize2d = textureSize(tilesTexture[4],0);
            tex = texture(tilesTexture[4], mosaicTile.xy + (fragOffset * scale) / vec2(textureSize2d));
            break;
        case 5:
            textureSize2d = textureSize(tilesTexture[5],0);
            tex = texture(tilesTexture[5], mosaicTile.xy + (fragOffset * scale) / vec2(textureSize2d));
            break;
        case 6:
            textureSize2d = textureSize(tilesTexture[6],0);
            tex = texture(tilesTexture[6], mosaicTile.xy + (fragOffset * scale) / vec2(textureSize2d));
            break;
        case 7:
            textureSize2d = textureSize(tilesTexture[7],0);
            tex = texture(tilesTexture[7], mosaicTile.xy + (fragOffset * scale) / vec2(textureSize2d));
            break;
        case 8:
            textureSize2d = textureSize(tilesTexture[8],0);
            tex = texture(tilesTexture[8], mosaicTile.xy + (fragOffset * scale) / vec2(textureSize2d));
            break;
        case 9:
            textureSize2d = textureSize(tilesTexture[9],0);
            tex = texture(tilesTexture[9], mosaicTile.xy + (fragOffset * scale) / vec2(textureSize2d));
            break;
        case 10:
            textureSize2d = textureSize(tilesTexture[10],0);
            tex = texture(tilesTexture[10], mosaicTile.xy + (fragOffset * scale) / vec2(textureSize2d));
            break;
        case 11:
            textureSize2d = textureSize(tilesTexture[11],0);
            tex = texture(tilesTexture[11], mosaicTile.xy + (fragOffset * scale) / vec2(textureSize2d));
            break;
        case 12:
            textureSize2d = textureSize(tilesTexture[12],0);
            tex = texture(tilesTexture[12], mosaicTile.xy + (fragOffset * scale) / vec2(textureSize2d));
            break;
        case 13:
            textureSize2d = textureSize(tilesTexture[13],0);
            tex = texture(tilesTexture[13], mosaicTile.xy + (fragOffset * scale) / vec2(textureSize2d));
            break;
    }

    fragColor = tex * vTintColor * (1 - iDebugNoTextures)
                  + vec4(vColor, 1.f) * iDebugNoTextures;
}