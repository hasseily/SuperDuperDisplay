#version 330 core

precision mediump float;

layout (location = 0) in vec4 aPos;

out vec3 vFragPos;

uniform int ticks;      // ms since start
uniform mat4 transform; // Final mesh transform matrix from model to world space

void main()
{
    // Move the mesh vertices with the transform
    // Replace w with 1.0 as the vertices are normalized, and we're using w
    // just to flag that the vertex is the top left corner of the mesh
    gl_Position = transform * vec4(aPos.xyz, 1.0); 

    // This is for the fragment can determine which mosaic tile it's part of
    vFragPos = aPos.xyz;
}