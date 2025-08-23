#pragma once
#ifndef COMMON_H
#define COMMON_H

#define SDD_VERSION "0.7.5"

#ifdef _DEBUG   // Visual Studio
#define DEBUG
#endif

#include <stdio.h>

#include "glad/glad.h"
#include <SDL.h>
#include "nlohmann/json.hpp"

#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <SDL_opengles2.h>
#define GL2_PROTOTYPES 1
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2platform.h>
#else
#include <SDL_opengl.h>
#endif

#include "glm/glm.hpp"

#include "ConcurrentQueue.h"
#include "ByteBuffer.h"

// stb_image include. Suppress "unused function" warning.
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#elif defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4505)
#endif
#include "stb_image.h"
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(pop)
#endif

typedef struct uixy { uint32_t x; uint32_t y; } uXY;
typedef struct ixy { int32_t x; int32_t y; } iXY;

enum SwapInterval_e
{
	SWAPINTERVAL_ADAPTIVE = -1,	// Adaptive vsync
	SWAPINTERVAL_NONE,
	SWAPINTERVAL_VSYNC,			// regular vsync
	SWAPINTERVAL_APPLE2BUS,		// VSYNC to Apple 2 bus
	SWAPINTERVAL_TOTAL_COUNT
};

struct A2RenderVertex {
	glm::vec2 RelPos;		// Relative position of the vertex
	glm::vec2 PixelPos;		// Pixel position of the vertex in the Apple 2 screen
};

// Apple 2 frequency
#define _A2_CPU_FREQUENCY_NTSC 1'020'484
#define _A2_CPU_FREQUENCY_PAL 1'015'625

// START AND END POINTS OF MEMORY SHADOWING
// Both banks of the Apple 2 are shadowed on the host machine
// in a single memory space, of size _A2_MEMORY_SHADOW_END*2
// To access the AUX bank, add _A2_MEMORY_SHADOW_END to the
// pointer of the start of the memory
// Anything between 0 and _A2_MEMORY_SHADOW_BEGIN in each bank is unused
#define _A2_MEMORY_SHADOW_BEGIN 0x0000
#define _A2_MEMORY_SHADOW_END 0xC000

// For all modes!
// The data buffer is always in tex1.
// The special SHR4 PAL256 vram is in tex2.
// Image assets can be put in tex4 to tex12
// Post processing input texture is always in tex16
#define _TEXUNIT_DATABUFFER_R8UI GL_TEXTURE1	// Texunit of the data buffer (R8UI VRAM)
#define _TEXUNIT_DATABUFFER_RGBA8UI GL_TEXTURE2	// Texunit of the data buffer (RGBA8UI VRAM)
#define _TEXUNIT_PAL256BUFFER GL_TEXTURE3		// Texunit of the SHR4 PAL256 vram
#define _TEXUNIT_IMAGE_ASSETS_START GL_TEXTURE4	// Start of the image assets
#define _TEXUNIT_APPLE2MEMORY_R8UI GL_TEXTURE12 // Memory texture for use in shaders
#define _TEXUNIT_MERGE_OFFSET GL_TEXTURE13		// Merge Offset buffer (for sine wobble)
#define _TEXUNIT_PRE_NTSC GL_TEXTURE14			// If NTSC legacy output requested, this is the non-NTSC tex
#define _TEXUNIT_PP_PREVIOUS GL_TEXTURE15		// The previous frame as a texture
#define _TEXUNIT_POSTPROCESS GL_TEXTURE16		// input texunit the PP will use to generate the final output
#define _TEXUNIT_PP_BEZEL GL_TEXTURE17			// The bezel in postprocessing
#define _TEXUNIT_PP_BEZEL_GLASS GL_TEXTURE18	// The bezel glass in postprocessing
// exact asset textures
#define _TEXUNIT_IMAGE_FONT_ROM_DEFAULT GL_TEXTURE4
#define _TEXUNIT_IMAGE_FONT_ROM_ALTERNATE GL_TEXTURE5
#define _TEXUNIT_IMAGE_COMPOSITE_LGR GL_TEXTURE6
#define _TEXUNIT_IMAGE_COMPOSITE_HGR GL_TEXTURE7
#define _TEXUNIT_IMAGE_COMPOSITE_DHGR GL_TEXTURE8
#define _TEXUNIT_IMAGE_FONT_VIDHD_8X8 GL_TEXTURE9

// AUDIO
#define _AUDIO_SAMPLE_RATE 44100

// DEFINITIONS OF SDHR SPECS
#define _SDHR_UPLOAD_REGION_SIZE 256*256*256	// Upload data region size (should be 16MB)
#define _SDHR_MAX_WINDOWS 256
#define _SDHR_MAX_TEXTURES (_TEXUNIT_APPLE2MEMORY_R8UI - _TEXUNIT_IMAGE_ASSETS_START)	// Max # of image assets available
#define _SDHR_MAX_UV_SCALE 100.f				// Maximum scale of Mosaic Tile UV

// ORIGINAL APPLE 2 VIDEO MODES
#define _A2VIDEO_LEGACY_BYTES_PER_LINE 40
#define _A2VIDEO_LEGACY_SCANLINES 24
#define _A2VIDEO_LEGACY_WIDTH _A2VIDEO_LEGACY_BYTES_PER_LINE*7*2
#define _A2VIDEO_LEGACY_HEIGHT _A2VIDEO_LEGACY_SCANLINES*8*2
#define _A2VIDEO_MIN_MIXED_HEIGHT (_A2VIDEO_LEGACY_SCANLINES-4)*8*2
#define _A2VIDEO_SHR_BYTES_PER_LINE 160
#define _A2VIDEO_SHR_SCANLINES 200
#define _A2VIDEO_SHR_WIDTH _A2VIDEO_SHR_BYTES_PER_LINE*4
#define _A2VIDEO_SHR_HEIGHT _A2VIDEO_SHR_SCANLINES7*2

#define _A2VIDEO_LEGACY_ASPECT_RATIO 280.f/192.f
#define _A2VIDEO_SHR_ASPECT_RATIO 320.f/200.f

#define _A2VIDEO_TEXT1_START 0x400
#define _A2VIDEO_TEXT2_START 0x800
#define _A2VIDEO_TEXT_SIZE 0x400
#define _A2VIDEO_HGR1_START 0x2000
#define _A2VIDEO_HGR2_START 0x4000
#define _A2VIDEO_HGR_SIZE 0x2000
#define _A2VIDEO_SHR_START 0x2000	// All SHR is in the AUX (E1) bank!
#define _A2VIDEO_SHR_SIZE 0x8000
#define _A2VIDEO_SHR_SCB_START 0x9D00			// scanline control bytes: 1 per line, 200 total
#define _A2VIDEO_SHR_CTRL_BYTES 0x9DF8			// start of the 4 control bytes. MSB is DoubleMode_e, then bank, lo and hi of SHR_3200 palettes
#define _A2VIDEO_SHR_MAGIC_BYTES 0x9DFC			// start of the 4 magic bytes determining the SHR mode
#define _A2VIDEO_SHR_PALETTE_START 0x9E00	// 16 SHR palettes of 16 colors, 2 bytes per color. Total 512 bytes

/* ---- endianness detection for extra safety ---- */
// we're casting the magic string to a uint32_t so it flips when in little endian
// In memory, the bytes are as if they're a string, so in big endian format
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__) \
 || defined(__BIG_ENDIAN__) || defined(_BIG_ENDIAN) \
 || defined(__ARMEB__) || defined(__AARCH64EB__) || defined(__MIPSEB__)
#define _A2_BIG_ENDIAN 1
#define _A2VIDEO_SHR4_MAGIC_STRING 0xD3C8D2B4	// 'SHR4' activates SHR4 mode
#define _A2VIDEO_3200_MAGIC_STRING 0xB3B2B0B0	// '3200'activates SHR_3200 mode
#else
#define _A2_BIG_ENDIAN 0
#define _A2VIDEO_SHR4_MAGIC_STRING 0xB4D2C8D3	// 'SHR4' (in reverse order in memory) activates SHR4 mode
#define _A2VIDEO_3200_MAGIC_STRING 0xB0B0B2B3	// '3200' (in reverse order in memory) activates SHR_3200 mode
#endif



// SHADERS
#define _SHADER_VERTEX_BASIC "shaders/basic.vert"
#define _SHADER_VERTEX_BASIC_TRANSFORM "shaders/basic_with_transform.vert"
#define _SHADER_FRAGMENT_BASIC "shaders/basic.frag"

#define _SHADER_A2_VERTEX_DEFAULT "shaders/a2video.vert"
#define _SHADER_RGB_TEXT_FRAGMENT "shaders/a2video_text.frag"
#define _SHADER_RGB_LGR_FRAGMENT "shaders/a2video_lgr.frag"
#define _SHADER_RGB_HGR_FRAGMENT "shaders/a2video_hgr.frag"
#define _SHADER_RGB_DHGR_FRAGMENT "shaders/a2video_dhgr.frag"
#define _SHADER_RGB_SHR_FRAGMENT "shaders/a2video_shr.frag"
#define _SHADER_BEAM_LEGACY_FRAGMENT "shaders/a2video_beam_legacy.frag"
#define _SHADER_BEAM_SHR_FRAGMENT "shaders/a2video_beam_shr_raw.frag"
//#define _SHADER_BEAM_MERGE_FRAGMENT "shaders/a2video_beam_merge.frag"
#define _SHADER_VIDHD_TEXT_FRAGMENT "shaders/vidhd_beam_text.frag"

#define _SHADER_SDHR_VERTEX_DEFAULT "shaders/sdhr_default_330.vert"
#define _SHADER_SDHR_FRAGMENT_DEFAULT "shaders/sdhr_default_330.frag"
#define _SHADER_SDHR_VERTEX_DEPIXELIZE "shaders/sdhr_depixelize_330.vert"
#define _SHADER_SDHR_FRAGMENT_DEPIXELIZE "shaders/sdhr_depixelize_330.frag"

#endif	// COMMON_H
