#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in int aTexIdx;       // texture index (max 16)

out vec2 vTexCoord;
out int vTexIdx;
out vec3 vColor;    // DEBUG

uniform mat4 transform; // Transform from model to world space

void main()
{
    vTexCoord = aTexCoord;
    vTexIdx = aTexIdx;
    gl_Position = transform * vec4(aPos, 1.0); 

    vColor = vec3(      // DEBUG: Change the colors of each triangle to be better visible
        (gl_VertexID % 0xFF),
        ((gl_VertexID >> 0x1) % 0xFF),
        ((gl_VertexID >> 0x2) % 0xFF)
    );
}