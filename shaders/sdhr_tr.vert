#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in int aTexIdx;       // texture index (max 16)

// the meshIndex is the index of the window for z-depth
uniform int meshIndex;

out vec2 vTexCoord;
out int vTexIdx;

void main()
{
    vTexCoord = aTexCoord;
    vTexIdx = aTexIdx;
    gl_Position = vec4(aPos, meshIndex / 256.0, 1.0); 
}