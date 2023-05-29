#version 330 core

precision mediump float;

layout (location = 0) in vec4 aPos;
layout (location = 1) in vec4 aTintColor;

out vec2 vTexCoord;
out vec4 vTintColor;
out vec3 vColor;    // DEBUG for non-textured display

out vec3 vFragPos;

uniform mat4 model;     // model matrix
uniform mat4 transform; // Final mesh transform matrix from model to world space

// simple pseudo random function just for color coding the triangles for debug
float rand(vec2 co){
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

void main()
{
    // Move the mesh vertices with the transform
    // Replace w with 1.0 as the vertices are normalized, and we're using w
    // just to flag that the vertex is the top left corner of the mesh
    gl_Position = transform * vec4(aPos.xyz, 1.0); 

    // Calculate the transformed mesh origin
    // So the fragment can determine which mosaic tile it's part of
    vec4 worldPos = model * vec4(aPos.xyz, 1.0);
    //vFragPos = worldPos.xyz;
    vFragPos = aPos.xyz;

    // The tint color is passed in to tint each vertex
    vTintColor = aTintColor;

    // This below is just to create a random vertex color for debugging untextured triangles
    float r = rand(vec2((gl_VertexID % 0xFF) / 255.0f, (gl_VertexID >> 0x1) / 255.0f));
    float g = rand(vec2((gl_VertexID % 0xFF) / 255.0f, (gl_VertexID >> 0x2) / 255.0f));
    float b = rand(vec2((gl_VertexID >> 0x2) / 255.0f, (gl_VertexID >> 0x1) / 255.0f));
    vColor = vec3(      // DEBUG: Change the colors of each triangle to be better visible
        r, g, b
    );
}