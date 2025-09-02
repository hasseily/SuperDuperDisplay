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
 SHR shader
 For each pixel, get its byte value and row. Then get its control byte.
 Then using the control byte info, get the palette. And with the byte value, get the color from the palette.

 The Apple 2 memory passed in should start at 0x2000 in AUX.
 The control bytes start at offset 0x7D00 and the palettes at offset 0x7E00.
 They're both on line 0x1F of the memory texture that's split in 1024 byte lines.
 Hence control bytes start at 0x100 of line 0x1F, and palettes at 0x200 of line 0x1F
 */

uniform uvec2 tileSize;
uniform usampler2D APPLE2MEMORYTEX; // Apple 2e's memory, starting at 0x2000 in AUX for SHR
								 // Unsigned int sampler!
uniform int memstart;			 // where to start in memory

in vec2 vFragPos;       // The fragment position in pixels
out vec4 fragColor;

bool is640Mode = false;
bool isColorFill = false;
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

void main()
{
	// Grab Scanline Control Byte information
	int scbByteOffset = memstart + 0x7D00 + ((int(vFragPos.y))/int(tileSize.y));
	uint scb = texelFetch(APPLE2MEMORYTEX,ivec2(scbByteOffset % 1024, scbByteOffset / 1024), 0).r;
	is640Mode = bool(scb & 0x80u);
	isColorFill = bool(scb & 0x20u);

	// then figure out which byte this fragment is part of
	// Calculate the position of the fragment in byte intervals
	uvec2 fragOffset;
	// Fragment offset to byte origin, in pixels. It is 0-3.
	// If in 320 mode, there are only 2 pixels, duplicated
	// If in 640 mode, there are 4 unique pixels
    // Reverse the fragment offset, it's cheaper when calculating the color
	fragOffset.x = 3u - uint(int(vFragPos.x) % int(tileSize.x));
	fragOffset.y = uint(int(vFragPos.y) % int(tileSize.y));
	// Row and column number of the byte containing this fragment
	ivec2 byteColRow = ivec2(vFragPos) / ivec2(tileSize);
	
	// Each line is 160 (0xA0) bytes
	int byteOffset = memstart + byteColRow.y * 0xA0 + byteColRow.x;
	uint byteVal = texelFetch(APPLE2MEMORYTEX, ivec2(byteOffset % 1024, byteOffset / 1024), 0).r;
	uint colorIdx = 0u;
	
	
	if (is640Mode)
	{
		colorIdx = palette640[(fragOffset.x * 4u) + ((byteVal >> (2u * fragOffset.x)) & 0x3u)];
	}
	else
	{
		colorIdx = (byteVal >> (4u * (fragOffset.x/2u))) & 0xFu;
		if (isColorFill && (colorIdx == 0u))
		{
            if (fragOffset.x < 2u)
            {
                // The other pixel may have a color
                colorIdx = (byteVal >> 4u);
            }
            if (colorIdx == 0u)
            {
                // Loop through every byte in this row
                for (int i = byteColRow.x; i > 0; i--)
                {
                    byteOffset--;
				    byteVal = texelFetch(APPLE2MEMORYTEX, ivec2(byteOffset % 1024, byteOffset / 1024), 0).r;
                    if (byteVal == 0u)
                        continue;
                    colorIdx = byteVal & 0xFu;
                    if (colorIdx == 0u)
                        colorIdx = (byteVal >> 4u);
                    break;
                }
            }
            /*
			// Needs to be colorfilled with the closest previous pixel color that isn't 0
			// Start searching backward from the current position
			for (int i = (int(vFragPos.x) - 1); i >= 0; --i)
			{
				fragOffset.x = 3u - uint(i % 4);
				byteColRow.x = i / 4;
				byteOffset = byteColRow.y * 0xA0 + byteColRow.x;
				byteVal = texelFetch(APPLE2MEMORYTEX, ivec2(byteOffset % 1024, byteOffset / 1024), 0).r;
				
				uint prevColorIdx = (byteVal >> (4u * (fragOffset.x/2u))) & 0xFu;
				if (prevColorIdx != 0u)
				{
					// Found a previous pixel color that isn't 0, that's the one to use
					colorIdx = prevColorIdx;
					break;
				}
			}
            */
		}
	}
	// Get the palette color from the relevant palette.
	// There are 16 colors per palette, each color is 2 bytes.
	// So each palette is 32 bytes, and we jump 32 bytes at a time (<< 5) to get the requested palette
	// Palettes are on line 0x1F, starting at 0x200
	int paletteOffset = memstart + 0x7E00 + int(((scb & 0xFu) << 5));
	paletteColorB1 = texelFetch(APPLE2MEMORYTEX,ivec2((paletteOffset % 1024) + int(colorIdx*2u), paletteOffset / 1024), 0).r;
	paletteColorB2 = texelFetch(APPLE2MEMORYTEX,ivec2((paletteOffset % 1024) + int(colorIdx*2u) + 1, paletteOffset / 1024), 0).r;
	fragColor = ConvertIIgs2RGB((paletteColorB2 << 8) + paletteColorB1);
}
