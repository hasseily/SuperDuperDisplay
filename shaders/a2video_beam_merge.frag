#ifdef GL_ES
#define COMPAT_PRECISION mediump
precision highp float;
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
- Type GL_R32F
- One byte per line (Red channel only)
- Lines are the same count as the SHR lines (200 plus border)
- Each float is the x offset the pixel needs to take from the textures to display

The shader goes through the following phases:
- The fragment grabs the OFFSETTEX byte given its vertical position
- The fragment returns a mix of legacy and SHR at the (x + offset) and y position
*/

// Global uniforms
uniform int ticks;						// ms since start
uniform sampler2D OFFSETTEX;			// Offset texture
uniform int shrScanlineCount;			// amount of SHR scanlines (size of OFFSETTEX)

uniform sampler2D legacyTex;			// legacy output texture
uniform vec2 legacySize;
uniform sampler2D shrTex;				// shr output texture
uniform vec2 shrSize;

in vec2 vTexCoords;						// The fragment relative position
out vec4 fragColor;

void main()
{
	float xOffset = texelFetch(OFFSETTEX, ivec2(0, gl_FragCoord.y/2.0), 0).r;
	if (xOffset < 0.0)
		fragColor = texture(legacyTex, vec2(vTexCoords.x + xOffset + 10.f, vTexCoords.y * shrSize.y/legacySize.y));
	else
		fragColor = texture(shrTex, vec2(vTexCoords.x + xOffset - 10.f, vTexCoords.y));
	//fragColor = vec4(abs(xOffset), 1.0, 0.0, 1.0);
}
