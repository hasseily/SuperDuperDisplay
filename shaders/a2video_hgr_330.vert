#version 330 core

precision mediump float;

layout (location = 0) in vec2 aPos;         // -1,1 relative position
layout (location = 1) in vec2 pixelPos;     // pixel absolute position

out vec2 vFragPos;

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

}