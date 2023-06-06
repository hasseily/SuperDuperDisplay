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

// DEFINITIONS OF SDHR SPECS
#define _SDHR_DEFAULT_WIDTH  640
#define _SDHR_DEFAULT_HEIGHT 360
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
#else
#define _SHADER_SDHR_VERTEX_DEFAULT "shaders/sdhr_default_330.vert"
#define _SHADER_SDHR_FRAGMENT_DEFAULT "shaders/sdhr_default_330.frag"
#endif

#endif	// COMMON_H