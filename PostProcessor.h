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
	nlohmann::json SerializeState();
	void DeserializeState(const nlohmann::json &jsonState);

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
	int idx_preset = 0;		// Preset chosen
	char preset_name_buffer[28];	// Preset's name
	int max_integer_scale = 1;	// Maximum possible integer scale given screen size
	int integer_scale = 1;		// Base integer scale used
	bool bAutoScale = true;		// Automatically scale to max scale?

	// Shader parameter variables
	bool p_b_smoothCorner = false;
	bool p_b_extGamma = false;
	bool p_b_interlace = false;
	bool p_b_potato = false;
	bool p_b_slot = false;
	bool p_b_vignette = false;
	float p_f_barrelDistortion = 0.0f;
	float p_f_bgr = 0.0f;
	float p_f_black = 0.0f;
	float p_f_brDep = 0.2f;
	float p_f_brightness = 2.0f;
	float p_f_centerX = 0.0f;
	float p_f_centerY = 0.0f;
	float p_f_convB = 0.0f;
	float p_f_convG = 0.0f;
	float p_f_convR = 0.0f;
	float p_f_corner = 0.0f;
	float p_f_cStr = 0.0f;
	float p_f_hueGB = 0.0f;
	float p_f_hueRB = 0.0f;
	float p_f_hueRG = 0.0f;
	float p_f_maskHigh = 0.75f;
	float p_f_maskLow = 0.3f;
	float p_f_maskSize = 1.0f;
	float p_f_saturation = 1.0f;
	float p_f_scanlineWeight = 0.3f;
	float p_f_slotW = 3.0f;
	float p_f_vignetteWeight = 0.8f;
	float p_f_warpX = 0.0f;
	float p_f_warpY = 0.0f;
	float p_f_zoomX = 0.0f;
	float p_f_zoomY = 0.0f;
	int p_i_cSpace = 0;
	int p_i_maskType = 0;
	int p_i_postprocessingLevel = 0;
	int p_i_scanlineType = 2;

};

#endif	// POSTPROCESSOR_H
