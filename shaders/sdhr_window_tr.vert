#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in int aTexIdx;       // texture index (max 16)

out vec2 vTexCoord;
flat out int vTexIdx;
out vec3 vColor;    // DEBUG

uniform mat4 transform; // Transform from model to world space

// simple pseudo random function just for color coding the triangles for debug
float rand(vec2 co){
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

void main()
{
    vTexCoord = aTexCoord;
    vTexIdx = aTexIdx;
    gl_Position = transform * vec4(aPos, 1.0); 

    float r = rand(vec2((gl_VertexID % 0xFF) / 255.0f, (gl_VertexID >> 0x1) / 255.0f));
    float g = rand(vec2((gl_VertexID % 0xFF) / 255.0f, (gl_VertexID >> 0x2) / 255.0f));
    float b = rand(vec2((gl_VertexID >> 0x2) / 255.0f, (gl_VertexID >> 0x1) / 255.0f));
    vColor = vec3(      // DEBUG: Change the colors of each triangle to be better visible
        r, g, b
    );
}