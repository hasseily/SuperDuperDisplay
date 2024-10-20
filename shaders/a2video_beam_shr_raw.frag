#ifdef GL_ES
#define COMPAT_PRECISION mediump
precision mediump float;
precision highp usampler2D;
precision highp int;
#else
#define COMPAT_PRECISION
layout(pixel_center_integer) in vec4 gl_FragCoord;
#endif

/*
 Apple 2gs video beam shader for SHR using raw Color Filter Array (CFA) RGGB images.
 It doesn't use the palettes, and instead every byte is 2 colors (RG or GB depending on
 the line it's on -- RG for even, GB for odd)
 See the standard SHR shader for more info.
 */


// Global uniforms assigned in A2VideoManager
uniform int ticks;              // ms since start
uniform int hborder;			// horizontal border in cycles
uniform int vborder;			// vertical border in scanlines
uniform usampler2D VRAMTEX;		// Video RAM texture

in vec2 vFragPos;       // The fragment position in pixels
out vec4 fragColor;

// Monitor color type
// enum A2VideoMonitorType_e
// {
// 		A2_MON_COLOR = 0,
// 		A2_MON_WHITE,
// 		A2_MON_GREEN,
// 		A2_MON_AMBER,
//		A2_MON_TOTAL_COUNT
// };

uniform int monitorColorType;
// colors for monitor color types
const vec4 monitorcolors[5] = vec4[5](
	vec4(0.000000,	0.000000,	0.000000,	1.000000)	/*BLACK, -- this is a color monitor */
	,vec4(1.000000,	1.000000,	1.000000,	1.000000)	/*WHITE PHOSPHOR,*/
	,vec4(0.290196,	1.000000,	0.000000,	1.000000)	/*GREEN PHOSPHOR,*/
	,vec4(1.000000,	0.717647,	0.000000,	1.000000)	/*AMBER PHOSPHOR,*/
	,vec4(1.000000,	0.000000,	0.500000,	1.000000)	/*PINK, -- this option shouldn't exist */
);


const vec4 bordercolors[16] = vec4[16] (
    vec4(0.00, 0.00, 0.00, 1.0), // BLACK
    vec4(0.67, 0.07, 0.30, 1.0), // DEEP_RED
    vec4(0.00, 0.03, 0.51, 1.0), // DARK_BLUE
    vec4(0.67, 0.10, 0.82, 1.0), // MAGENTA
    vec4(0.00, 0.51, 0.18, 1.0), // DARK_GREEN
    vec4(0.62, 0.59, 0.49, 1.0), // DARK_GRAY
    vec4(0.00, 0.54, 0.71, 1.0), // BLUE
    vec4(0.62, 0.62, 1.00, 1.0), // LIGHT_BLUE
    vec4(0.48, 0.37, 0.00, 1.0), // BROWN
    vec4(1.00, 0.45, 0.28, 1.0), // ORANGE
    vec4(0.47, 0.41, 0.49, 1.0), // LIGHT_GRAY
    vec4(1.00, 0.48, 0.81, 1.0), // PINK
    vec4(0.43, 0.90, 0.17, 1.0), // GREEN
    vec4(1.00, 0.96, 0.48, 1.0), // YELLOW
    vec4(0.42, 0.93, 0.70, 1.0), // AQUA
    vec4(1.00, 1.00, 1.00, 1.0)  // WHITE
);

// If the monitor is monochrome, get the luminance (greyscale) value of the color
// and apply it to the monochrome value
vec4 GetMonochromeValue(vec4 aColor, vec4 monchromeColor)
{
	float luminance = dot(aColor.rgb, vec3(0.299, 0.587, 0.114));
	return vec4(monchromeColor.rgb * luminance, aColor.a);
}

void main()
{
	uint scanline = uint(vFragPos.y) / 2u;
	// first do the borders
	if ((vFragPos.y < float(vborder*2)) || (vFragPos.y >= float(vborder*2+400)) || 
		(vFragPos.x < float(hborder*16)) || (vFragPos.x >= float(640+hborder*16)))
	{
		fragColor = bordercolors[texelFetch(VRAMTEX, ivec2(33u + uint(float(vFragPos.x) / 4.0), scanline), 0).r & 0x0Fu];
		if (monitorColorType > 0)
			fragColor = GetMonochromeValue(fragColor, monitorcolors[monitorColorType]);
		return;
	}
	
	// We want raw Color Filter Array (CFA) RGGB images, so we need to "demosaic".
	// Each byte is 2 pixels. If it's in an even scanline, the byte has 2 pixels: R and G
	// If in an odd scanline, the byte has 2 other pixels: G and B
	// Also we always have pixel-doubled vertical images for all Apple 2 modes
	// so we need to be careful to duplicate each line

	// https://demo.ipol.im/demo/g_malvar_he_cutler_linear_image_demosaicking
	
	/*
	 THIS IS A BASIC DEMOSAICKING USING JUST 2x2 PIXELS OF RGGB
	 uint xpos = uint(vFragPos.x);
	 uint byteVal = texelFetch(VRAMTEX, ivec2(33u + uint(float(xpos) / 4.0), scanline), 0).r;
	 uint byteVal2 = byteVal;
	 if ((scanline % 2u) == 0u)	// first line of 2
	 {
	 byteVal2 = texelFetch(VRAMTEX, ivec2(33u + uint(float(xpos) / 4.0), scanline+1), 0).r;
	 }
	 else
	 {
	 byteVal = texelFetch(VRAMTEX, ivec2(33u + uint(float(xpos) / 4.0), scanline-1), 0).r;
	 }
	 if ((2u * scanline) != uint(vFragPos.y))
	 {
	 scanline -= 1;
	 }
	 float _red = float((byteVal & 0xF0u) >> 4);
	 float _gr1 = float(byteVal & 0x0Fu);
	 float _gr2 = float((byteVal2 & 0xF0u) >> 4);
	 float _blue = float(byteVal2 & 0x0Fu);
	 _red /= 16.0;
	 _gr1 /= 16.0;
	 _gr2 /= 16.0;
	 _blue /= 16.0;
	 
	 fragColor.r = _red;
	 fragColor.b = _blue;
	 // We reduce the Greens a LOT (by 4x)
	 if ((uint(vFragPos.y) % 4) < 2)	// even y (the lines are doubled)
	 {
	 if ((xpos % 2u) == 0)	// even x
	 {
	 // Top-left pixel (Red)
	 fragColor.g = (_gr1 + _gr2)/8.0;
	 } else {
	 // Top-right pixel (Green1)
	 fragColor.g = _gr1/4.0;
	 }
	 } else {	// odd y
	 if ((xpos % 2u) == 0)	// even x
	 {
	 // Bottom-left pixel (Green2)
	 fragColor.g = _gr2/4.0;
	 } else {
	 // Bottom-right pixel (Blue)
	 fragColor.g = (_gr1 + _gr2)/8.0;
	 }
	 }
	 fragColor *= 4.5;	// we reincrease luminance by 4.5x
	 fragColor = clamp(fragColor, 0.0, 1.0);

	 */
	
	/*
	 We have the following cases:

	 R at red locations		(just get its value)
	 G at red locations
	 B at red locations

	 G at any of the green locations	(just get its value)
	 R at green locations in red rows (even rows)
	 B at green locations in red rows (even rows)
	 R at green locations in blue rows (odd rows)
	 B at green locations in blue rows (odd rows)

	 R at blue locations
	 G at blue locations
	 Blue at blue locations	(just get its value)
	*/
	
	uint xpos = uint(vFragPos.x);
	uint ypos = uint(vFragPos.y);
	// Note that the VRAM fetches use "scanline" and not ypos
	// because the lines are doubled. Each scanline is for 2 consecutive ypos
	
	// For the algorithm, we need to fetch 9 bytes. Which ones around our pixel depend
	// on the XY of the RGGB 2x2 square that we're in
	
	uint byteVal_AL = texelFetch(VRAMTEX, ivec2(33u + uint(float(xpos) / 4.0) - 1, scanline - 1), 0).r;
	uint byteVal_A = texelFetch(VRAMTEX, ivec2(33u + uint(float(xpos) / 4.0), scanline - 1), 0).r;
	uint byteVal_C = texelFetch(VRAMTEX, ivec2(33u + uint(float(xpos) / 4.0), scanline), 0).r;
	uint byteVal_R = texelFetch(VRAMTEX, ivec2(33u + uint(float(xpos) / 4.0) + 1, scanline), 0).r;
	uint byteVal_B = texelFetch(VRAMTEX, ivec2(33u + uint(float(xpos) / 4.0), scanline + 1), 0).r;
	
	ivec2 originByte = ivec2(33u + uint(float(xpos) / 4.0), scanline);
	
	// ALL COLORS ARE SCALED BY 8.0
	if (((xpos % 2) == 0) && ((scanline % 2) == 0))
	{
		// top left corner: red location, even row
		uint byteVal_1_0 = texelFetch(VRAMTEX,originByte+ivec2(0,-2),0).r;
		uint byteVal_0_1 = texelFetch(VRAMTEX,originByte+ivec2(-1,-1),0).r;
		uint byteVal_1_1 = texelFetch(VRAMTEX,originByte+ivec2(0,-1),0).r;
		uint byteVal_0_2 = texelFetch(VRAMTEX,originByte+ivec2(-1,0),0).r;
		uint byteVal_1_2 = texelFetch(VRAMTEX,originByte,0).r;
		uint byteVal_2_2 = texelFetch(VRAMTEX,originByte+ivec2(+1,0),0).r;
		uint byteVal_0_3 = texelFetch(VRAMTEX,originByte+ivec2(-1,+1),0).r;
		uint byteVal_1_3 = texelFetch(VRAMTEX,originByte+ivec2(0,+1),0).r;
		uint byteVal_1_4 = texelFetch(VRAMTEX,originByte+ivec2(0,+2),0).r;

		fragColor.r = 	float((byteVal_1_2 & 0xF0u) >> 4) * 8.0;
		fragColor.g =	float((byteVal_1_0 & 0xF0u) >> 4) * -1.0 +
						float((byteVal_1_1 & 0xFu)) * 2.0 +
						float((byteVal_0_2 & 0xF0u) >> 4) * -1.0 +
						float((byteVal_0_2 & 0xFu)) * 2.0 +
						float((byteVal_1_2 & 0xF0u) >> 4) * 4.0 +
						float((byteVal_1_2 & 0xFu)) * 2.0 +
						float((byteVal_2_2 & 0xF0u) >> 4) * -1.0 +
						float((byteVal_1_3 & 0xFu)) * 2.0 +
						float((byteVal_1_4 & 0xF0u) >> 4) * -1.0;
		fragColor.b =	float((byteVal_1_0 & 0xF0u) >> 4) * -1.5 +
						float((byteVal_0_1 & 0xFu)) * 2.0 +
						float((byteVal_1_1 & 0xFu)) * 2.0 +
						float((byteVal_0_2 & 0xF0u) >> 4) * -1.5 +
						float((byteVal_1_2 & 0xF0u) >> 4) * 6.0 +
						float((byteVal_2_2 & 0xF0u) >> 4) * -1.5 +
						float((byteVal_0_3 & 0xFu)) * 2.0 +
						float((byteVal_1_3 & 0xFu)) * 2.0 +
						float((byteVal_1_4 & 0xF0u) >> 4) * -1.5;

	} else if (((xpos % 2) == 0) && ((scanline % 2) == 1))
	{
		// top right corner: green location, even row
		uint byteVal_1_0 = texelFetch(VRAMTEX,originByte+ivec2(0,-2),0).r;
		uint byteVal_1_1 = texelFetch(VRAMTEX,originByte+ivec2(0,-1),0).r;
		uint byteVal_2_1 = texelFetch(VRAMTEX,originByte+ivec2(+1,-1),0).r;
		uint byteVal_0_2 = texelFetch(VRAMTEX,originByte+ivec2(-1,0),0).r;
		uint byteVal_1_2 = texelFetch(VRAMTEX,originByte,0).r;
		uint byteVal_2_2 = texelFetch(VRAMTEX,originByte+ivec2(+1,0),0).r;
		uint byteVal_1_3 = texelFetch(VRAMTEX,originByte+ivec2(0,+1),0).r;
		uint byteVal_2_3 = texelFetch(VRAMTEX,originByte+ivec2(+1,+1),0).r;
		uint byteVal_1_4 = texelFetch(VRAMTEX,originByte+ivec2(0,+2),0).r;
		
		fragColor.r =	float((byteVal_1_0 & 0xFu)) * 0.5 +
						float((byteVal_1_1 & 0xF0u) >> 4) * -1.0 +
						float((byteVal_2_1 & 0xF0u) >> 4) * -1.0 +
						float((byteVal_0_2 & 0xFu)) * -1.0 +
						float((byteVal_1_2 & 0xF0u) >> 4) * 4.0 +
						float((byteVal_1_2 & 0xFu)) * 5.0 +
						float((byteVal_2_2 & 0xF0u) >> 4) * 4.0 +
						float((byteVal_2_2 & 0xFu)) * -1.0 +
						float((byteVal_1_3 & 0xF0u) >> 4) * -1.0 +
						float((byteVal_2_3 & 0xF0u) >> 4) * -1.0 +
						float((byteVal_1_4 & 0xFu)) * 0.5;
		fragColor.g = 	float(byteVal_1_2 & 0xFu) * 8.0;
		fragColor.b =	float((byteVal_1_0 & 0xFu)) * -1.0 +
						float((byteVal_1_1 & 0xF0u) >> 4) * -1.0 +
						float((byteVal_1_1 & 0xFu)) * 4.0 +
						float((byteVal_2_1 & 0xF0u) >> 4) * -1.0 +
						float((byteVal_0_2 & 0xFu)) * 0.5 +
						float((byteVal_1_2 & 0xFu)) * 5.0 +
						float((byteVal_2_2 & 0xFu)) * 0.5 +
						float((byteVal_1_3 & 0xF0u) >> 4) * -1.0 +
						float((byteVal_1_3 & 0xFu)) * 4.0 +
						float((byteVal_2_3 & 0xF0u) >> 4) * -1.0 +
						float((byteVal_1_4 & 0xFu)) * -1.0;

	} else if (((xpos % 2) == 1) && ((scanline % 2) == 0))
	{
		// bottom left corner: green location, odd row
		uint byteVal_1_0 = texelFetch(VRAMTEX,originByte+ivec2(0,-2),0).r;
		uint byteVal_0_1 = texelFetch(VRAMTEX,originByte+ivec2(-1,-1),0).r;
		uint byteVal_1_1 = texelFetch(VRAMTEX,originByte+ivec2(0,-1),0).r;
		uint byteVal_0_2 = texelFetch(VRAMTEX,originByte+ivec2(-1,0),0).r;
		uint byteVal_1_2 = texelFetch(VRAMTEX,originByte,0).r;
		uint byteVal_2_2 = texelFetch(VRAMTEX,originByte+ivec2(+1,0),0).r;
		uint byteVal_0_3 = texelFetch(VRAMTEX,originByte+ivec2(-1,+1),0).r;
		uint byteVal_1_3 = texelFetch(VRAMTEX,originByte+ivec2(0,+1),0).r;
		uint byteVal_1_4 = texelFetch(VRAMTEX,originByte+ivec2(0,+2),0).r;
		
		fragColor.r =	float((byteVal_1_0 & 0xF0u) >> 4) * -1.0 +
						float((byteVal_0_1 & 0xFu)) * -1.0 +
						float((byteVal_1_1 & 0xF0u) >> 4) * 4.0 +
						float((byteVal_1_1 & 0xFu)) * -1.0 +
						float((byteVal_0_2 & 0xF0u) >> 4) * 0.5 +
						float((byteVal_1_2 & 0xF0u) >> 4) * 5.0 +
						float((byteVal_2_2 & 0xF0u) >> 4) * 0.5 +
						float((byteVal_0_3 & 0xFu)) * -1.0 +
						float((byteVal_1_3 & 0xF0u) >> 4) * 4.0 +
						float((byteVal_1_3 & 0xFu)) * -1.0 +
						float((byteVal_1_4 & 0xF0u) >> 4) * -1.0;
		fragColor.g = 	float((byteVal_1_2 & 0xF0u) >> 4) * 8.0;
		fragColor.b =	float((byteVal_1_0 & 0xF0u) >> 4) * 0.5 +
						float((byteVal_0_1 & 0xFu)) * -1.0 +
						float((byteVal_1_1 & 0xFu)) * -1.0 +
						float((byteVal_0_2 & 0xF0u) >> 4) * -1.0 +
						float((byteVal_0_2 & 0xFu)) * 4.0 +
						float((byteVal_1_2 & 0xF0u) >> 4) * 5.0 +
						float((byteVal_1_2 & 0xFu)) * 4.0 +
						float((byteVal_2_2 & 0xF0u) >> 4) * -1.0 +
						float((byteVal_0_3 & 0xFu)) * -1.0 +
						float((byteVal_1_3 & 0xFu)) * -1.0 +
						float((byteVal_1_4 & 0xF0u) >> 4) * 0.5;
	} else
	{
		// bottom right corner: blue location, odd row
		uint byteVal_1_0 = texelFetch(VRAMTEX,originByte+ivec2(0,-2),0).r;
		uint byteVal_1_1 = texelFetch(VRAMTEX,originByte+ivec2(0,-1),0).r;
		uint byteVal_2_1 = texelFetch(VRAMTEX,originByte+ivec2(+1,-1),0).r;
		uint byteVal_0_2 = texelFetch(VRAMTEX,originByte+ivec2(-1,0),0).r;
		uint byteVal_1_2 = texelFetch(VRAMTEX,originByte,0).r;
		uint byteVal_2_2 = texelFetch(VRAMTEX,originByte+ivec2(+1,0),0).r;
		uint byteVal_1_3 = texelFetch(VRAMTEX,originByte+ivec2(0,+1),0).r;
		uint byteVal_2_3 = texelFetch(VRAMTEX,originByte+ivec2(+1,+1),0).r;
		uint byteVal_1_4 = texelFetch(VRAMTEX,originByte+ivec2(0,+2),0).r;
		fragColor.r = 	float((byteVal_1_0 & 0xFu)) * -1.5 +
						float((byteVal_1_1 & 0xF0u) >> 4) * 2.0 +
						float((byteVal_2_1 & 0xF0u) >> 4) * 2.0 +
						float((byteVal_0_2 & 0xFu)) * -1.5 +
						float((byteVal_1_2 & 0xFu)) * 6.0 +
						float((byteVal_2_2 & 0xFu)) * -1.5 +
						float((byteVal_1_3 & 0xF0u) >> 4) * 2.0 +
						float((byteVal_2_3 & 0xF0u) >> 4) * 2.0 +
						float((byteVal_1_4 & 0xFu)) * -1.5;
		fragColor.g =	float((byteVal_1_0 & 0xFu)) * -1.0 +
						float((byteVal_1_1 & 0xFu)) * 2.0 +
						float((byteVal_0_2 & 0xFu)) * -1.0 +
						float((byteVal_1_2 & 0xF0u) >> 4) * 2.0 +
						float((byteVal_1_2 & 0xFu)) * 4.0 +
						float((byteVal_2_2 & 0xF0u) >> 4) * 2.0 +
						float((byteVal_2_2 & 0xFu)) * -1.0 +
						float((byteVal_1_3 & 0xFu)) * 2.0 +
						float((byteVal_1_4 & 0xFu)) * -1.0;
		fragColor.b =	float(byteVal_1_2 & 0xFu) * 8.0;
	}
	
	fragColor /= (8.0 * 16.0);
	fragColor.a = 1.0;
	fragColor = clamp(fragColor, 0.0, 1.0);
	
	if (monitorColorType > 0)
		fragColor = GetMonochromeValue(fragColor, monitorcolors[monitorColorType]);
}
