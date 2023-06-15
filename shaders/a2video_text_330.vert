#version 330 core

precision mediump float;

layout (location = 0) in vec2 aPos;         // -1,1 relative position
layout (location = 1) in vec2 pixelPos;     // pixel absolute position

out vec2 vFragPos;
out vec3 vColor;    // DEBUG for non-textured display

uniform int index;
uniform int ticks;      // ms since start

void main()
{
    // aPos is the normalized (-1, 1) coords of the vertex
    // In A2Video the window always spans the whole screen
    gl_Position = vec4(aPos.xy, float(index), 1.0); 

    // pixelPos is the pixel coords of the vertex
    // This is for the fragment can determine which mosaic tile it's part of
    vFragPos = pixelPos;

    // This below is just to create a random vertex color for debugging untextured triangles
    float r = float(int(gl_VertexID * 204.95f) % 0xFF) / 255.f; 
    float g = float(int(gl_VertexID * 182.53f * abs(aPos.x)) % 0xFF) / 255.f; 
    float b = float(int(gl_VertexID * 359.65f * abs(aPos.y)) % 0xFF) / 255.f; 
    vColor = vec3(      // DEBUG: Change the colors of each triangle to be better visible
        r, g, b
    );
}