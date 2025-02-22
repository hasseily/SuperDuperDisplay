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
	if (FBO_prevFrame == UINT_MAX)
	{
		glGenFramebuffers(1, &FBO_prevFrame);
		glGenTextures(1, &prevFrame_texture_id);
	}
	if (quadVAO == UINT_MAX)
	{
		glGenVertexArrays(1, &quadVAO);
		glGenBuffers(1, &quadVBO);
	}

	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTexSize);

	v_ppshaders.clear();
	Shader shader_basic = Shader();
	shader_basic.build(_SHADER_VERTEX_BASIC, _SHADER_FRAGMENT_BASIC);
	v_ppshaders.push_back(shader_basic);
	Shader shader_pp = Shader();
	shader_pp.build("shaders/a2video_postprocess.glsl", "shaders/a2video_postprocess.glsl");
	v_ppshaders.push_back(shader_pp);
	memset(preset_name_buffer, 0, sizeof(preset_name_buffer));
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
		{"p_i_postprocessingLevel", p_i_postprocessingLevel},
		{"bCRTFillWindow", bCRTFillWindow},
		{"integer_scale", integer_scale},
		{"bAutoScale", bAutoScale},
		{"bCRTFillWindow", bCRTFillWindow},
		{"p_f_corner", p_f_corner},
		{"p_b_smoothCorner", p_b_smoothCorner},
		{"p_b_extGamma", p_b_extGamma},
		{"p_f_interlace", p_f_interlace},
		{"p_b_potato", p_b_potato},
		{"p_b_slot", p_b_slot},
		{"p_f_bgr", p_f_bgr},
		{"p_f_black", p_f_black},
		{"p_f_brDep", p_f_brDep},
		{"p_f_brightness", p_f_brightness},
		{"p_i_cSpace", p_i_cSpace},
		{"p_f_cStr", p_f_cStr},
		{"p_f_centerX", p_f_centerX},
		{"p_f_centerY", p_f_centerY},
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
		{"p_i_scanlineType", p_i_scanlineType},
		{"p_f_slotW", p_f_slotW},
		{"p_f_vignetteWeight", p_f_vignetteWeight},
		{"p_f_warpX", p_f_warpX},
		{"p_f_warpY", p_f_warpY},
		{"p_f_barrelDistortion", p_f_barrelDistortion},
		{"p_f_zoomX", p_f_zoomX},
		{"p_f_zoomY", p_f_zoomY},
		{"p_f_ghostingPercent", p_f_ghostingPercent},
		{"p_f_phosphorBlur", p_f_phosphorBlur}
	};
	return jsonState;
}

void PostProcessor::DeserializeState(const nlohmann::json &jsonState)
{
	std::strncpy(preset_name_buffer, jsonState.value("preset_name", preset_name_buffer).c_str(), sizeof(preset_name_buffer) - 1);
	p_i_postprocessingLevel = jsonState.value("p_i_postprocessingLevel", p_i_postprocessingLevel);
	bCRTFillWindow = jsonState.value("bCRTFillWindow", bCRTFillWindow);
	integer_scale = jsonState.value("integer_scale", integer_scale);
	bAutoScale = jsonState.value("bAutoScale", bAutoScale);
	p_f_corner = jsonState.value("p_f_corner", p_f_corner);
	p_b_smoothCorner = jsonState.value("p_b_smoothCorner", p_b_smoothCorner);
	p_b_extGamma = jsonState.value("p_b_extGamma", p_b_extGamma);
	p_f_interlace = jsonState.value("p_f_interlace", p_f_interlace);
	p_b_potato = jsonState.value("p_b_potato", p_b_potato);
	p_b_slot = jsonState.value("p_b_slot", p_b_slot);
	p_f_bgr = jsonState.value("p_f_bgr", p_f_bgr);
	p_f_black = jsonState.value("p_f_black", p_f_black);
	p_f_brDep = jsonState.value("p_f_brDep", p_f_brDep);
	p_f_brightness = jsonState.value("p_f_brightness", p_f_brightness);
	p_i_cSpace = jsonState.value("p_i_cSpace", p_i_cSpace);
	p_f_cStr = jsonState.value("p_f_cStr", p_f_cStr);
	p_f_centerX = jsonState.value("p_f_centerX", p_f_centerX);
	p_f_centerY = jsonState.value("p_f_centerY", p_f_centerY);
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
	p_i_scanlineType = jsonState.value("p_i_scanlineType", p_i_scanlineType);
	p_f_slotW = jsonState.value("p_f_slotW", p_f_slotW);
	p_f_vignetteWeight = jsonState.value("p_f_vignetteWeight", p_f_vignetteWeight);
	p_f_warpX = jsonState.value("p_f_warpX", p_f_warpX);
	p_f_warpY = jsonState.value("p_f_warpY", p_f_warpY);
	p_f_barrelDistortion = jsonState.value("p_f_barrelDistortion", p_f_barrelDistortion);
	p_f_zoomX = jsonState.value("p_f_zoomX", p_f_zoomX);
	p_f_zoomY = jsonState.value("p_f_zoomY", p_f_zoomY);
	p_f_ghostingPercent = jsonState.value("p_f_ghostingPercent", p_f_ghostingPercent);
	p_f_phosphorBlur = jsonState.value("p_f_phosphorBlur", p_f_phosphorBlur);
}

void PostProcessor::SaveState(int profile_id) {
	nlohmann::json jsonState = SerializeState();
	
	std::ostringstream filename;
	filename << "pp_profile_" << profile_id << ".json";
	std::ofstream file(filename.str());
	file << jsonState.dump(4); // Save with pretty printing
}

void PostProcessor::LoadState(int profile_id) {
	std::ostringstream filename;
	filename << "pp_profile_" << profile_id << ".json";
	std::ifstream file(filename.str());
	nlohmann::json jsonState;
	
	if (file.is_open()) {
		file >> jsonState;
		DeserializeState(jsonState);
	}
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
		shaderProgram.setVec4("VideoRect", quadViewportCoords);

		// shader specific
		shaderProgram.setFloat("GhostingPercent", p_f_ghostingPercent);
		shaderProgram.setFloat("BlurSize", p_f_phosphorBlur);
		shaderProgram.setBool("bCORNER_SMOOTH", p_b_smoothCorner);
		shaderProgram.setBool("bEXT_GAMMA", p_b_extGamma);
		shaderProgram.setBool("bPOTATO", p_b_potato);
		shaderProgram.setBool("bSLOT", p_b_slot);
		shaderProgram.setFloat("BARRELDISTORTION", p_f_barrelDistortion);
		shaderProgram.setFloat("BGR", p_f_bgr);
		shaderProgram.setFloat("BLACK", p_f_black);
		shaderProgram.setFloat("BR_DEP", p_f_brDep);
		shaderProgram.setFloat("BRIGHTNESS", p_f_brightness);
		shaderProgram.setFloat("C_STR", p_f_cStr);
		shaderProgram.setFloat("CENTERX", p_f_centerX);
		shaderProgram.setFloat("CENTERY", p_f_centerY);
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
		shaderProgram.setFloat("SLOTW", p_f_slotW);
		shaderProgram.setFloat("VIGNETTE_WEIGHT", p_f_vignetteWeight);
		shaderProgram.setFloat("INTERLACE_WEIGHT", p_f_interlace);
		shaderProgram.setFloat("WARPX", p_f_warpX);
		shaderProgram.setFloat("WARPY", p_f_warpY);
		shaderProgram.setFloat("ZOOMX", p_f_zoomX);
		shaderProgram.setFloat("ZOOMY", p_f_zoomY);
		shaderProgram.setInt("iCOLOR_SPACE", p_i_cSpace);
		shaderProgram.setInt("iM_TYPE", p_i_maskType);
		shaderProgram.setInt("iSCANLINE_TYPE", p_i_scanlineType);
		break;
	}
	// common
	shaderProgram.setInt("POSTPROCESSING_LEVEL", p_i_postprocessingLevel);
	shaderProgram.setVec2("TextureSize", glm::vec2(texWidth, texHeight));
}

void PostProcessor::Render(SDL_Window* window, GLuint inputTextureSlot, GLuint scanlineCount)
{
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

	if (bCRTFillWindow && p_i_postprocessingLevel > 1)
	{
		quadViewportCoords.x = -1.0;	// left
		quadViewportCoords.y = -1.0;	// top
		quadViewportCoords.z = 1.0;		// right
		quadViewportCoords.w = 1.0;		// bottom
	}
	else {
		quadViewportCoords.x = static_cast<float>(-quadWidth) / static_cast<float>(viewportWidth);		// left
		quadViewportCoords.y = static_cast<float>(-quadHeight) / static_cast<float>(viewportHeight);	// top
		quadViewportCoords.z = static_cast<float>(quadWidth) / static_cast<float>(viewportWidth);		// right
		quadViewportCoords.w = static_cast<float>(quadHeight) / static_cast<float>(viewportHeight);		// bottom
	}

	GLfloat quadVertices[] = {
		// Positions												// Texture Coords
		quadViewportCoords.x, quadViewportCoords.w,  	0.0f, 0.0f,
		quadViewportCoords.z, quadViewportCoords.y,  	1.0f, 1.0f,
		quadViewportCoords.z, quadViewportCoords.w,  	1.0f, 0.0f,
		
		quadViewportCoords.x, quadViewportCoords.w,  	0.0f, 0.0f,
		quadViewportCoords.x, quadViewportCoords.y,  	0.0f, 1.0f,
		quadViewportCoords.z, quadViewportCoords.y,  	1.0f, 1.0f
	};
	
//		std::cout << "Viewport coordinates:" << ": (" << quadViewportCoords[0] << ", " << quadViewportCoords[1]
//		<< "), (" << quadViewportCoords[2] << ", " << quadViewportCoords[3] << ")" << std::endl;
	
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
	shaderProgram.setInt("A2TextureCurrent", texUnitCurrent - GL_TEXTURE0);
	// Only used for the full PP shader
	if (p_i_postprocessingLevel > 1) {
		shaderProgram.setInt("PreviousFrame", _TEXUNIT_PP_PREVIOUS - GL_TEXTURE0);
		shaderProgram.setInt("FrameCount", frame_count);
		shaderProgram.setVec2("OutputSize", glm::vec2(quadWidth, quadHeight));
		shaderProgram.setUInt("ScanlineCount", scanlineCount);
	}

	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL error PP 2: " << glerr << std::endl;
	}
	
	// Now always send in the vertices because it all may have been resized upstream
	glBindVertexArray(quadVAO);
	glBindBuffer(GL_ARRAY_BUFFER, quadVBO);

	glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);

	// Position attribute
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (GLvoid*)0);

	// Texture coordinate attribute
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (GLvoid*)(2 * sizeof(GLfloat)));


	// Render the fullscreen quad
	// Target the main SDL2 window
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindVertexArray(0);

	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL error PP 3: " << glerr << std::endl;
	}

	// Compute the pixel boundaries of the quad from its normalized coordinates.
	// The conversion from normalized device coordinates (range [-1,1]) to pixel coordinates is:
	//    pixel = (ndc * 0.5 + 0.5) * viewportDimension
	int quadLeft   = (int)lround((quadViewportCoords.x * 0.5f + 0.5f) * viewportWidth);
	int quadRight  = (int)lround((quadViewportCoords.z * 0.5f + 0.5f) * viewportWidth);
	int quadTop    = (int)lround((quadViewportCoords.y * 0.5f + 0.5f) * viewportHeight);
	int quadBottom = (int)lround((quadViewportCoords.w * 0.5f + 0.5f) * viewportHeight);

	glBindFramebuffer(GL_FRAMEBUFFER, FBO_prevFrame);
	glBindTexture(GL_TEXTURE_2D, prevFrame_texture_id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, quadRight-quadLeft, quadBottom-quadTop, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, prevFrame_texture_id, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0); // Unbind FBO
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL error PP 4: " << glerr << std::endl;
	}
	// Always bind the previous frame texture to its dedicated texture unit
	glActiveTexture(_TEXUNIT_PP_PREVIOUS);
	glBindTexture(GL_TEXTURE_2D, prevFrame_texture_id);
	glActiveTexture(GL_TEXTURE0);

	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL error PP 5: " << glerr << std::endl;
	}

	// Now copy the screen texture to prevFrame_texture_id, to use it for the next frame
	// NOTE: prevFrame is flipped on the Y axis, so we flip Y on the destination to realign it
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, FBO_prevFrame);
	glBlitFramebuffer(quadLeft, quadTop, quadRight, quadBottom,	// source rectangle (quad region)
		0, quadBottom - quadTop, quadRight-quadLeft, 0,				// destination rectangle (Y flipped)
		GL_COLOR_BUFFER_BIT, GL_NEAREST);

	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL error PP glBlitFramebuffer: " << glerr << std::endl;
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glViewport(last_viewport[0], last_viewport[1], (GLsizei)last_viewport[2], (GLsizei)last_viewport[3]);
	// revert the texture assignment
	glActiveTexture(GL_TEXTURE0);
	++frame_count;
}

void PostProcessor::DisplayImGuiWindow(bool* p_open)
{
	bImguiWindowIsOpen = p_open;
	if (p_open)
	{
		ImGui::SetNextWindowSizeConstraints(ImVec2(420, 400), ImVec2(FLT_MAX, FLT_MAX));
		ImGui::Begin("Post Processing CRT Shader", p_open);
		// Handle presets. Disable load/save if the chosen button is "Off"
		ImGui::Text("[ PRESETS ]");
		if (idx_preset == 0)
		{
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f); // Reduce button opacity
			ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true); // Disable button (and make it unclickable)
		}
		if (ImGui::Button("Load##Presets"))
		{
			this->LoadState(idx_preset);
		}
		if (idx_preset == 0)
		{
			ImGui::PopItemFlag();
			ImGui::PopStyleVar();
		}
		ImGui::SameLine();
		ImGui::Dummy(ImVec2(5.0f, 0.0f));
		ImGui::SameLine();
		ImGui::RadioButton("None##Presets", &idx_preset, 0); ImGui::SameLine();
		ImGui::RadioButton("1##Presets", &idx_preset, 1); ImGui::SameLine();
		ImGui::RadioButton("2##Presets", &idx_preset, 2); ImGui::SameLine();
		ImGui::RadioButton("3##Presets", &idx_preset, 3); ImGui::SameLine();
		ImGui::RadioButton("4##Presets", &idx_preset, 4); ImGui::SameLine();
		ImGui::RadioButton("5##Presets", &idx_preset, 5); ImGui::SameLine();
		ImGui::RadioButton("6##Presets", &idx_preset, 6); ImGui::SameLine();
		ImGui::RadioButton("7##Presets", &idx_preset, 7);
		ImGui::SameLine();
		ImGui::Dummy(ImVec2(5.0f, 0.0f));
		ImGui::SameLine();
		
		if (idx_preset == 0)
		{
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f); // Reduce button opacity
			ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true); // Disable button (and make it unclickable)
		}
		if (ImGui::Button("Save##Presets"))
		{
			this->SaveState(idx_preset);
		}
		if (idx_preset == 0)
		{
			ImGui::PopItemFlag();
			ImGui::PopStyleVar();
		}
		if (std::strlen(preset_name_buffer) == 0) {
			std::strncpy(preset_name_buffer, "Unnamed", sizeof(preset_name_buffer) - 1);
		}
		ImGui::PushItemWidth(200);
		ImGui::InputText("Preset Name", preset_name_buffer, sizeof(preset_name_buffer));
		ImGui::PopItemWidth();

		ImGui::PushItemWidth(200);

		// PP Type
		ImGui::Separator();
		ImGui::Text("[ POSTPROCESSING LEVEL ]");
		ImGui::RadioButton("None##PPLEVEL", &p_i_postprocessingLevel, 0); ImGui::SameLine();
		ImGui::SetItemTooltip("No postprocessing, fast");
		ImGui::RadioButton("Scanline only##PPLEVEL", &p_i_postprocessingLevel, 1); ImGui::SameLine();
		ImGui::SetItemTooltip("Simple alternating scanlines, fastest");
		ImGui::RadioButton("Full CRT##PPLEVEL", &p_i_postprocessingLevel, 2);
		ImGui::SetItemTooltip("Customizable CRT shader, slow");
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
				ImGui::SliderFloat("Scanline Speed", &p_f_scanSpeed, 0.0f, 4.0f, "%.2f");
				ImGui::SliderFloat("Vignette Weight", &p_f_vignetteWeight, 0.0f, 5.0f, "%.2f");
				ImGui::SetItemTooltip("Darker sides of the scanlines, works better when there's distortion");
				ImGui::SliderFloat("Interlacing", &p_f_interlace, 0.0f, 5.0f, "%.2f");
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
			ImGui::SetItemTooltip("Mix in a bit of ghosting to smooth bad framerates. Overdo it for the Apple /// monitor");

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
			ImGui::Text("[ GEOMETRY SETTINGS ]");
			if (ImGui::Checkbox("Fill Window", &bCRTFillWindow))
			{
				p_f_zoomX = 0;
				p_f_zoomY = 0;
				p_f_centerX = 0;
				p_f_centerY = 0;

			}
			ImGui::SliderFloat("Zoom Image X", &p_f_zoomX, -2.0f, 2.0f, "%.3f");
			ImGui::SliderFloat("Zoom Image Y", &p_f_zoomY, -2.0f, 2.0f, "%.3f");
			ImGui::SliderFloat("Image Center X", &p_f_centerX, -100.0f, 100.0f, "%.2f");
			ImGui::SliderFloat("Image Center Y", &p_f_centerY, -100.0f, 100.0f, "%.2f");
			ImGui::SliderFloat("Curvature Horizontal", &p_f_warpX, 0.00f, 0.25f, "%.2f");
			ImGui::SliderFloat("Curvature Vertical", &p_f_warpY, 0.00f, 0.25f, "%.2f");
			ImGui::SliderFloat("Barrel Distortion", &p_f_barrelDistortion, -0.30f, 5.00f, "%.2f");
			ImGui::SliderFloat("Corners Cut", &p_f_corner, 0.f, 100.f, "%.3f");
			ImGui::Spacing();ImGui::SameLine();ImGui::Checkbox("Smooth Corners", &p_b_smoothCorner);
			ImGui::Separator();
			
			// Color Settings
			ImGui::Text("[ COLOR SETTINGS ]");
			ImGui::SliderFloat("Scan/Mask Brightness Dependence", &p_f_brDep, 0.0f, 0.5f, "%.3f");
			ImGui::SliderInt("Color Space: sRGB,PAL,NTSC-U,NTSC-J", &p_i_cSpace, 0, 3, "%1d");
			ImGui::SliderFloat("Saturation", &p_f_saturation, 0.0f, 3.0f, "%.2f");
			ImGui::SliderFloat("Brightness", &p_f_brightness, 0.0f, 4.0f, "%.2f");
			ImGui::SliderFloat("Black Level", &p_f_black, -0.50f, 0.50f, "%.2f");
			ImGui::SliderFloat("Green <-to-> Red Hue", &p_f_hueRG, -0.25f, 0.25f, "%.2f");
			ImGui::SliderFloat("Blue <-to-> Red Hue", &p_f_hueRB, -0.25f, 0.25f, "%.2f");
			ImGui::SliderFloat("Blue <-to-> Green Hue", &p_f_hueGB, -0.25f, 0.25f, "%.2f");
			ImGui::Checkbox("External Gamma In (Glow etc)", &p_b_extGamma);
			ImGui::Separator();
			
			// Convergence Settings
			ImGui::Text("[ CONVERGENCE SETTINGS ]");
			ImGui::SliderFloat("Convergence Overall Strength", &p_f_cStr, 0.0f, 0.5f, "%.2f");
			ImGui::SliderFloat("Convergence Red X-Axis", &p_f_convR, -3.0f, 3.0f, "%.2f");
			ImGui::SliderFloat("Convergence Green X-axis", &p_f_convG, -3.0f, 3.0f, "%.2f");
			ImGui::SliderFloat("Convergence Blue X-Axis", &p_f_convB, -3.0f, 3.0f, "%.2f");
			ImGui::Checkbox("Potato Boost(Simple Gamma, adjust Mask)", &p_b_potato);
		}

		ImGui::PopItemWidth();
		ImGui::End();
	}
}
