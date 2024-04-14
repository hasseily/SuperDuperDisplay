#ifdef GL_ES
#define COMPAT_PRECISION mediump
precision highp float;
precision highp sampler2D;
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
- Each float is the x offset the pixel needs to take from the textures to display,
	but needs to remove 10.f to get the actual offset. A negative offset is a legacy line
	and a positive offset is a shr line
*/

// Global uniforms
uniform COMPAT_PRECISION int ticks;				// ms since start
uniform sampler2D OFFSETTEX;					// Offset texture
uniform COMPAT_PRECISION int forceSHRWidth;		// force legacy to be shr width
uniform COMPAT_PRECISION int shrScanlineCount;	// amount of SHR scanlines (size of OFFSETTEX)

uniform sampler2D legacyTex;					// legacy output texture
uniform COMPAT_PRECISION vec2 legacySize;
uniform sampler2D shrTex;						// shr output texture
uniform COMPAT_PRECISION vec2 shrSize;

in vec2 vTexCoords;								// The fragment relative position
out vec4 fragColor;

void main()
{
	float xOffset = texelFetch(OFFSETTEX, ivec2(0, gl_FragCoord.y/2.0), 0).r;
	if (xOffset < 0.0) {			// LEGACY
		if (forceSHRWidth == 0) {	// Legacy is centered, in its original size
			fragColor = texture(legacyTex, 
									vec2(vTexCoords.x*shrSize.x/legacySize.x - (shrSize.x-legacySize.x)/(legacySize.x*2.0f) + xOffset + 10.f,
										 vTexCoords.y * shrSize.y/legacySize.y)
								);
		} else {					// Legacy is stretched horizontally to SHR size
			fragColor = texture(legacyTex, 
								vec2(vTexCoords.x + xOffset + 10.f, 
									 vTexCoords.y * shrSize.y/legacySize.y));
		}
	} else {						// SHR
		fragColor = texture(shrTex, vec2(vTexCoords.x + xOffset - 10.f, vTexCoords.y));
	}
}
