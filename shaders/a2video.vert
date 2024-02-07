#ifdef GL_ES
#define COMPAT_PRECISION mediump
precision mediump float;
#else
#define COMPAT_PRECISION
#endif

layout (location = 0) in vec2 aPos;         // -1,1 relative position
layout (location = 1) in vec2 pixelPos;     // pixel absolute position

out vec2 vFragPos;
// out vec3 vColor;    // DEBUG for non-textured display

uniform int ticks;      // ms since start

void main()
{
    // aPos is the normalized (-1, 1) coords of the vertex
    // In A2Video the window always spans the whole screen
    gl_Position = vec4(aPos.xy, 0.0, 1.0); 

    // pixelPos is the pixel coords of the vertex
    vFragPos = pixelPos;
}
