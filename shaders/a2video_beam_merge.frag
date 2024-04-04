#ifdef GL_ES
#define COMPAT_PRECISION mediump
precision mediump float;
precision highp isampler2D;
precision highp usampler2D;
precision highp int;
#else
#define COMPAT_PRECISION
layout(pixel_center_integer) in vec4 gl_FragCoord;
#endif

/*
Apple 2 video beam shader for merging Legacy and SHR
in the case that both modes exist in the same frame

This shader expects as input a OFFSETTEX texture that has the following features:
- Type GL_R8I
- One byte per line
- Lines are the same count as the SHR lines (200 plus border)
- Each (signed) byte is the x offset the pixel needs to take from the textures to display

The shader goes through the following phases:
- The fragment grabs the OFFSETTEX byte given its vertical position
- The fragment returns a mix of legacy and SHR at the (x + offset) and y position
*/

// Global uniforms
uniform int ticks;						// ms since start
uniform isampler2D OFFSETTEX;			// Offset texture
uniform sampler2D legacyTex;			// legacy output texture
uniform sampler2D shrTex;				// shr output texture

in vec2 vFragPos;						// The fragment position in pixels
in vec2 vTexCoords;
out vec4 fragColor;

void main()
{
	int xOffset = texelFetch(OFFSETTEX, ivec2(0, int(vFragPos.y / 2.0)), 0).r;
    fragColor = mix(texture(legacyTex, vTexCoords), texture(shrTex, vTexCoords), 0.5f);
}
