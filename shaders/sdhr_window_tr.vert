#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in int aTexIdx;       // texture index (max 16)

out vec2 vTexCoord;
out int vTexIdx;

void main()
{
    vTexCoord = aTexCoord;
    vTexIdx = aTexIdx;
    gl_Position = vec4(aPos, 1.0); 
}