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
 in the case that both modes exist in the same frame.

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

// merge layers are the following bits:
// 0: legacy
// 1: SHR
// 2: VidHD Text Modes (this layer is added in the vidHD layer
uniform int mergeLayers;

uniform sampler2D OFFSETTEX;					// Offset texture

uniform COMPAT_PRECISION int forceSHRWidth;		// force legacy to be shr width

uniform sampler2D LEGACYTEX;					// legacy output texture
uniform COMPAT_PRECISION vec2 legacySize;
uniform sampler2D SHRTEX;						// shr output texture
uniform COMPAT_PRECISION vec2 shrSize;

in vec2 vTexCoords;								// The fragment relative position
out vec4 fragColor;

void main()
{
	vec2 finalSize = shrSize;
	vec4 fragColorMerged;	// the color of the Apple 2 modes
	if ((mergeLayers & 0x3) == 0x3) {	// SHR+LEGACY
		float xOffset = texelFetch(OFFSETTEX, ivec2(0, gl_FragCoord.y/2.0), 0).r;
		if (xOffset < 0.0) {			// LEGACY part
			if (forceSHRWidth == 0) {	// Legacy is centered, in its original size
				fragColorMerged = texture(LEGACYTEX,
										vec2(vTexCoords.x*finalSize.x/legacySize.x - (finalSize.x-legacySize.x)/(legacySize.x*2.0f) + xOffset + 10.f,
											 vTexCoords.y * finalSize.y/legacySize.y)
									);
			} else {					// Legacy is stretched horizontally to the final size
				fragColorMerged = texture(LEGACYTEX,
										  vec2(vTexCoords.x * finalSize.x / shrSize.x + xOffset + 10.0,
											   vTexCoords.y * finalSize.y / shrSize.y));
			}
		} else {									// SHR part
			fragColorMerged = texture(SHRTEX, vec2(vTexCoords.x*finalSize.x/shrSize.x + xOffset - 10.f, vTexCoords.y*finalSize.y/shrSize.y));
		}
	} else if ((mergeLayers & 0x2) == 0x2) {		// SHR only
		fragColorMerged = texture(SHRTEX,
								  vec2(vTexCoords.x * finalSize.x/shrSize.x,
									   vTexCoords.y * finalSize.y/shrSize.y));
	} else if ((mergeLayers & 0x1) == 0x1) { 		// LEGACY only
		// Have to translate the origin because the default size is SHR's size
		fragColorMerged = texture(LEGACYTEX,
								  vec2(vTexCoords.x * finalSize.x/legacySize.x - (640.f-560.f)/(560.f*2.0f),
									   vTexCoords.y * finalSize.y/legacySize.y - (400.f-384.f)/(384.f*2.0f)));
	}	// SHR+LEGACY, SHR, LEGACY
	
	fragColor = fragColorMerged;

}
