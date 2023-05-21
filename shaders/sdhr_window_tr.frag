#version 330 core

uniform sampler2D tilesTexture[16];
uniform bool bDebugTextures;
in vec2 vTexCoord;
flat in int vTexIdx;    // the texture is the same for all pixels in the triangle
in vec3 vColor;         // DEBUG

out vec4 fragColor;

void main()
{
    if(bDebugTextures) {
        fragColor = texture(tilesTexture[vTexIdx], vTexCoord);
    } else {
        fragColor = vec4(vColor, 1.f);  // DEBUG
    }
}