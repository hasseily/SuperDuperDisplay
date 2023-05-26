#version 330 core

uniform sampler2D tilesTexture[16];
uniform int iDebugNoTextures;
uniform vec2 windowBottomLeft;  // The stencil window coordinates in SDHR (pixel) space
uniform vec2 windowTopRight;
uniform vec2 vTexelSize;        // render output size divided by SDHR resolution size

vec4 vStencilFailColor = vec4(0);

in vec2 vTexCoord;
flat in int iTexIdx;    // the texture is the same for all pixels in the triangle
in vec3 vColor;         // DEBUG color, a mix of all 3 vertex colors

out vec4 fragColor;

// return 1 if the fragment v of the MosaicMesh is inside the Window, return 0 otherwise
// Essentially a stencil test
float isInsideWindow(vec2 v, vec2 bottomLeft, vec2 topRight) {
    vec2 s = step(bottomLeft, v) - step(topRight, v);
    return s.x * s.y;
}

void main()
{
    // Check if the fragment is inside the window
    float t = isInsideWindow(
        gl_FragCoord.xy,
        windowBottomLeft * vTexelSize,
        windowTopRight * vTexelSize
    );

    vec4 tex = texture(tilesTexture[iTexIdx], vTexCoord);   // The actual texture color

    fragColor = (t * tex + (1 - t) * vStencilFailColor) * (1 - iDebugNoTextures)
                                         + vec4(vColor, 1.f) * iDebugNoTextures;

}