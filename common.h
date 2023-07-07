#pragma once
#ifndef COMMON_H
#define COMMON_H

#ifdef _DEBUG   // Visual Studio
#define DEBUG
#endif

#include <stdio.h>

#include "glad/glad.h"
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

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#include "stb_image.h"

typedef struct uixy { uint32_t x; uint32_t y; } uXY;
typedef struct ixy { int32_t x; int32_t y; } iXY;

#define _A2VIDEO_MIN_WIDTH 40*7*2
#define _A2VIDEO_MIN_HEIGHT 24*8*2
#define _A2VIDEO_MIN_MIXED_HEIGHT 20*8*2

#define _A2VIDEO_TEXT1_START 0x400
#define _A2VIDEO_TEXT2_START 0x800
#define _A2VIDEO_TEXT_SIZE 0x400
#define _A2VIDEO_HGR1_START 0x2000
#define _A2VIDEO_HGR2_START 0x4000
#define _A2VIDEO_HGR_SIZE 0x2000

#define _SCREEN_DEFAULT_WIDTH  _A2VIDEO_MIN_WIDTH
#define _SCREEN_DEFAULT_HEIGHT _A2VIDEO_MIN_HEIGHT

// DEFINITIONS OF SDHR SPECS
#define _SDHR_SERVER_PORT 8080
#define _SDHR_MEMORY_SHADOW_BEGIN 0x200		// Starting point of memory shadowing
#define _SDHR_MEMORY_SHADOW_END 0xC000		// Ending point (exclusive) of memory shadowing
#define _SDHR_UPLOAD_REGION_SIZE 256*256*256	// Upload data region size (should be 16MB)
#define _SDHR_MAX_WINDOWS 256
#define _SDHR_TBO_TEXUNIT 1					// Texture unit (GL_TEXTURE0 + unit) of the tilebufferobject
#define _SDHR_MAX_TEXTURES 14				// Max # of image assets available
#define _SDHR_START_TEXTURES GL_TEXTURE2	// Start of the image assets
#define _SDHR_MAX_UV_SCALE 100.f			// Maximum scale of Mosaic Tile UV	

// SHADERS
#if defined(IMGUI_IMPL_OPENGL_ES2)
#define _SHADER_SDHR_VERTEX_DEFAULT "shaders/sdhr_default_310es.vert"
#define _SHADER_SDHR_FRAGMENT_DEFAULT "shaders/sdhr_default_310es.frag"
#define _SHADER_TEXT_VERTEX_DEFAULT "shaders/a2video_text_310es.vert"
#define _SHADER_TEXT_FRAGMENT_DEFAULT "shaders/a2video_text_310es.frag"
#else
#define _SHADER_SDHR_VERTEX_DEFAULT "shaders/sdhr_default_330.vert"
#define _SHADER_SDHR_FRAGMENT_DEFAULT "shaders/sdhr_default_330.frag"
#define _SHADER_TEXT_VERTEX_DEFAULT "shaders/a2video_text_330.vert"
#define _SHADER_TEXT_FRAGMENT_DEFAULT "shaders/a2video_text_330.frag"
#endif

#endif	// COMMON_H