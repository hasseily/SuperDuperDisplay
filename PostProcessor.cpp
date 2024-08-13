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
	v_ppshaders.clear();
	Shader shader_basic = Shader();
	shader_basic.build(_SHADER_VERTEX_BASIC, _SHADER_FRAGMENT_BASIC);
	v_ppshaders.push_back(shader_basic);
	Shader shader_pp = Shader();
	shader_pp.build("shaders/a2video_postprocess.glsl", "shaders/a2video_postprocess.glsl");
	v_ppshaders.push_back(shader_pp);
}

PostProcessor::~PostProcessor()
{
	if (quadVAO != UINT_MAX)
	{
			// Cleanup
		glDeleteVertexArrays(1, &quadVAO);
		glDeleteBuffers(1, &quadVBO);
	}
}

//////////////////////////////////////////////////////////////////////////
// Main methods
//////////////////////////////////////////////////////////////////////////

nlohmann::json PostProcessor::SerializeState()
{
	nlohmann::json jsonState = {
		{"p_i_postprocessingLevel", p_i_postprocessingLevel},
		{"bCRTFillWindow", bCRTFillWindow},
		{"integer_scale", integer_scale},
		{"bAutoScale", bAutoScale},
		{"bCRTFillWindow", bCRTFillWindow},
		{"p_f_corner", p_f_corner},
		{"p_b_extGamma", p_b_extGamma},
		{"p_b_interlace", p_b_interlace},
		{"p_b_potato", p_b_potato},
		{"p_b_slot", p_b_slot},
		{"p_b_vignette", p_b_vignette},
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
		{"p_i_scanlineType", p_i_scanlineType},
		{"p_f_slotW", p_f_slotW},
		{"p_f_vignetteWeight", p_f_vignetteWeight},
		{"p_f_warpX", p_f_warpX},
		{"p_f_warpY", p_f_warpY},
		{"p_f_barrelDistortion", p_f_barrelDistortion},
		{"p_f_zoomX", p_f_zoomX},
		{"p_f_zoomY", p_f_zoomY}
	};
	return jsonState;
}

void PostProcessor::DeserializeState(const nlohmann::json &jsonState)
{
	p_i_postprocessingLevel = jsonState.value("p_i_postprocessingLevel", p_i_postprocessingLevel);
	bCRTFillWindow = jsonState.value("bCRTFillWindow", bCRTFillWindow);
	integer_scale = jsonState.value("integer_scale", integer_scale);
	bAutoScale = jsonState.value("bAutoScale", bAutoScale);
	p_f_corner = jsonState.value("p_f_corner", p_f_corner);
	p_b_extGamma = jsonState.value("p_b_extGamma", p_b_extGamma);
	p_b_interlace = jsonState.value("p_b_interlace", p_b_interlace);
	p_b_potato = jsonState.value("p_b_potato", p_b_potato);
	p_b_slot = jsonState.value("p_b_slot", p_b_slot);
	p_b_vignette = jsonState.value("p_b_vignette", p_b_vignette);
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
	p_i_scanlineType = jsonState.value("p_i_scanlineType", p_i_scanlineType);
	p_f_slotW = jsonState.value("p_f_slotW", p_f_slotW);
	p_f_vignetteWeight = jsonState.value("p_f_vignetteWeight", p_f_vignetteWeight);
	p_f_warpX = jsonState.value("p_f_warpX", p_f_warpX);
	p_f_warpY = jsonState.value("p_f_warpY", p_f_warpY);
	p_f_barrelDistortion = jsonState.value("p_f_barrelDistortion", p_f_barrelDistortion);
	p_f_zoomX = jsonState.value("p_f_zoomX", p_f_zoomX);
	p_f_zoomY = jsonState.value("p_f_zoomY", p_f_zoomY);
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
	// Frame count is always set, outside of the shader selection
	switch (p_i_postprocessingLevel)
	{
	case 0:	// basic passthrough shader with optional scanlines
	case 1:
		shaderProgram = v_ppshaders.at(0);
		shaderProgram.use();
		shaderProgram.setInt("Texture", _PP_INPUT_TEXTURE_UNIT - GL_TEXTURE0);
		shaderProgram.setInt("POSTPROCESSING_LEVEL", p_i_postprocessingLevel);
		shaderProgram.setVec2("TextureSize", glm::vec2(texWidth, texHeight));
		break;
	case 2:	// CRT shader
		shaderProgram = v_ppshaders.at(1);
		shaderProgram.use();
		// common
		shaderProgram.setInt("A2Texture", _PP_INPUT_TEXTURE_UNIT - GL_TEXTURE0);
		shaderProgram.setVec2("ViewportSize", glm::vec2(viewportWidth, viewportHeight));
		shaderProgram.setVec2("InputSize", glm::vec2(texWidth, texHeight));
		shaderProgram.setVec2("TextureSize", glm::vec2(texWidth, texHeight));
		shaderProgram.setVec2("OutputSize", glm::vec2(quadWidth, quadHeight));
		shaderProgram.setVec4("VideoRect", quadViewportCoords);
		shaderProgram.setInt("POSTPROCESSING_LEVEL", p_i_postprocessingLevel);

		// shader specific
		shaderProgram.setBool("bEXT_GAMMA", p_b_extGamma);
		shaderProgram.setBool("bINTERLACE", p_b_interlace);
		shaderProgram.setBool("bPOTATO", p_b_potato);
		shaderProgram.setBool("bSLOT", p_b_slot);
		shaderProgram.setBool("bVIGNETTE", p_b_vignette);
		shaderProgram.setFloat("BARRELDISTORTION", p_f_barrelDistortion);
		shaderProgram.setFloat("BGR", p_f_bgr);
		shaderProgram.setFloat("BLACK", p_f_black);
		shaderProgram.setFloat("BR_DEP", p_f_brDep);
		shaderProgram.setFloat("BRIGHTNESs", p_f_brightness);
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
		shaderProgram.setFloat("SLOTW", p_f_slotW);
		shaderProgram.setFloat("VIGNETTE_WEIGHT", p_f_vignetteWeight);
		shaderProgram.setFloat("WARPX", p_f_warpX);
		shaderProgram.setFloat("WARPY", p_f_warpY);
		shaderProgram.setFloat("ZOOMX", p_f_zoomX);
		shaderProgram.setFloat("ZOOMY", p_f_zoomY);
		shaderProgram.setInt("iCOLOR_SPACE", p_i_cSpace);
		shaderProgram.setInt("iM_TYPE", p_i_maskType);
		shaderProgram.setInt("iSCANLINE_TYPE", p_i_scanlineType);
		break;
	}
}

void PostProcessor::Render(SDL_Window* window, GLuint inputTextureId)
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
	
	// Bind the texture we're given to our _POSTPROCESSOR_INPUT_TEXTURE
	// And get its actual size.
	glActiveTexture(_PP_INPUT_TEXTURE_UNIT);
	GLint last_bound_texture = 0;
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_bound_texture);
	glBindTexture(GL_TEXTURE_2D, inputTextureId);
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
	max_integer_scale = std::max(1, static_cast<int>(_scale) + 3);	// allow for manual further zoom
	integer_scale = std::min(integer_scale, max_integer_scale);
	if (!bAutoScale)
		_scale = static_cast<float>(integer_scale);

	// Determine the quad's origin
	quadWidth = static_cast<int>(_scale * texWidth);
	quadHeight = static_cast<int>(_scale * texHeight);

	if (bCRTFillWindow && p_i_postprocessingLevel > 1)
	{
		quadViewportCoords.x = -1.0;		// left
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
		|| (last_bound_texture != inputTextureId)
		|| (prev_texWidth != texWidth) || (prev_texHeight != texHeight))
	{
		// only update the shader parameters in certain cases
		// as it may be very costly for rPi and slow CPUs
		this->SelectShader();
		last_bound_texture = inputTextureId;
		prev_texWidth = texWidth;
		prev_texHeight = texHeight;
	}
	else
	{
		shaderProgram.use();
	}

	// Always set the frame count!
	shaderProgram.setInt("FrameCount", frame_count);
	shaderProgram.setVec2("OutputSize", glm::vec2(quadWidth, quadHeight));

	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL error PP 2: " << glerr << std::endl;
	}

	// Setup fullscreen quad VAO and VBO
	if (quadVAO == UINT_MAX)
	{
		glGenVertexArrays(1, &quadVAO);
		glGenBuffers(1, &quadVBO);
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
	glViewport(last_viewport[0], last_viewport[1], (GLsizei)last_viewport[2], (GLsizei)last_viewport[3]);
	// revert the texture assignment
	glActiveTexture(_PP_INPUT_TEXTURE_UNIT);
	glBindTexture(GL_TEXTURE_2D, last_bound_texture);
	glActiveTexture(GL_TEXTURE0);
	++frame_count;
}

void PostProcessor::DisplayImGuiWindow(bool* p_open)
{
	bImguiWindowIsOpen = p_open;
	if (p_open)
	{
		ImGui::SetNextWindowSizeConstraints(ImVec2(400, 400), ImVec2(FLT_MAX, FLT_MAX));
		ImGui::Begin("Post Processing CRT Shader", p_open);
		// Handle presets. Disable load/save if the chosen button is "Off"
		ImGui::Text("[ PRESETS ]");
		if (v_presets == 0)
		{
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f); // Reduce button opacity
			ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true); // Disable button (and make it unclickable)
		}
		if (ImGui::Button("Load##Presets"))
		{
			this->LoadState(v_presets);
		}
		if (v_presets == 0)
		{
			ImGui::PopItemFlag();
			ImGui::PopStyleVar();
		}
		ImGui::SameLine();
		ImGui::Dummy(ImVec2(50.0f, 0.0f));
		ImGui::SameLine();
		ImGui::RadioButton("None##Presets", &v_presets, 0); ImGui::SameLine();
		ImGui::RadioButton("1##Presets", &v_presets, 1); ImGui::SameLine();
		ImGui::RadioButton("2##Presets", &v_presets, 2); ImGui::SameLine();
		ImGui::RadioButton("3##Presets", &v_presets, 3);
		ImGui::SameLine();
		ImGui::Dummy(ImVec2(50.0f, 0.0f));
		ImGui::SameLine();
		
		if (v_presets == 0)
		{
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f); // Reduce button opacity
			ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true); // Disable button (and make it unclickable)
		}
		if (ImGui::Button("Save##Presets"))
		{
			this->SaveState(v_presets);
		}
		if (v_presets == 0)
		{
			ImGui::PopItemFlag();
			ImGui::PopStyleVar();
		}

		 // enable to reload the shader
		if (ImGui::Button("Reload Shader"))
		{
			auto ppshader = v_ppshaders.at(1);
			v_ppshaders.at(1).build(ppshader.GetVertexPath().c_str(), ppshader.GetFragmentPath().c_str());
		}
		
		// Enable to choose the shader
		if (ImGui::Button("Slot 1 Shader"))
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
			if (ImGui::RadioButton("None##SCANLINETYPE", &p_i_scanlineType, 0))
			{
				p_f_scanlineWeight = 0.3f;
			}
			ImGui::SameLine();
			if (ImGui::RadioButton("Simple##SCANLINETYPE", &p_i_scanlineType, 1))
			{
				p_f_scanlineWeight = 0.3f;
			}
			ImGui::SameLine();
			ImGui::RadioButton("Complex##SCANLINETYPE", &p_i_scanlineType, 2);
			ImGui::SetItemTooltip("You should generally increase the brightness (further down) when using the complex scanline type");
			if (p_i_scanlineType == 2)
			{
				ImGui::SliderFloat("Scanline Weight", &p_f_scanlineWeight, 0.03f, 0.7f, "%.2f");
				ImGui::Checkbox("Scanline Vignette", &p_b_vignette);
				ImGui::SetItemTooltip("Darker sides of the scanlines, works better when there's distortion");
				if (p_b_vignette)
					ImGui::SliderFloat("Vignette Weight", &p_f_vignetteWeight, 0.1, 5.0, "%.2f");
				ImGui::Checkbox("Interlacing", &p_b_interlace);
				ImGui::SetItemTooltip("If you really want to feel the pain of bad refresh rates, disable VSYNC and set a low FPS limit like 30 Hz");
			}
			
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
			ImGui::SliderFloat("Corners Cut", &p_f_corner, 0.f, 90.f, "%.3f");
			ImGui::Separator();
			
			// Color Settings
			ImGui::Text("[ COLOR SETTINGS ]");
			ImGui::SliderFloat("Scan/Mask Brightness Dependence", &p_f_brDep, 0.0f, 0.5f, "%.3f");
			ImGui::SliderInt("Color Space: sRGB,PAL,NTSC-U,NTSC-J", &p_i_cSpace, 0, 3, "%1d");
			ImGui::SliderFloat("Saturation", &p_f_saturation, 0.0f, 2.0f, "%.2f");
			ImGui::SliderFloat("Brightness", &p_f_brightness, 0.0f, 4.0f, "%.2f");
			ImGui::SliderFloat("Black Level", &p_f_black, -0.20f, 0.20f, "%.2f");
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
