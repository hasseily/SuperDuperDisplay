#version 330 core

uniform sampler2D tilesTexture[16];
in vec2 vTexCoord;
flat in int vTexIdx;    // the texture is the same for all pixels in the triangle

out vec4 fragColor;

void main()
{
    fragColor = texture(tilesTexture[vTexIdx], vTexCoord);
//    fragColor = vec4 (1.f, 0.3f, 0.6f, 1.0f);    // TODO: Debug Pink
}