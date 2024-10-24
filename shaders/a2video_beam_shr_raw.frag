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
 Apple 2gs video beam shader for SHR.
 
 This shader expects as input a VRAMTEX texture that is a GL_R8UI byte buffer.
 It is a series of 193 byte lines.
 On each line:
 - the first byte is the SCB of the line
 - the next 32 bytes is the color palette (16 colors of 2 bytes each)
 - the last 160 bytes are the SHR scanline byte

 The SCB and palette are loaded at the start of the line by the beam generator,
 so they are always fixed for the line. This has been verified on original hardware
 by a number of people.
 
 As a reminder, the SCB has the following bits:
 Bits 0-3 Palette number
 Bit 4 Reserved (defaults to 0)
 Bit 5 Mode fill (1=on, 0=off)
 Bit 6 Interrupt (1=requested, 0=not requested)
 Bit 7 Horizontal pixel count (0=320px, 1=640px)
 
 We set bit 4 of the SCB in the GPU to state if the line is unused.
 If so, then we make the whole line transparent.
 
 Also note that the colorfill situation has been precalculated when generating the VRAM.
 
 The shader goes through the following phases:
 - The fragment determines which VRAMTEX texel it's part of, including the x offset
 to the start of the texel (there is no y offset, each byte is on one scanline).
 - It grabs the texel and determines the video mode to use
 - It runs the video mode code on that byte and chooses the correct fragment

 In addition, if the magicBytes are "RGGB":
    One still needs to grab the color value of the pixel in the relevant palette.
    If the top 4 palette color bits are anything except 0b0001, then the pixel is drawn using standard SHR.
    Otherwise, it's drawn using a raw Color Filter Array (CFA) RGGB algorithm.
    See comments in the code below.
    
 */


// Global uniforms assigned in A2VideoManager
uniform int ticks;              // ms since start
uniform int hborder;			// horizontal border in cycles
uniform int vborder;			// vertical border in scanlines
uniform usampler2D VRAMTEX;		// Video RAM texture

// Uniforms assigned in A2WindowBeam
uniform int magicBytes;			// type of SHR format

in vec2 vFragPos;       // The fragment position in pixels
out vec4 fragColor;

bool is640Mode = false;
bool isColorFill = false;	// unused, colorfill is handled by the CPU code
uint paletteColorB1 = 0u;	// first byte of the palette color
uint paletteColorB2 = 0u;	// second byte of the palette color

// This is reversed because we reversed the fragOffset for
// faster calculation of color values
const uint palette640[16] = uint[16](
                                     4u,5u,6u,7u,
                                     0u,1u,2u,3u,
                                     12u,13u,14u,15u,
                                     8u,9u,10u,11u
                                   );

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

vec4 ConvertIIgs2RGB(uint gscolor)
{
    float _red = float((gscolor & 0x0F00u) >> 8);		// 0000 1111 0000 0000
    float _green = float((gscolor & 0x00F0u) >> 4);		// 0000 0000 1111 0000
    float _blue = float(gscolor & 0x000Fu);				// 0000 0000 0000 1111
    float _alpha = 1.0; 								// Fully opaque
    
    // They're 4 bits. Normalize them to 1.0
    _red /= 16.0;
    _green /= 16.0;
    _blue /= 16.0;
    return vec4(_red, _green, _blue, _alpha);
}

// If the monitor is monochrome, get the luminance (greyscale) value of the color
// and apply it to the monochrome value
vec4 GetMonochromeValue(vec4 aColor, vec4 monchromeColor)
{
    float luminance = dot(aColor.rgb, vec3(0.299, 0.587, 0.114));
    return vec4(monchromeColor.rgb * luminance, aColor.a);
}

// Matrices of linear filters for RGGB color calculations
// All colors are scaled by 8, remember to divide by 8 the final result
mat4 matGFilter = mat4(    // G at any location
    -1, 0, 2, 0,
    -1, 2, 4, 2,
    -1, 0, 2, 0,
    -1, 0, 0, 0
);
mat4 matXGFilter = mat4(    // R or B at green locations in their own color rows
    0.5,-1, 0,-1,
    -1,  4, 5, 4,
    -1, -1, 0,-1,
    0.5, 0, 0, 0
);
mat4 matXGXFilter = mat4(    // R or B at green locations in the other color rows
    -1, -1, 4,-1,
    0.5, 0, 5, 0,
    0.5,-1, 4,-1,
    -1,  0, 0, 0
);
mat4 matRBFilter = mat4(    // R at B or B at R
    -1.5, 2, 0, 2,
    -1.5, 0, 6, 0,
    -1.5, 2, 0, 2,
    -1.5, 0, 0, 0
);

// Functions for RGGB logic
// Function to extract a color index from a byte based on the pixel's local index (0 to 3 or 0 to 1)
uint extractColorIdx640(uint byteVal, int localPixel) {
    return (byteVal >> (6 - 2 * localPixel)) & 0x3u;
}
uint extractColorIdx320(uint byteVal, int localPixel) {
    return (byteVal >> (4 - 4 * localPixel)) & 0xFu;
}

// Function to fetch 4 or 2 color indexes from a byte
void fetchByteColorsIdx640(ivec2 byteCoord, out uint colors[4]) {
    uint byteVal = texelFetch(VRAMTEX, byteCoord, 0).r;
    for (int i = 0; i < 4; i++) {
        colors[i] = extractColorIdx640(byteVal, i);
    }
}
void fetchByteColorsIdx320(ivec2 byteCoord, out uint colors[2]) {
    uint byteVal = texelFetch(VRAMTEX, byteCoord, 0).r;
    for (int i = 0; i < 2; i++) {
        colors[i] = extractColorIdx320(byteVal, i);
    }
}

// Function that applies the 5x5 filter to a color component
void applyFilterToColor(inout float colorComponent, mat4 filterMatrix, mat4 colors) {
    for (int i = 0; i < 4; ++i) {
        colorComponent += dot(colors[i], filterMatrix[i]);
    }
}

void main()
{
    uint scanline = uint(vFragPos.y) >> 1;  // Divide by 2
    // first do the borders
    if ((vFragPos.y < float(vborder*2)) || (vFragPos.y >= float(vborder*2+400)) || 
        (vFragPos.x < float(hborder*16)) || (vFragPos.x >= float(640+hborder*16)))
    {
        fragColor = bordercolors[texelFetch(VRAMTEX, ivec2(33u + uint(float(vFragPos.x) / 4.0), scanline), 0).r & 0x0Fu];
        if (monitorColorType > 0)
            fragColor = GetMonochromeValue(fragColor, monitorcolors[monitorColorType]);
        return;
    }
    
    // grab the the scb
    uint scb = texelFetch(VRAMTEX, ivec2(0, scanline), 0).r;
    
    // Parse the useful scb information
    is640Mode = bool(scb & 0x80u);
    // isColorFill = bool(scb & 0x20u);	// unused, already handled when generating VRAM
    if (bool(scb & 0x10u))		// if the CPU told us this line is unused, set the pixel to transparent
    {
        fragColor = vec4(0.0,0.0,0.0,0.0);
        return;
    }

    uint xpos = uint(vFragPos.x);
    uint ypos = uint(vFragPos.y);
    uint fragOffset = 3u - (xpos & 2u);			// reversed so that palette calc is easier
      
    // Also we're running at 640x400 so each byte is 4x2 pixels
    // And each color is 2x2 pixels because we have 2 colors per byte
    ivec2 originByte = ivec2(33u + (xpos >> 2), ypos >> 1);

    // Grab the scanline color byte value
    // The scanline color byte value gives the color for either 4 dots in 640 mode,
    // or 2 doubled dots in 320 mode
    uint byteVal = texelFetch(VRAMTEX,originByte,0).r;

    uint colorIdx = 0u;
    
    if (is640Mode)
    {
        colorIdx = palette640[(fragOffset * 4u) + ((byteVal >> (2u * fragOffset)) & 0x3u)];
    }
    else
    {
        colorIdx = (byteVal >> (4u * (fragOffset >> 1))) & 0xFu;
    }

    // Get the second palette byte, we need it to determine if it's standard SHR or not
    paletteColorB2 = texelFetch(VRAMTEX, ivec2(1u + colorIdx*2u + 1u, originByte.y), 0).r;

    if ((magicBytes == 0xC2C7C7D2)	        // Frame has "RGGB" (reversed bytes in the integer)
        && ((paletteColorB2 >> 4) == 0x1u))  // pixel is a RGGB 
    {
        // We want raw Color Filter Array (CFA) RGGB images, so we need to "demosaic".
        // Each byte is 2 pixels. If it's in an even scanline, the byte has 2 pixels: R and G
        // If in an odd scanline, the byte has 2 other pixels: G and B
        // Also we always have pixel-doubled vertical images for all Apple 2 modes
        // so we need to be careful to duplicate each line

        // https://demo.ipol.im/demo/g_malvar_he_cutler_linear_image_demosaicking
    
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
    

    
        /*
         The pattern is a 2x2 of:
          RG
          GB
         Each byte has either RG or GB, depending on the scanline row (even or odd)
     
         We fetch all the necessary texels in order to get the pixels
         in the following pattern around the origin:
                  X
                 XXX
                XXOXX
                 XXX
                  X
     
         */
        // Let's use matrices to store the colors. We need to store exactly 13 colors.
        // So we can use a 4x4 matrix and keep the last values 0.
        mat4 colors = mat4(0.0);
        /* 
            The color indexes are:
                    0
                1   2   3
            4   5   6   7   8
                9   10  11
                    12
            In matrix terms, using the same layout as the filter matrices:
            0: 0,0      1: 1,0      2: 2,0      3: 3,0
            4: 0,1      5: 1,1      6: 2,1      7: 3,1
            8: 0,2      9: 1,2     10: 2,2     11: 3,2
            12: 0,3     13: 1,3     14: 2,3     15: 3,3

            13-15 are unused and set to 0
                
        */

        if (is640Mode)
        {
            uint originLocalPixel = xpos & 3u;    // Local pixel index [0, 3] within the byte

            // Arrays to store the 4 colors of the fetched byte
            uint byteColorsU[4];  // The 4 colors of the byte on the rows above center
            uint byteColorsD[4];  // The 4 colors of the byte on the rows below center

            // Top and bottom row, just fetch the center byte and get the single pixel color
            fetchByteColorsIdx640(originByte + ivec2(0, -2), byteColorsU);
            fetchByteColorsIdx640(originByte + ivec2(0, +2), byteColorsD);
            colors[0][0] = float(byteColorsU[originLocalPixel]);    // 0
            colors[0][3] = float(byteColorsD[originLocalPixel]);    // 12

            // For rows 2 and 4, we need to fetch 3 consecutive pixels, which could be in different bytes
            fetchByteColorsIdx640(originByte + ivec2(0, -1), byteColorsU);
            fetchByteColorsIdx640(originByte + ivec2(0, +1), byteColorsD);
            colors[2][0] = float(byteColorsU[originLocalPixel]);      // 2
            colors[2][2] = float(byteColorsD[originLocalPixel]);     // 10
            if (originLocalPixel == 0u)  // needs the left bytes
            {
                colors[3][0] = float(byteColorsU[originLocalPixel+1u]);  // 3 right side
                colors[3][2] = float(byteColorsD[originLocalPixel+1u]); // 11
                fetchByteColorsIdx640(originByte + ivec2(-1, -1), byteColorsU);
                fetchByteColorsIdx640(originByte + ivec2(-1, +1), byteColorsD);
                colors[1][0] = float(byteColorsU[3]);  // 1 left side
                colors[1][2] = float(byteColorsD[3]);  // 9
            } else if (originLocalPixel == 3u) // needs the right bytes
            {
                colors[1][0] = float(byteColorsU[originLocalPixel-1u]);  // 1 left side
                colors[1][2] = float(byteColorsD[originLocalPixel-1u]);  // 9
                fetchByteColorsIdx640(originByte + ivec2(+1, -1), byteColorsU);
                fetchByteColorsIdx640(originByte + ivec2(+1, +1), byteColorsD);
                colors[3][0] = float(byteColorsU[0]);  // 3 right side
                colors[3][2] = float(byteColorsD[0]); // 11
            } else {    // no need for another fetch
                colors[1][0] = float(byteColorsU[originLocalPixel-1u]);  // 1 left side
                colors[1][2] = float(byteColorsD[originLocalPixel-1u]);  // 9
                colors[3][0] = float(byteColorsU[originLocalPixel+1u]);  // 3 right side
                colors[3][2] = float(byteColorsD[originLocalPixel+1u]); // 11
            }

            // Finally, the center row. We need to fetch 5 consecutive pixels, which could be in different bytes
            fetchByteColorsIdx640(originByte, byteColorsU);
            colors[2][1] = float(byteColorsU[originLocalPixel]);
            if (originLocalPixel < 2u)  // needs the left byte
            {
                colors[3][1] = float(byteColorsU[originLocalPixel+1u]);  // 7 right side
                colors[0][2] = float(byteColorsU[originLocalPixel+2u]);  // 8
                if (originLocalPixel == 1u)
                {
                    colors[1][1] = float(byteColorsU[0]);  // 5
                    fetchByteColorsIdx640(originByte + ivec2(-1, 0), byteColorsU);
                    colors[0][1] = float(byteColorsU[3]);  // 4 left side
                } else {
                    fetchByteColorsIdx640(originByte + ivec2(-1, 0), byteColorsU);
                    colors[0][1] = float(byteColorsU[2]);  // 4 left side
                    colors[1][1] = float(byteColorsU[3]);  // 5
                }
            } else // needs the right byte
            {
                colors[0][1] = float(byteColorsU[originLocalPixel-2u]);  // 4 left side
                colors[1][1] = float(byteColorsU[originLocalPixel-1u]);  // 5
                if (originLocalPixel == 2u)
                {
                    colors[3][1] = float(byteColorsU[3]);  // 7 right side
                    fetchByteColorsIdx640(originByte + ivec2(+1, 0), byteColorsU);
                    colors[0][2] = float(byteColorsU[0]);  // 8
                } else {
                    fetchByteColorsIdx640(originByte + ivec2(+1, 0), byteColorsU);
                    colors[3][1] = float(byteColorsU[0]);  // 7 right side
                    colors[0][2] = float(byteColorsU[1]);  // 8
                }
            }
            // The `colors` mat4 now contains the color values around the origin pixel

            // Switch to 640x200, from 640x400
            ypos = ypos >> 1;   // divide by 2
            if (((xpos & 1u) == 0u) && ((ypos & 1u) == 0u))
            {
                // top left corner: red location, even row
                fragColor.r = colors[2][1] * 8.0;
                applyFilterToColor(fragColor.g, matGFilter, colors);
                applyFilterToColor(fragColor.b, matRBFilter, colors);
            } else if (((xpos & 1u) == 1u) && ((ypos & 1u) == 0u))
            {
                // top right corner: green location, even row
                applyFilterToColor(fragColor.r, matXGFilter, colors);
                fragColor.g = colors[2][1] * 8.0;
                applyFilterToColor(fragColor.b, matXGXFilter, colors);
            } else if (((xpos & 1u) == 0u) && ((ypos & 1u) == 1u))
            {
                // bottom left corner: green location, odd row
                applyFilterToColor(fragColor.r, matXGXFilter, colors);
                fragColor.g = colors[2][1] * 8.0;
                applyFilterToColor(fragColor.b, matXGFilter, colors);
            } else
            {
                // bottom right right corner: blue location, odd row
                applyFilterToColor(fragColor.r, matRBFilter, colors);
                applyFilterToColor(fragColor.g, matGFilter, colors);
                fragColor.b = colors[2][1] * 8.0;
            }
            fragColor *= (1/(24.0));  // Colors are 0-3, and filter gives x8, so divide by 3x8

        } else {    // 320 mode

            uint originLocalPixel = (xpos >> 1) & 1u;    // Local pixel index [0, 1] within the byte

            // Arrays to store the 2 colors of the fetched byte
            uint byteColorsU[2];  // The 2 colors of the byte on the rows above center
            uint byteColorsD[2];  // The 2 colors of the byte on the rows below center

            // Top and bottom row, just fetch the center byte and get the single pixel color
            fetchByteColorsIdx320(originByte + ivec2(0, -2), byteColorsU);
            fetchByteColorsIdx320(originByte + ivec2(0, +2), byteColorsD);
            colors[0][0] = float(byteColorsU[originLocalPixel]);    // 0
            colors[0][3] = float(byteColorsD[originLocalPixel]);    // 12

            // For rows 2 and 4, we need to fetch 3 consecutive pixels, which could be in different bytes
            fetchByteColorsIdx320(originByte + ivec2(0, -1), byteColorsU);
            fetchByteColorsIdx320(originByte + ivec2(0, +1), byteColorsD);
            colors[2][0] = float(byteColorsU[originLocalPixel]);      // 2
            colors[2][2] = float(byteColorsD[originLocalPixel]);     // 10
            if (originLocalPixel == 0u)  // needs the left bytes
            {
                colors[3][0] = float(byteColorsU[1]);  // 3 right side
                colors[3][2] = float(byteColorsD[1]); // 11
                fetchByteColorsIdx320(originByte + ivec2(-1, -1), byteColorsU);
                fetchByteColorsIdx320(originByte + ivec2(-1, +1), byteColorsD);
                colors[1][0] = float(byteColorsU[1]);  // 1 left side
                colors[1][2] = float(byteColorsD[1]);  // 9
            } else // needs the right bytes
            {
                colors[1][0] = float(byteColorsU[0]);  // 1 left side
                colors[1][2] = float(byteColorsD[0]);  // 9
                fetchByteColorsIdx320(originByte + ivec2(+1, -1), byteColorsU);
                fetchByteColorsIdx320(originByte + ivec2(+1, +1), byteColorsD);
                colors[3][0] = float(byteColorsU[0]);  // 3 right side
                colors[3][2] = float(byteColorsD[0]); // 11
            }

            // Finally, the center row. We need to fetch 5 consecutive pixels, which will be in different bytes
            fetchByteColorsIdx320(originByte, byteColorsU);
            colors[2][1] = float(byteColorsU[originLocalPixel]);    // 6 center pixel
            // colors[2][1] = paletteColorB2 & 0xFu;
            if (originLocalPixel == 0u)  // needs the full left byte and half of the right byte
            {
                colors[3][1] = float(byteColorsU[1]);  // 7 right side
                fetchByteColorsIdx320(originByte + ivec2(+1, 0), byteColorsU);
                colors[0][2] = float(byteColorsU[0]);  // 8
                fetchByteColorsIdx320(originByte + ivec2(-1, 0), byteColorsU);
                colors[0][1] = float(byteColorsU[0]);  // 4 left side
                colors[1][1] = float(byteColorsU[1]);  // 5
            } else // needs the the full right byte and half of the left byte
            {
                colors[1][1] = float(byteColorsU[0]);  // 5 left side
                fetchByteColorsIdx320(originByte + ivec2(-1, 0), byteColorsU);
                colors[0][1] = float(byteColorsU[1]);  // 4
                fetchByteColorsIdx320(originByte + ivec2(+1, 0), byteColorsU);
                colors[3][1] = float(byteColorsU[0]);  // 7 right side
                colors[0][2] = float(byteColorsU[1]);  // 8
            }
    
            // The `colors` mat4 now contains the color values around the origin pixel

            // Switch to 320x200, from 640x400
            xpos = xpos >> 1;
            ypos = ypos >> 1;
            if (((xpos & 1u) == 0u) && ((ypos & 1u) == 0u))
            {
                // top left corner: red location, even row
                fragColor.r = colors[2][1] * 8.0;
                applyFilterToColor(fragColor.g, matGFilter, colors);
                applyFilterToColor(fragColor.b, matRBFilter, colors);
            } else if (((xpos & 1u) == 1u) && ((ypos & 1u) == 0u))
            {
                // top right corner: green location, even row
                applyFilterToColor(fragColor.r, matXGFilter, colors);
                fragColor.g = colors[2][1] * 8.0;
                applyFilterToColor(fragColor.b, matXGXFilter, colors);
            } else if (((xpos & 1u) == 0u) && ((ypos & 1u) == 1u))
            {
                // bottom left corner: green location, odd row
                applyFilterToColor(fragColor.r, matXGXFilter, colors);
                fragColor.g = colors[2][1] * 8.0;
                applyFilterToColor(fragColor.b, matXGFilter, colors);
            } else
            {
                // bottom right right corner: blue location, odd row
                applyFilterToColor(fragColor.r, matRBFilter, colors);
                applyFilterToColor(fragColor.g, matGFilter, colors);
                fragColor.b = colors[2][1] * 8.0;
            }
            fragColor *= (1.0/120.0);  // Colors are 0-15, and filter gives x8, so divide by 15x8
        }   // end 640 or 320 mode

        fragColor.a = 1.0;
        fragColor = clamp(fragColor, 0.0, 1.0);

    }	// end magicBytes "RGGB"
    else {  // (magicBytes == 0x00000000)	// Standard SHR
        // get the missing first palette byte and fetch the color
        paletteColorB1 = texelFetch(VRAMTEX, ivec2(1u + colorIdx*2u, originByte.y), 0).r;
        fragColor = ConvertIIgs2RGB((paletteColorB2 << 8) + paletteColorB1);
    }
    
    if (monitorColorType > 0)
        fragColor = GetMonochromeValue(fragColor, monitorcolors[monitorColorType]);
}
