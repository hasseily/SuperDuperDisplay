#pragma once
#ifndef COMMON_H
#define COMMON_H

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
// Image assets can be put in tex3 to tex7
// Post processing input texture is always in tex15
#define _TEXUNIT_DATABUFFER GL_TEXTURE1			// Texunit of the data buffer (vram for legacy, TBO for SDHR)
#define _TEXUNIT_PAL256BUFFER GL_TEXTURE2		// Texunit of the SHR4 PAL256 vram
#define _TEXUNIT_IMAGE_ASSETS_START GL_TEXTURE3	// Start of the image assets
#define _TEXUNIT_POSTPROCESS GL_TEXTURE15		// input texunit the PP will use to generate the final output

// AUDIO
#define _AUDIO_SAMPLE_RATE 44100

// DEFINITIONS OF SDHR SPECS
#define _SDHR_UPLOAD_REGION_SIZE 256*256*256	// Upload data region size (should be 16MB)
#define _SDHR_MAX_WINDOWS 256
#define _SDHR_MAX_TEXTURES (_TEXUNIT_POSTPROCESS - _TEXUNIT_IMAGE_ASSETS_START)	// Max # of image assets available
#define _SDHR_MAX_UV_SCALE 100.f				// Maximum scale of Mosaic Tile UV

// ORIGINAL APPLE 2 VIDEO MODES
#define _A2VIDEO_LEGACY_WIDTH 40*7*2
#define _A2VIDEO_LEGACY_HEIGHT 24*8*2
#define _A2VIDEO_MIN_MIXED_HEIGHT 20*8*2
#define _A2VIDEO_SHR_WIDTH 640
#define _A2VIDEO_SHR_HEIGHT 200*2
#define _A2VIDEO_SHR_BYTES_PER_LINE 160
#define _A2VIDEO_SHR_SCANLINES 200

#define _A2VIDEO_TEXT1_START 0x400
#define _A2VIDEO_TEXT2_START 0x800
#define _A2VIDEO_TEXT_SIZE 0x400
#define _A2VIDEO_HGR1_START 0x2000
#define _A2VIDEO_HGR2_START 0x4000
#define _A2VIDEO_HGR_SIZE 0x2000
#define _A2VIDEO_SHR_START 0x2000	// All SHR is in the AUX (E1) bank!
#define _A2VIDEO_SHR_SIZE 0x8000
#define _A2VIDEO_SHR_SCB_START 0x9D00			// scanline control bytes: 1 per line, 200 total
#define _A2VIDEO_SHR_MAGIC_BYTES 0x9DFC			// start of the 4 magic bytes determining the SHR mode
#define _A2VIDEO_SHR_MAGIC_STRING 0xB4D2C8D3	// 'SHR4' (in reverse order in memory) activates SHR4 mode
#define _A2VIDEO_SHR_PALETTE_START 0x9E00	// 16 SHR palettes of 16 colors, 2 bytes per color. Total 512 bytes


// SHADERS
#define _SHADER_VERTEX_BASIC "shaders/basic.vert"
#define _SHADER_FRAGMENT_BASIC "shaders/basic.frag"

#define _SHADER_A2_VERTEX_DEFAULT "shaders/a2video.vert"
//#define _SHADER_TEXT_FRAGMENT "shaders/a2video_text.frag"
//#define _SHADER_LGR_FRAGMENT "shaders/a2video_lgr.frag"
//#define _SHADER_HGR_FRAGMENT "shaders/a2video_hgr.frag"
//#define _SHADER_DHGR_FRAGMENT "shaders/a2video_dhgr.frag"
//#define _SHADER_SHR_FRAGMENT "shaders/a2video_shr.frag"
#define _SHADER_BEAM_LEGACY_FRAGMENT "shaders/a2video_beam_legacy.frag"
#define _SHADER_BEAM_SHR_FRAGMENT "shaders/a2video_beam_shr_raw.frag"
#define _SHADER_BEAM_MERGE_FRAGMENT "shaders/a2video_beam_merge.frag"

#define _SHADER_SDHR_VERTEX_DEFAULT "shaders/sdhr_default_330.vert"
#define _SHADER_SDHR_FRAGMENT_DEFAULT "shaders/sdhr_default_330.frag"
#define _SHADER_SDHR_VERTEX_DEPIXELIZE "shaders/sdhr_depixelize_330.vert"
#define _SHADER_SDHR_FRAGMENT_DEPIXELIZE "shaders/sdhr_depixelize_330.frag"

#endif	// COMMON_H
