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

	void Render(SDL_Window* window, GLuint inputTextureSlot, GLuint scanlineCount);
	void DisplayImGuiWindow(bool* p_open);

	nlohmann::json SerializeState();
	void DeserializeState(const nlohmann::json &jsonState);

	// Tells main.cpp to skip flipping the buffers if we are halving the frame rate
	// This actually skips even frames if ShouldFrameBeSkipped() is called after the frame
	// is created
	const bool ShouldFrameBeSkipped() { return (bHalveFramerate && (frame_count & 1) == 1); };
	const bool IsFrameRateHalved() { return bHalveFramerate; };

	// public properties
	std::vector<Shader>v_ppshaders;
private:
	void Initialize();
	void SaveState(std::string filePath);
	void LoadState(std::string filePath);
	int PopulateBezelFiles(std::vector<std::string>& bezelFiles, const std::string& selectedBezelFile);
	void SelectShader();
	void RegeneratePreviousTexture();
	void ResetToDefaults();

	void LoadSelectedBezel();

	// Singleton pattern
	static PostProcessor* s_instance;
	PostProcessor()
	{
		Initialize();
	}

	// Attributes
	GLuint quadVAO = UINT_MAX;
	GLuint quadVBO = UINT_MAX;
	GLuint FBO_prevFrame = UINT_MAX;		// Framebuffer that holds the texture of the previous frame
	GLuint prevFrame_texture_id = UINT_MAX;	// The previous frame as a texture

	GLint maxTexSize = 0;	// maximum texture size, depends on GL implementation

	bool bImguiWindowIsOpen = false;

	Shader shaderProgram;		// PP shader program
	Shader shaderProgramBezel;	// Bezel shader program

	OpenGLHelper::ImageAsset bezelImageAsset;

	GLint viewportWidth = 0, viewportHeight = 0;
	GLint quadWidth = 0, quadHeight = 0;
	GLint texWidth = 0, texHeight = 0;
	GLint prev_texWidth = INT_MAX, prev_texHeight = INT_MAX;

	GLint texUnitCurrent = INT_MAX, texUnitPrevious = INT_MAX;

	// The transform matrix for the A2 texture quad
	glm::mat4 mTransform = glm::mat4(1.0f);

	// Transformed A2 render quad
	// the rectangle (in pixels) where the A2 texture is rendered on screen
	// based on the transformations requested by the user. This is only necessary
	// to copy the data into the previous frame texture, as the transformation is
	// otherwise happening in the vertex shader
	SDL_Rect tA2Quad;

	bool bCRTFillWindow = false;

	int frame_count = 0;	// Frame count for interlacing, it may not be aligned with A2Video frames
	char preset_name_buffer[28];	// Preset's name
	int max_integer_scale = 1;	// Maximum possible integer scale given screen size
	int integer_scale = 1;		// Base integer scale used
	bool bAutoScale = true;		// Automatically scale to max scale?
	bool bHalveFramerate = false;	// Mixes every pair of frames, to avoid page flip flicker
#define _PP_NO_BEZEL_FILENAME "NONE"
	std::string selectedBezelFile = _PP_NO_BEZEL_FILENAME;
	int currentBezelIndex = 0;
	glm::vec2 bezelSize = glm::vec2(1.0f, 1.0f);

	// Shader parameter variables
	bool p_b_smoothCorner = false;
	bool p_b_extGamma = false;
	bool p_b_slot = false;
	float p_f_barrelDistortion = 0.0f;
	float p_f_bgr = 0.0f;
	float p_f_black = 0.0f;
	float p_f_brDep = 0.2f;
	float p_f_brightness = 1.0f;
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
	float p_f_scanlineWeight = 1.0f;
	float p_f_scanSpeed = 1.0f;
	float p_f_filmGrain = 0.0f;
	float p_f_interlace = 0.f;
	float p_f_slotW = 3.0f;
	float p_f_vignetteWeight = 0.0f;
	int p_i_cSpace = 0;
	int p_i_maskType = 0;
	int p_i_postprocessingLevel = 0;
	int p_i_scanlineType = 2;
	int p_f_ghostingPercent = 0;	// Percentage of ghosting of previous frame. 0 means no ghosting
	float p_f_phosphorBlur = 0.0f;	// blur modifier
	glm::vec2 p_v_warp = glm::vec2(0.0f, 0.0f);	// curvature
	glm::vec2 p_v_center = glm::vec2(0.0f, 0.0f);
	glm::vec2 p_v_zoom = glm::vec2(1.0f, 1.0f);

	// bezel shader variables
	bool p_b_outlineQuad = false;
	float p_f_bezelReflection = 0.0f;
	float p_f_reflectionBlur = 0.0f;
	glm::vec2 p_v_reflectionScale = glm::vec2(1.0f, 1.0f);
	glm::vec2 p_v_reflectionTranslation = glm::vec2(0.0f, 0.0f);

	// imgui vars
	bool bImGuiLockWarp = false;
	bool bImGuiLockZoom = false;
};

#endif	// POSTPROCESSOR_H
