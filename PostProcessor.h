#pragma once
#ifndef POSTPROCESSOR_H
#define POSTPROCESSOR_H

/*
	Singleton postprocessing class that has a number of shaders to choose from.
	It simply takes as input a texture and passes it through the selected shader.
	It also has an ImGUI interface to modify the shader variables.
*/

#include "common.h"
#include "shader.h"
#include <vector>

// Link the output of the legacy, sdhr, ... renderers to the input of the postprocessor
#define _PP_INPUT_TEXTURE_UNIT _TEXUNIT_POSTPROCESS

class PostProcessor
{
public:
	// public singleton code
	static PostProcessor* GetInstance()
	{
		if (NULL == s_instance)
			s_instance = new PostProcessor();
		return s_instance;
	}
	~PostProcessor();

	void Render(SDL_Window* window, GLuint inputTextureId);
	void DisplayImGuiWindow(bool* p_open);
	nlohmann::json SerializeSate();
	void DeserializeSate(const nlohmann::json &jsonState);

	// public properties
	std::vector<Shader>v_ppshaders;
private:
	void Initialize();
	void SaveState(int profile_id);
	void LoadState(int profile_id);
	void SelectShader();

	// Singleton pattern
	static PostProcessor* s_instance;
	PostProcessor()
	{
		Initialize();
	}

	// Attributes
	GLuint quadVAO = UINT_MAX;
	GLuint quadVBO = UINT_MAX;
	
	bool bImguiWindowIsOpen = false;

	Shader shaderProgram;

	// The quad vertices will change based on the change in the requested screen size
	glm::vec4 quadViewportCoords = glm::vec4(0, 0, 0, 0);	// left, top, right, bottom
	bool bCRTFillWindow = false;

	GLint viewportWidth = 0, viewportHeight = 0;
	GLint quadWidth = 0, quadHeight = 0;
	GLint texWidth = 0, texHeight = 0;
	GLint prev_texWidth = INT_MAX, prev_texHeight = INT_MAX;

	int frame_count = 0;	// Frame count for interlacing
	int v_presets = 0;	// Preset chosen

	// Shader parameter variables
	int p_postprocessing_level = 1;
	bool p_bzl = false;
	bool p_corner = false;
	bool p_ext_gamma = false;
	bool p_interlace = false;
	bool p_potato = false;
	bool p_slot = false;
	bool p_vig = false;
	float p_bgr = 0.0f;
	float p_black = 0.0f;
	float p_br_dep = 0.2f;
	float p_brightness = 1.0f;
	int p_c_space = 0;
	float p_c_str = 0.0f;
	float p_centerx = 0.0f;
	float p_centery = 0.0f;
	float p_conv_b = 0.0f;
	float p_conv_g = 0.0f;
	float p_conv_r = 0.0f;
	float p_gb = 0.0f;
	int p_m_type = 0;
	float p_maskh = 0.75f;
	float p_maskl = 0.3f;
	float p_msize = 1.0f;
	float p_rb = 0.0f;
	float p_rg = 0.0f;
	float p_saturation = 1.0f;
	float p_scanline_weight = 0.3f;
	int p_scanline_type = 2;
	float p_slotw = 3.0f;
	float p_warpx = 0.0f;
	float p_warpy = 0.0f;
	float p_barrel_distortion = 0.0f;
	float p_zoomx = 0.0f;
	float p_zoomy = 0.0f;

};

#endif	// POSTPROCESSOR_H
