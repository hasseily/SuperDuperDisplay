#include "PostProcessor.h"
#include "imgui.h"
#include "imgui_internal.h"		// for PushItemFlag
#include "extras/ImGuiFileDialog.h"
// For save/restore of presets
#include <fstream>
#include <sstream>

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

// below because "The declaration of a static data member in its class definition is not a definition"
PostProcessor* PostProcessor::s_instance;

// The PostProcessor will take any texture that's in slot _PP_INPUT_TEXTURE_UNIT and apply the
// postprocessing shader on it.
// It always dynamically calculates the texture's size and properly scales it up in integer steps
// (or down in fractional steps).

// The PostProcessor shader has 3 modes of operation:
// - A passthrough mode
// - A simple scanline mode that makes every other scanline black
// - A full shader mode with a kitchensink of features

// To make it more optimal for low end devices that may not approve of the full shader,
// the passthrough mode uses a basic passthrough shader instead of the full one,
// although the full shader can handle passthrough as well.

//////////////////////////////////////////////////////////////////////////
// Basic singleton methods
//////////////////////////////////////////////////////////////////////////

void PostProcessor::Initialize()
{
	if (quadVAO == UINT_MAX)
	{
		// We're going to generate a static quad that the vertex shader will transform
		// based on the requested size changes. This way is more efficient than either
		// always creating a new static quad for every request, or using GL_DYNAMIC_DRAW
		GLfloat quadVertices[] = {
			// Positions         // Texture Coords
			-1.0f,  1.0f,        0.0f, 0.0f,   // Top-left
			 1.0f, -1.0f,        1.0f, 1.0f,   // Bottom-right
			 1.0f,  1.0f,        1.0f, 0.0f,   // Top-right

			-1.0f,  1.0f,        0.0f, 0.0f,   // Top-left
			-1.0f, -1.0f,        0.0f, 1.0f,   // Bottom-left
			 1.0f, -1.0f,        1.0f, 1.0f    // Bottom-right
		};

		glGenVertexArrays(1, &quadVAO);
		glGenBuffers(1, &quadVBO);

		glBindVertexArray(quadVAO);
		glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

		// Set up vertex attribute pointers
		// Attribute 0: position (2 floats)
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void*)0);
		glEnableVertexAttribArray(0);
		// Attribute 1: texture coordinates (2 floats)
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void*)(2 * sizeof(GLfloat)));
		glEnableVertexAttribArray(1);

		// Unbind for cleanliness
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
		GLenum glerr;
		if ((glerr = glGetError()) != GL_NO_ERROR) {
			std::cerr << "OpenGL error PP Initialize: " << glerr << std::endl;
		}
	}

	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTexSize);

	// PP shader list
	v_ppshaders.clear();
	Shader shader_basic = Shader();
	shader_basic.build(_SHADER_VERTEX_BASIC_TRANSFORM, _SHADER_FRAGMENT_BASIC);
	v_ppshaders.push_back(shader_basic);
	Shader shader_pp = Shader();
	shader_pp.build("shaders/a2video_postprocess.glsl", "shaders/a2video_postprocess.glsl");
	v_ppshaders.push_back(shader_pp);
	memset(preset_name_buffer, 0, sizeof(preset_name_buffer));

	//Bezel shader
	shaderProgramBezel = Shader();
	shaderProgramBezel.build("shaders/overlay_bezel.glsl", "shaders/overlay_bezel.glsl");
}

PostProcessor::~PostProcessor()
{
	if (quadVAO != UINT_MAX)
	{
			// Cleanup
		glDeleteVertexArrays(1, &quadVAO);
		glDeleteBuffers(1, &quadVBO);
	}
	quadVAO = UINT_MAX;
	quadVBO = UINT_MAX;

	if (FBO_prevFrame != UINT_MAX)
	{
		glDeleteFramebuffers(1, &FBO_prevFrame);
		glDeleteTextures(1, &prevFrame_texture_id);
	}
	FBO_prevFrame = UINT_MAX;
}

//////////////////////////////////////////////////////////////////////////
// Main methods
//////////////////////////////////////////////////////////////////////////

nlohmann::json PostProcessor::SerializeState()
{
	nlohmann::json jsonState = {
		{"preset_name", preset_name_buffer},
		{"bezelName", selectedBezelFile},
		{"bezelWidth", bezelSize.x},
		{"bezelHeight", bezelSize.y},
		{"p_f_bezelReflection", p_f_bezelReflection},
		{"p_f_reflectionBlur", p_f_reflectionBlur},
		{"p_i_postprocessingLevel", p_i_postprocessingLevel},
		{"bCRTFillWindow", bCRTFillWindow},
		{"integer_scale", integer_scale},
		{"bAutoScale", bAutoScale},
		{"bCRTFillWindow", bCRTFillWindow},
		{"p_f_corner", p_f_corner},
		{"p_b_smoothCorner", p_b_smoothCorner},
		{"p_b_extGamma", p_b_extGamma},
		{"p_f_interlace", p_f_interlace},
		{"p_b_slot", p_b_slot},
		{"p_f_bgr", p_f_bgr},
		{"p_f_black", p_f_black},
		{"p_f_brDep", p_f_brDep},
		{"p_f_brightness", p_f_brightness},
		{"p_i_cSpace", p_i_cSpace},
		{"p_f_cStr", p_f_cStr},
		{"p_f_convB", p_f_convB},
		{"p_f_convG", p_f_convG},
		{"p_f_convR", p_f_convR},
		{"p_f_hueGB", p_f_hueGB},
		{"p_i_maskType", p_i_maskType},
		{"p_f_maskHigh", p_f_maskHigh},
		{"p_f_maskLow", p_f_maskLow},
		{"p_f_maskSize", p_f_maskSize},
		{"p_f_hueRB", p_f_hueRB},
		{"p_f_hueRG", p_f_hueRG},
		{"p_f_saturation", p_f_saturation},
		{"p_f_scanlineWeight", p_f_scanlineWeight},
		{"p_f_scanSpeed", p_f_scanSpeed},
		{"p_f_flimGrain", p_f_filmGrain},
		{"p_i_scanlineType", p_i_scanlineType},
		{"p_f_slotW", p_f_slotW},
		{"p_f_vignetteWeight", p_f_vignetteWeight},
		{"p_f_barrelDistortion", p_f_barrelDistortion},
		{"p_f_ghostingPercent", p_f_ghostingPercent},
		{"p_f_phosphorBlur", p_f_phosphorBlur},
		{"p_v_warpX", p_v_warp.x},
		{"p_v_warpY", p_v_warp.y},
		{"p_v_centerX", p_v_center.x},
		{"p_v_centerY", p_v_center.y},
		{"p_v_zoomX", p_v_zoom.x},
		{"p_v_zoomY", p_v_zoom.y},
		{"p_v_reflectionScaleX", p_v_reflectionScale.x},
		{"p_v_reflectionScaleY", p_v_reflectionScale.y},
		{"p_v_reflectionTranslationX", p_v_reflectionTranslation.x},
		{"p_v_reflectionTranslationY", p_v_reflectionTranslation.y}
	};
	return jsonState;
}

void PostProcessor::DeserializeState(const nlohmann::json &jsonState)
{
	std::strncpy(preset_name_buffer, jsonState.value("preset_name", preset_name_buffer).c_str(), sizeof(preset_name_buffer) - 1);
	selectedBezelFile = jsonState.value("bezelName", selectedBezelFile);
	bezelSize.x = jsonState.value("bezelWidth", bezelSize.x);
	bezelSize.y = jsonState.value("bezelHeight", bezelSize.y);
	p_f_bezelReflection = jsonState.value("p_f_bezelReflection", p_f_bezelReflection);
	p_f_reflectionBlur = jsonState.value("p_f_reflectionBlur", p_f_reflectionBlur);
	p_i_postprocessingLevel = jsonState.value("p_i_postprocessingLevel", p_i_postprocessingLevel);
	bCRTFillWindow = jsonState.value("bCRTFillWindow", bCRTFillWindow);
	integer_scale = jsonState.value("integer_scale", integer_scale);
	bAutoScale = jsonState.value("bAutoScale", bAutoScale);
	p_f_corner = jsonState.value("p_f_corner", p_f_corner);
	p_b_smoothCorner = jsonState.value("p_b_smoothCorner", p_b_smoothCorner);
	p_b_extGamma = jsonState.value("p_b_extGamma", p_b_extGamma);
	p_f_interlace = jsonState.value("p_f_interlace", p_f_interlace);
	p_b_slot = jsonState.value("p_b_slot", p_b_slot);
	p_f_bgr = jsonState.value("p_f_bgr", p_f_bgr);
	p_f_black = jsonState.value("p_f_black", p_f_black);
	p_f_brDep = jsonState.value("p_f_brDep", p_f_brDep);
	p_f_brightness = jsonState.value("p_f_brightness", p_f_brightness);
	p_i_cSpace = jsonState.value("p_i_cSpace", p_i_cSpace);
	p_f_cStr = jsonState.value("p_f_cStr", p_f_cStr);
	p_f_convB = jsonState.value("p_f_convB", p_f_convB);
	p_f_convG = jsonState.value("p_f_convG", p_f_convG);
	p_f_convR = jsonState.value("p_f_convR", p_f_convR);
	p_f_hueGB = jsonState.value("p_f_hueGB", p_f_hueGB);
	p_i_maskType = jsonState.value("p_i_maskType", p_i_maskType);
	p_f_maskHigh = jsonState.value("p_f_maskHigh", p_f_maskHigh);
	p_f_maskLow = jsonState.value("p_f_maskLow", p_f_maskLow);
	p_f_maskSize = jsonState.value("p_f_maskSize", p_f_maskSize);
	p_f_hueRB = jsonState.value("p_f_hueRB", p_f_hueRB);
	p_f_hueRG = jsonState.value("p_f_hueRG", p_f_hueRG);
	p_f_saturation = jsonState.value("p_f_saturation", p_f_saturation);
	p_f_scanlineWeight = jsonState.value("p_f_scanlineWeight", p_f_scanlineWeight);
	p_f_scanSpeed = jsonState.value("p_f_scanSpeed", p_f_scanSpeed);
	p_f_filmGrain = jsonState.value("p_f_filmGrain", p_f_filmGrain);
	p_i_scanlineType = jsonState.value("p_i_scanlineType", p_i_scanlineType);
	p_f_slotW = jsonState.value("p_f_slotW", p_f_slotW);
	p_f_vignetteWeight = jsonState.value("p_f_vignetteWeight", p_f_vignetteWeight);
	p_f_barrelDistortion = jsonState.value("p_f_barrelDistortion", p_f_barrelDistortion);
	p_f_ghostingPercent = jsonState.value("p_f_ghostingPercent", p_f_ghostingPercent);
	p_f_phosphorBlur = jsonState.value("p_f_phosphorBlur", p_f_phosphorBlur);
	p_v_warp.x = jsonState.value("p_v_warpX", p_v_warp.x);
	p_v_warp.y = jsonState.value("p_v_warpY", p_v_warp.y);
	p_v_center.x = jsonState.value("p_v_centerX", p_v_center.x);
	p_v_center.y = jsonState.value("p_v_centerY", p_v_center.y);
	p_v_zoom.x = jsonState.value("p_v_zoomX", p_v_zoom.x);
	p_v_zoom.y = jsonState.value("p_v_zoomY", p_v_zoom.y);
	p_v_reflectionScale.x = jsonState.value("p_v_reflectionScaleX", p_v_reflectionScale.x);
	p_v_reflectionScale.y = jsonState.value("p_v_reflectionScaleY", p_v_reflectionScale.y);
	p_v_reflectionTranslation.x = jsonState.value("p_v_reflectionTranslationX", p_v_reflectionTranslation.x);
	p_v_reflectionTranslation.y = jsonState.value("p_v_reflectionTranslationY", p_v_reflectionTranslation.y);

}

void PostProcessor::SaveState(std::string filePath) {
	nlohmann::json jsonState = SerializeState();
	std::ofstream file(filePath);
	if (file.is_open()) {
		file << jsonState.dump(4); // Pretty print JSON
		file.close();
	}
}

void PostProcessor::LoadState(std::string filePath) {
	std::ifstream file(filePath);
	nlohmann::json jsonState;
	if (file.is_open()) {
		file >> jsonState;
		DeserializeState(jsonState);
	}
}

int PostProcessor::PopulateBezelFiles(std::vector<std::string>& _bezelFiles, const std::string& _selectedBezelFile) {
	int _selIdx = 0;
	_bezelFiles.clear();
	_bezelFiles.push_back(_PP_NO_BEZEL_FILENAME);  // Default for no bezel

	// populate bezel files
	for (const auto& entry : std::filesystem::directory_iterator("assets/bezels/")) {
		if (entry.is_regular_file() && (entry.path().extension() == ".png" || entry.path().extension() == ".jpg")) {
			_bezelFiles.push_back(entry.path().filename().string());
			if (_selectedBezelFile == entry.path().filename().string()) {
				_selIdx = (int)_bezelFiles.size() - 1;
			}
		}
	}
	return _selIdx;
}

void PostProcessor::SelectShader()
{
	// Choose the shader
	switch (p_i_postprocessingLevel)
	{
	case 0:	// basic passthrough shader with optional scanlines
	case 1:
		shaderProgram = v_ppshaders.at(0);
		shaderProgram.use();
		break;
	case 2:	// CRT shader
		shaderProgram = v_ppshaders.at(1);
		shaderProgram.use();
		// size info
		shaderProgram.setVec2("ViewportSize", glm::vec2(viewportWidth, viewportHeight));
		shaderProgram.setVec2("InputSize", glm::vec2(texWidth, texHeight));
		shaderProgram.setVec2("OutputSize", glm::vec2(quadWidth, quadHeight));

		// shader specific
		shaderProgram.setFloat("GhostingPercent", p_f_ghostingPercent);
		shaderProgram.setFloat("BlurSize", p_f_phosphorBlur);
		shaderProgram.setBool("bCORNER_SMOOTH", p_b_smoothCorner);
		shaderProgram.setBool("bEXT_GAMMA", p_b_extGamma);
		shaderProgram.setBool("bSLOT", p_b_slot);
		shaderProgram.setFloat("BARRELDISTORTION", p_f_barrelDistortion);
		shaderProgram.setFloat("BGR", p_f_bgr);
		shaderProgram.setFloat("BLACK", p_f_black);
		shaderProgram.setFloat("BR_DEP", p_f_brDep);
		shaderProgram.setFloat("BRIGHTNESS", p_f_brightness);
		shaderProgram.setFloat("C_STR", p_f_cStr);
		shaderProgram.setFloat("CONV_B", p_f_convB);
		shaderProgram.setFloat("CONV_G", p_f_convG);
		shaderProgram.setFloat("CONV_R", p_f_convR);
		shaderProgram.setFloat("CORNER", p_f_corner / 10000);
		shaderProgram.setFloat("GB", p_f_hueGB);
		shaderProgram.setFloat("MASKH", p_f_maskHigh);
		shaderProgram.setFloat("MASKL", p_f_maskLow);
		shaderProgram.setFloat("MSIZE", p_f_maskSize);
		shaderProgram.setFloat("RB", p_f_hueRB);
		shaderProgram.setFloat("RG", p_f_hueRG);
		shaderProgram.setFloat("SATURATION", p_f_saturation);
		shaderProgram.setFloat("SCANLINE_WEIGHT", p_f_scanlineWeight);
		shaderProgram.setFloat("SCAN_SPEED", p_f_scanSpeed);
		shaderProgram.setFloat("FILM_GRAIN", p_f_filmGrain);
		shaderProgram.setFloat("SLOTW", p_f_slotW);
		shaderProgram.setFloat("VIGNETTE_WEIGHT", p_f_vignetteWeight);
		shaderProgram.setFloat("INTERLACE_WEIGHT", p_f_interlace);
		shaderProgram.setInt("iCOLOR_SPACE", p_i_cSpace);
		shaderProgram.setInt("iM_TYPE", p_i_maskType);
		shaderProgram.setInt("iSCANLINE_TYPE", p_i_scanlineType);
		shaderProgram.setVec2("vWARP", p_v_warp);
		break;
	}
	// common
	shaderProgram.setInt("POSTPROCESSING_LEVEL", p_i_postprocessingLevel);
	shaderProgram.setVec2("TextureSize", glm::vec2(texWidth, texHeight));
}

void PostProcessor::RegeneratePreviousTexture()
{
	// Compute the pixel boundaries of the quad from its normalized coordinates.
// The conversion from normalized device coordinates (range [-1,1]) to pixel coordinates is:
//    pixel = (ndc * 0.5 + 0.5) * viewportDimension
	if (FBO_prevFrame == UINT_MAX)
	{
		glGenFramebuffers(1, &FBO_prevFrame);
		glGenTextures(1, &prevFrame_texture_id);
	}

	glm::vec4 quadCorners[4] = {
		glm::vec4(-1.0f, -1.0f, 0.0f, 1.0f),
		glm::vec4(1.0f, -1.0f, 0.0f, 1.0f),
		glm::vec4(1.0f,  1.0f, 0.0f, 1.0f),
		glm::vec4(-1.0f,  1.0f, 0.0f, 1.0f)
	};
	glm::vec4 transformed[4];
	for (int i = 0; i < 4; ++i)
	{
		transformed[i] = mTransform * quadCorners[i];
		if (transformed[i].w != 0.0f)
			transformed[i] /= transformed[i].w;
	}

	float nquadLeft = std::min({ transformed[0].x, transformed[1].x, transformed[2].x, transformed[3].x });
	float nquadRight = std::max({ transformed[0].x, transformed[1].x, transformed[2].x, transformed[3].x });
	float nquadBottom = std::max({ transformed[0].y, transformed[1].y, transformed[2].y, transformed[3].y });
	float nquadTop = std::min({ transformed[0].y, transformed[1].y, transformed[2].y, transformed[3].y });

	// rounding is critical, to properly align the previous frame texture
	tA2Quad.x = std::round((nquadLeft * 0.5 + 0.5) * viewportWidth);
	tA2Quad.y = std::round((nquadTop * 0.5 + 0.5) * viewportHeight);
	tA2Quad.w = std::round((nquadRight * 0.5 + 0.5) * viewportWidth - tA2Quad.x);
	tA2Quad.h = std::round((nquadBottom * 0.5 + 0.5) * viewportHeight - tA2Quad.y);

	glBindFramebuffer(GL_FRAMEBUFFER, FBO_prevFrame);
	glBindTexture(GL_TEXTURE_2D, prevFrame_texture_id);
	// Also here use GL_NEAREST to get rid of tiny rounding errors that will compound
	// dramatically at high ghosting values. Proper rounding and GL_NEAREST fix this.
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tA2Quad.w, tA2Quad.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, prevFrame_texture_id, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0); // Unbind FBO
	// Always bind the previous frame texture to its dedicated texture unit
	glActiveTexture(_TEXUNIT_PP_PREVIOUS);
	glBindTexture(GL_TEXTURE_2D, prevFrame_texture_id);
	glActiveTexture(GL_TEXTURE0);
	GLuint glerr;
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL error PP RegeneratePreviousTexture: " << glerr << std::endl;
	}
}

void PostProcessor::Render(SDL_Window* window, GLuint inputTextureSlot, GLuint scanlineCount)
{
	if (bezelImageAsset.tex_id == UINT_MAX)
	{
		glGenTextures(1, &bezelImageAsset.tex_id);
		glActiveTexture(_TEXUNIT_PP_BEZEL);
		glBindTexture(GL_TEXTURE_2D, bezelImageAsset.tex_id);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		std::vector<std::string> _bezelFiles;
		currentBezelIndex = PopulateBezelFiles(_bezelFiles, selectedBezelFile);
		if (currentBezelIndex > 0)
		{
			std::string bezelPath = "assets/bezels/" + selectedBezelFile;
			bezelImageAsset.AssignByFilename(bezelPath.c_str());
		}
		glActiveTexture(GL_TEXTURE0);
	}

	SDL_GL_GetDrawableSize(window, &viewportWidth, &viewportHeight);

	GLint last_viewport[4]; glGetIntegerv(GL_VIEWPORT, last_viewport);
	// Don't let the viewport have odd values. It creates artifacts when scaling
	if (viewportWidth % 2 == 1)
		viewportWidth -= 1;
	if (viewportHeight % 2 == 1)
		viewportHeight -= 1;
	glViewport(0, 0, viewportWidth, viewportHeight);
	GLenum glerr;
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL error PP 0: " << glerr << std::endl;
	}
	
	// Bind the texture we're given
	// And get its actual size.
	texUnitCurrent  = inputTextureSlot;
	glActiveTexture(texUnitCurrent);
	prev_texWidth = texWidth;
	prev_texHeight = texHeight;
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &texWidth);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &texHeight);
	glActiveTexture(GL_TEXTURE0);	// Target the main SDL window

	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL error PP 1: " << glerr << std::endl;
	}

	// How much can we scale the output quad?
	// Always scale up in integers numbers, but auto scale down is float
	float _scale = static_cast<float>(viewportWidth) / static_cast<float>(texWidth);
	_scale = std::min(_scale, static_cast<float>(viewportHeight) / static_cast<float>(texHeight));
	if (_scale > 1.0f)
		_scale = std::floor(_scale);
	else
		_scale = 1.0f;
	// make sure the scale doesn't extend beyond the GL max texture size
	max_integer_scale = std::min(maxTexSize / texHeight, maxTexSize / texWidth);

	if (!bAutoScale)
	{
		integer_scale = std::min(integer_scale, max_integer_scale);
		_scale = static_cast<float>(integer_scale);
	}
	else {
		integer_scale = std::min(static_cast<int>(_scale), max_integer_scale);
	}

	// Determine the quad's origin
	quadWidth = static_cast<int>(_scale * texWidth);
	quadHeight = static_cast<int>(_scale * texHeight);
	
	// Now determine the quad transformations as necessary, to send to the vertex shader
	glm::mat4 _transform = glm::mat4(1.0f);
	if (bCRTFillWindow && p_i_postprocessingLevel > 1)
	{
		// For full-window
		quadWidth = viewportWidth;
		quadHeight = viewportHeight;
	}
	// Compute scale factors based on desired quad dimensions.
	float scaleX = (quadWidth / static_cast<float>(viewportWidth));
	float scaleY = (quadHeight / static_cast<float>(viewportHeight));
	_transform = glm::scale(_transform, glm::vec3(scaleX*p_v_zoom.x, scaleY*p_v_zoom.y, 1.0f));
	_transform = glm::translate(_transform, glm::vec3(p_v_center.x/100.f, p_v_center.y/100.f, 0.0f));
	if ((_transform != mTransform)
		|| (FBO_prevFrame == UINT_MAX))
	{
		// std::cerr << "regenerating transform" << std::endl;
		mTransform = _transform;
		RegeneratePreviousTexture();
	}

	if (bImguiWindowIsOpen || (!shaderProgram.isReady)
		|| (prev_texWidth != texWidth) || (prev_texHeight != texHeight))
	{
		// only update the shader parameters in certain cases
		// as it may be very costly for rPi and slow CPUs
		this->SelectShader();
		prev_texWidth = texWidth;
		prev_texHeight = texHeight;
		if ((glerr = glGetError()) != GL_NO_ERROR) {
			std::cerr << "OpenGL error PP shaderProgram select: " << glerr << std::endl;
		}
	}
	else
	{
		shaderProgram.use();
		if ((glerr = glGetError()) != GL_NO_ERROR) {
			std::cerr << "OpenGL error PP shaderProgram use: " << glerr << std::endl;
		}
	}

	// Used for all PP shaders
	shaderProgram.setMat4("uTransform", mTransform);		// in the vertex shader
	shaderProgram.setInt("A2TextureCurrent", texUnitCurrent - GL_TEXTURE0);
	shaderProgram.setInt("PreviousFrame", _TEXUNIT_PP_PREVIOUS - GL_TEXTURE0);
	shaderProgram.setInt("iFrameCount", frame_count);
	shaderProgram.setBool("bHalveFrameRate", bHalveFramerate);
	// Only used for the full PP shader
	if (p_i_postprocessingLevel > 1) {
		shaderProgram.setVec2("OutputSize", glm::vec2(quadWidth, quadHeight));
		shaderProgram.setUInt("ScanlineCount", scanlineCount);
	}

	// Bind the quad VAO and draw the quad (static VBO already set up)
	glBindVertexArray(quadVAO);
	glDrawArrays(GL_TRIANGLES, 0, 6);

	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL error PP 2: " << glerr << std::endl;
	}

	// Now Build and Draw the Bezel if necessary
	if (currentBezelIndex > 0)
	{
		shaderProgramBezel.use();
		glm::mat4 transformBezel = glm::mat4(1.0f);
		//transformBezel = glm::translate(transformBezel, glm::vec3(static_cast<float>(viewportWidth)*bezelSize.x, static_cast<float>(viewportHeight) * bezelSize.y, 0.0f));
		transformBezel = glm::scale(transformBezel, glm::vec3(bezelSize.x, bezelSize.y, 1.0f));
		shaderProgramBezel.setMat4("uTransform", transformBezel);		// in the vertex shader
		shaderProgramBezel.setInt("uMainTex", _TEXUNIT_PP_BEZEL - GL_TEXTURE0);
		shaderProgramBezel.setInt("uA2Tex", _TEXUNIT_POSTPROCESS - GL_TEXTURE0);
		shaderProgramBezel.setFloat("uReflectionAmount", p_f_bezelReflection);
		shaderProgramBezel.setFloat("uReflectionBlur", p_f_reflectionBlur);
		shaderProgramBezel.setVec2("uReflectionScale", p_v_reflectionScale);
		shaderProgramBezel.setVec2("uReflectionTranslation", p_v_reflectionTranslation);
		shaderProgramBezel.setBool("uOutlineQuad", p_b_outlineQuad);

		glActiveTexture(_TEXUNIT_POSTPROCESS);
		if (p_f_bezelReflection > 0.001)
		{
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
		} else {
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        }
		glActiveTexture(_TEXUNIT_PP_BEZEL);
		glBindTexture(GL_TEXTURE_2D, bezelImageAsset.tex_id);
		glBindVertexArray(quadVAO);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		glActiveTexture(GL_TEXTURE0);
	}

	glBindVertexArray(0);

	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL error PP 3: " << glerr << std::endl;
	}

	/////////////////////////// BEGIN PREVIOUS FRAME TEXTURE ///////////////////////////

	// DO NOT COPY INTO THE PREVIOUS FRAME TEXTURE UNLESS IT IS REQUIRED
	// THIS _DRAMATICALLY_ REDUCES THE FPS ON A RASPBERRY PI
	if ((p_f_ghostingPercent > 0.0000001f) || bHalveFramerate)
	{


		if ((glerr = glGetError()) != GL_NO_ERROR) {
			std::cerr << "OpenGL error PP 4: " << glerr << std::endl;
		}

		// Now copy the screen texture to prevFrame_texture_id, to use it for the next frame
		// NOTE: prevFrame is flipped on the Y axis, so we flip Y on the destination to realign it
		glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, FBO_prevFrame);
		glBlitFramebuffer(tA2Quad.x, tA2Quad.y, tA2Quad.w + tA2Quad.x, tA2Quad.h + tA2Quad.y,	// source rectangle (quad region)
			0, tA2Quad.h, tA2Quad.w, 0,									// destination rectangle (Y flipped)
			GL_COLOR_BUFFER_BIT, GL_NEAREST);

		if ((glerr = glGetError()) != GL_NO_ERROR) {
			std::cerr << "OpenGL error PP glBlitFramebuffer: " << glerr << std::endl;
		}
	}

	//////////////////////////// END OF PREVIOUS FRAME TEXTURE ///////////////////////////

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glViewport(last_viewport[0], last_viewport[1], (GLsizei)last_viewport[2], (GLsizei)last_viewport[3]);
	// revert the texture assignment
	glActiveTexture(GL_TEXTURE0);
	++frame_count;
}


void PostProcessor::ResetToDefaults()
{
	memset(preset_name_buffer, 0, sizeof(preset_name_buffer));

	max_integer_scale = 1;
	integer_scale = 1;		// Base integer scale used
	bAutoScale = true;		// Automatically scale to max scale?
	bHalveFramerate = false;	// Mixes every pair of frames, to avoid page flip flicker
	bCRTFillWindow = false;

	selectedBezelFile = _PP_NO_BEZEL_FILENAME;
	currentBezelIndex = 0;
	bezelSize = glm::vec2(1.0f, 1.0f);

	p_b_smoothCorner = false;
	p_b_extGamma = false;
	p_b_slot = false;
	p_f_barrelDistortion = 0.0f;
	p_f_bgr = 0.0f;
	p_f_black = 0.0f;
	p_f_brDep = 0.2f;
	p_f_brightness = 1.0f;
	p_f_convB = 0.0f;
	p_f_convG = 0.0f;
	p_f_convR = 0.0f;
	p_f_corner = 0.0f;
	p_f_cStr = 0.0f;
	p_f_hueGB = 0.0f;
	p_f_hueRB = 0.0f;
	p_f_hueRG = 0.0f;
	p_f_maskHigh = 0.75f;
	p_f_maskLow = 0.3f;
	p_f_maskSize = 1.0f;
	p_f_saturation = 1.0f;
	p_f_scanlineWeight = 1.0f;
	p_f_scanSpeed = 1.0f;
	p_f_filmGrain = 0.0f;
	p_f_interlace = 0.f;
	p_f_slotW = 3.0f;
	p_f_vignetteWeight = 0.0f;
	p_i_cSpace = 0;
	p_i_maskType = 0;
	p_i_postprocessingLevel = 0;
	p_i_scanlineType = 2;
	p_f_ghostingPercent = 0;
	p_f_phosphorBlur = 0.0f;
	p_v_warp = glm::vec2(0.0f, 0.0f);
	p_v_center = glm::vec2(0.0f, 0.0f);
	p_v_zoom = glm::vec2(1.0f, 1.0f);

	// bezel shader variables
	p_b_outlineQuad = false;
	p_f_bezelReflection = 0.0f;
	p_f_reflectionBlur = 0.0f;
	p_v_reflectionScale = glm::vec2(1.0f, 1.0f);
	p_v_reflectionTranslation = glm::vec2(0.0f, 0.0f);

	// imgui vars
	bImGuiLockWarp = false;
	bImGuiLockZoom = false;
}

void PostProcessor::DisplayImGuiWindow(bool* p_open)
{
	bImguiWindowIsOpen = p_open;
	if (p_open)
	{
		ImGui::SetNextWindowSizeConstraints(ImVec2(450, 400), ImVec2(FLT_MAX, FLT_MAX));
		ImGui::Begin("Post Processing CRT Shader", p_open);

		if (ImGui::Button("Reset to defaults"))
		{
			ResetToDefaults();
		}

		// Handle presets
		ImGui::Text("[ PRESETS ]");
		IGFD::FileDialogConfig config;
		config.path = "./presets/";
		static ImGuiFileDialog instance_presets;
		if (ImGui::Button("Load")) {
			if (instance_presets.IsOpened())
				instance_presets.Close();
			ImGui::SetNextWindowSize(ImVec2(800, 400));
			instance_presets.OpenDialog("LoadStateDlg", "Choose File to Load", ".json", config);
		}
		ImGui::SameLine(); ImGui::Spacing(); ImGui::SameLine();	// Start Name
		if (std::strlen(preset_name_buffer) == 0) {
			std::strncpy(preset_name_buffer, "Untitled", sizeof(preset_name_buffer) - 1);
		}
		ImGui::PushItemWidth(200);
		ImGui::InputText("Name", preset_name_buffer, sizeof(preset_name_buffer));
		ImGui::PopItemWidth();
		ImGui::SameLine(); ImGui::Spacing(); ImGui::SameLine();	// End Name
		if (ImGui::Button("Save")) {
			if (instance_presets.IsOpened())
				instance_presets.Close();
			ImGui::SetNextWindowSize(ImVec2(800, 400));
			instance_presets.OpenDialog("SaveStateDlg", "Choose Save Location", ".json", config);
		}

		if (instance_presets.Display("LoadStateDlg")) {
			if (instance_presets.IsOk()) {
				std::string filePath = instance_presets.GetFilePathName();
				LoadState(filePath);
			}
			instance_presets.Close();
		}

		if (instance_presets.Display("SaveStateDlg")) {
			if (instance_presets.IsOk()) {
				std::string filePath = instance_presets.GetFilePathName();
				SaveState(filePath);
			}
			instance_presets.Close();
		}

		ImGui::PushItemWidth(200);

		// PP Type
		ImGui::Separator();
		ImGui::Text("[ POSTPROCESSING LEVEL ]");
		ImGui::RadioButton("None##PPLEVEL", &p_i_postprocessingLevel, 0); ImGui::SameLine();
		ImGui::SetItemTooltip("No postprocessing, optional bezel. Fastest!");
		ImGui::RadioButton("Scanline only##PPLEVEL", &p_i_postprocessingLevel, 1); ImGui::SameLine();
		ImGui::SetItemTooltip("Simple alternating scanlines, optional bezel. Fast.");
		ImGui::RadioButton("Full CRT##PPLEVEL", &p_i_postprocessingLevel, 2);
		ImGui::SetItemTooltip("The one and only Super Duper CRT shader. Customize away!");
		ImGui::Separator();
		if (p_i_postprocessingLevel == 2) {
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Shader Programming: ");
			ImGui::SameLine();
			// Enable to choose the shader
			ImGui::SameLine();
			if (ImGui::Button("Select"))
			{
				IGFD::FileDialogConfig config;
				config.path = "./shaders/";
				ImGuiFileDialog::Instance()->OpenDialog("ChooseShader1DlgKey", "Choose File", ".glsl,", config);
			}
			if (ImGuiFileDialog::Instance()->Display("ChooseShader1DlgKey")) {
				// Check if a file was selected
				if (ImGuiFileDialog::Instance()->IsOk()) {
					v_ppshaders.at(1).build(
						ImGuiFileDialog::Instance()->GetFilePathName().c_str(),
						ImGuiFileDialog::Instance()->GetFilePathName().c_str()
					);
				}
				ImGuiFileDialog::Instance()->Close();
			}

			ImGui::Separator();
		}
		ImGui::Text("[ BASE INTEGER SCALE ]");
		ImGui::Checkbox("Auto", &bAutoScale);
		ImGui::SetItemTooltip("Automatically selects the largest output possible, with pixel perfect scaling");
		if (bAutoScale)
			ImGui::BeginDisabled();
		ImGui::SliderInt("Integer Scale", &integer_scale, 1, max_integer_scale, "%d");
		if (bAutoScale)
			ImGui::EndDisabled();

		ImGui::Separator();

		ImGui::Text("[ GEOMETRY & OVERLAY]");
		// GEOMETRY
		if (ImGui::Checkbox("Fill Window", &bCRTFillWindow))
		{
			p_v_zoom.x = 1.0f;
			p_v_zoom.y = 1.0f;
			p_v_center.x = 0;
			p_v_center.y = 0;

		}
		if (bImGuiLockZoom)
		{
			float uniform = p_v_zoom.x;
			p_v_zoom.y = p_v_zoom.x;
			if (ImGui::DragFloat("Image Zoom", &uniform, 0.001f, 0.001f, 5.0f, "%.3f"))
				p_v_zoom = glm::vec2(uniform);
		}
		else
			ImGui::DragFloat2("Image Zoom", reinterpret_cast<float*>(&p_v_zoom), 0.001f, 0.001f, 5.0f, "%.3f");
		ImGui::SameLine(); ImGui::Spacing();
		ImGui::SameLine(); ImGui::Checkbox("Uniform##Zoom", &bImGuiLockZoom);

		ImGui::DragFloat2("Image Center", reinterpret_cast<float*>(&p_v_center), 0.1f, -100.0f, 100.0f, "%.2f");

		// OVERLAY
		currentBezelIndex = 0;
		std::vector<std::string> _bezelFiles;
		currentBezelIndex = PopulateBezelFiles(_bezelFiles, selectedBezelFile);

		std::vector<const char*> bezelFileCStrs;
		for (const auto& file : _bezelFiles) {
			bezelFileCStrs.push_back(file.c_str());
		}
		if (ImGui::Combo("Image", &currentBezelIndex, bezelFileCStrs.data(), static_cast<int>(bezelFileCStrs.size())))
		{
			selectedBezelFile = _bezelFiles[currentBezelIndex];
			if (currentBezelIndex > 0)
			{
				std::string bezelPath = "assets/bezels/" + selectedBezelFile;
				glActiveTexture(_TEXUNIT_PP_BEZEL);
				bezelImageAsset.AssignByFilename(bezelPath.c_str());
				glActiveTexture(GL_TEXTURE0);
			}
		}
		ImGui::SliderFloat("Overlay Relative Width", &bezelSize.x, 0.f, 2.f, "%.2f");
		ImGui::SliderFloat("Overlay Relative Height", &bezelSize.y, 0.f, 2.f, "%.2f");
		p_b_outlineQuad = false;
		ImGui::SliderFloat("Bezel Reflection", &p_f_bezelReflection, 0.f, 0.5f, "%.3f");
		ImGui::SetItemTooltip("WARNING: Tricky to get right. Read on for more info: \n \
In order to make a very fast fake reflection technique, we mirror the Apple 2\n\
texture with blur, and superpose it onto the overlay only where the bezel has\n\
some transparency (>0, <1). Since each overlay is different, and your choice\n\
of screen, resolution or window size is unique, you're going to have to manually\n\
tweak the size and position of the mirrored texture to conform to your idea of\n\
a reflection for your bezel. It takes work but can provide a really valuable\n\
FX that makes the whole screen 'pop'. Tweak the scale and center when at 0 blur\n\
and strong reflection, then dial blur up and reflection down.");
		ImGui::SliderFloat("Reflection Blur", &p_f_reflectionBlur, 0.0f, 10.f, "%.3f");
		if (ImGui::DragFloat2("Reflection Scale", reinterpret_cast<float*>(&p_v_reflectionScale), 
			0.001f, 0.001f, 5.0f, "%.3f"))
			p_b_outlineQuad = true;
		if (ImGui::DragFloat2("Reflection Center", reinterpret_cast<float*>(&p_v_reflectionTranslation),
			0.001f, -4.f, 4.f, "%.3f"))
			p_b_outlineQuad = true;

		ImGui::Separator();

		ImGui::Text("[ FRAME MERGING ]");
		ImGui::Checkbox("Merge Frame Pairs", &bHalveFramerate);
		ImGui::SetItemTooltip("WARNING: SIGNIFICANT FPS IMPACT!\n\
Merges every pair of even and odd frames.\n\
Effectively halves the frame rate\n\
but removes any flickering associated\n\
with page flipping images");
		if (p_i_postprocessingLevel == 2) {
			ImGui::Separator();
			// Scanline and Interlacing
			ImGui::Text("[ SCANLINE TYPE ]");
			ImGui::RadioButton("None##SCANLINETYPE", &p_i_scanlineType, 0);
			ImGui::SameLine();
			if (ImGui::RadioButton("Simple##SCANLINETYPE", &p_i_scanlineType, 1))
			{
				p_f_scanlineWeight = 0.3f;
			}
			ImGui::SameLine();
			if (ImGui::RadioButton("Complex##SCANLINETYPE", &p_i_scanlineType, 2))
			{
				p_f_scanlineWeight = 1.0f;
			}
			ImGui::SetItemTooltip("You should generally tweak color settings (further down) when using the complex scanline type");
			if (p_i_scanlineType >= 2)
			{
				ImGui::SliderFloat("Scanline Weight", &p_f_scanlineWeight, 0.0f, 2.0f, "%.2f");
				ImGui::SliderFloat("Scanline Speed", &p_f_scanSpeed, 0.0f, 2.0f, "%.2f");
				ImGui::SliderFloat("Film Grain", &p_f_filmGrain, 0.0f, 1.0f, "%.2f");
				ImGui::SliderFloat("Vignette Weight", &p_f_vignetteWeight, 0.0f, 5.0f, "%.2f");
				ImGui::SetItemTooltip("Darker sides of the scanlines, works better when there's distortion");
				ImGui::SliderFloat("Interlacing", &p_f_interlace, 0.0f, 2.0f, "%.2f");
				ImGui::SetItemTooltip("If you really want to feel the pain of bad refresh rates");
			}

			ImGui::Separator();

			// Blurring and Ghosting
			ImGui::Text("[ BLUR & GHOSTING ]");
			ImGui::SliderFloat("Phosphor Blur", &p_f_phosphorBlur, 0.0, 2.0, "%.2f");
			ImGui::SetItemTooltip("Some screen blur");

			// We'll use a normalized slider value in [0,1]
			static float _ghostingSV = 100.0f * (1.0f - pow(1.0f - p_f_ghostingPercent / 100.0f, 0.25f));
			if (p_f_ghostingPercent < 0.001)
				_ghostingSV = 0.0f;
			if (ImGui::SliderFloat("Ghosting Amount", &_ghostingSV, 0.0f, 100.0f, "%.0f"))
			{
				// Map sliderValue to ghosting percentage
				// The mapping (1 - (1-x)^4) gives finer control near 100.
				p_f_ghostingPercent = 100.0f - 100.0f * powf(1.0f - _ghostingSV/100.f, 4.0f);
			}
			ImGui::SetItemTooltip("WARNING: SIGNIFICANT FPS IMPACT! \nMix in a bit of ghosting to smooth animations. \nOverdo it to emulate the Apple /// monitor!");

			ImGui::Separator();
			
			// Mask Settings
			ImGui::Text("[ MASK SETTINGS ]");
			ImGui::RadioButton("None##Mask", &p_i_maskType, 0); ImGui::SameLine();
			ImGui::RadioButton("CGWG##Mask", &p_i_maskType, 1); ImGui::SameLine();
			ImGui::RadioButton("RGB##Mask", &p_i_maskType, 2);
			ImGui::SliderFloat("Mask Size", &p_f_maskSize, 1.0f, 2.0f, "%.1f");
			ImGui::Checkbox("Slot Mask On/Off", &p_b_slot);
			ImGui::SliderFloat("Slot Mask Width", &p_f_slotW, 2.0f, 3.0f, "%.1f");
			ImGui::SliderFloat("Subpixels BGR/RGB", &p_f_bgr, 0.0f, 1.0f, "%.1f");
			ImGui::SliderFloat("Mask Brightness Dark", &p_f_maskLow, 0.0f, 1.0f, "%.2f");
			ImGui::SliderFloat("Mask Brightness Bright", &p_f_maskHigh, 0.0f, 1.0f, "%.2f");
			ImGui::Separator();
			
			// Geometry Settings
			ImGui::Text("[ ADVANCED GEOMETRY ]");
			if (bImGuiLockWarp)
			{
				float uniform = p_v_warp.x;
				p_v_warp.y = p_v_warp.x;
				if (ImGui::DragFloat("Curvature", &uniform, 0.001f, -0.5f, 0.5f, "%.3f"))
					p_v_warp = glm::vec2(uniform);
			}
			else
				ImGui::DragFloat2("Curvature", reinterpret_cast<float*>(&p_v_warp), 0.001f, -0.5f, 0.5f, "%.3f");
			ImGui::SameLine(); ImGui::Spacing();
			ImGui::SameLine(); ImGui::Checkbox("Uniform##Curvature", &bImGuiLockWarp);

			ImGui::SliderFloat("Barrel Distortion", &p_f_barrelDistortion, -0.30f, 5.00f, "%.2f");
			ImGui::SliderFloat("Corners Cut", &p_f_corner, 0.f, 100.f, "%.3f");
			ImGui::SameLine(); ImGui::Spacing();
			ImGui::SameLine(); ImGui::Checkbox("Smooth", &p_b_smoothCorner);
			ImGui::Separator();
			
			// Color Settings
			ImGui::Text("[ COLOR SETTINGS ]");
			ImGui::SliderFloat("Scan/Mask Brightness Dependence", &p_f_brDep, 0.0f, 0.5f, "%.3f");
			ImGui::SliderInt("Color Space: sRGB,PAL,NTSC-U,NTSC-J", &p_i_cSpace, 0, 3, "%1d");
			ImGui::SliderFloat("Saturation", &p_f_saturation, 0.0f, 3.0f, "%.2f");
			ImGui::SliderFloat("Brightness", &p_f_brightness, 0.0f, 4.0f, "%.2f");
			ImGui::SliderFloat("Black Level", &p_f_black, -0.50f, 0.50f, "%.2f");
			ImGui::SliderFloat("Green <-to-> Red Hue", &p_f_hueRG, -2.50f, 2.50f, "%.2f");
			ImGui::SliderFloat("Blue <-to-> Red Hue", &p_f_hueRB, -2.50f, 2.50f, "%.2f");
			ImGui::SliderFloat("Blue <-to-> Green Hue", &p_f_hueGB, -2.50f, 2.50f, "%.2f");
			ImGui::Checkbox("External Gamma In (Glow etc)", &p_b_extGamma);
			ImGui::Separator();
			
			// Convergence Settings
			ImGui::Text("[ CONVERGENCE SETTINGS ]");
			ImGui::SliderFloat("Convergence Overall Strength", &p_f_cStr, 0.0f, 0.5f, "%.2f");
			ImGui::SliderFloat("Convergence Red X-Axis", &p_f_convR, -3.0f, 3.0f, "%.2f");
			ImGui::SliderFloat("Convergence Green X-axis", &p_f_convG, -3.0f, 3.0f, "%.2f");
			ImGui::SliderFloat("Convergence Blue X-Axis", &p_f_convB, -3.0f, 3.0f, "%.2f");
		}

		ImGui::PopItemWidth();
		ImGui::End();
	}
}
