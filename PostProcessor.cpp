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
		{"p_postprocessing_level", p_postprocessing_level},
		{"bCRTFillWindow", bCRTFillWindow},
		{"integer_scale", integer_scale},
		{"bAutoScale", bAutoScale},
		{"bCRTFillWindow", bCRTFillWindow},
		{"p_bzl", p_bzl},
		{"p_corner", p_corner},
		{"p_ext_gamma", p_ext_gamma},
		{"p_interlace", p_interlace},
		{"p_potato", p_potato},
		{"p_slot", p_slot},
		{"p_vig", p_vig},
		{"p_bgr", p_bgr},
		{"p_black", p_black},
		{"p_br_dep", p_br_dep},
		{"p_brightness", p_brightness},
		{"p_c_space", p_c_space},
		{"p_c_str", p_c_str},
		{"p_centerx", p_centerx},
		{"p_centery", p_centery},
		{"p_conv_b", p_conv_b},
		{"p_conv_g", p_conv_g},
		{"p_conv_r", p_conv_r},
		{"p_gb", p_gb},
		{"p_m_type", p_m_type},
		{"p_maskh", p_maskh},
		{"p_maskl", p_maskl},
		{"p_msize", p_msize},
		{"p_rb", p_rb},
		{"p_rg", p_rg},
		{"p_saturation", p_saturation},
		{"p_scanline_weight", p_scanline_weight},
		{"p_scanline_type", p_scanline_type},
		{"p_slotw", p_slotw},
		{"p_warpx", p_warpx},
		{"p_warpy", p_warpy},
		{"p_barrel_distortion", p_barrel_distortion},
		{"p_zoomx", p_zoomx},
		{"p_zoomy", p_zoomy}
	};
	return jsonState;
}

void PostProcessor::DeserializeState(const nlohmann::json &jsonState)
{
	p_postprocessing_level = jsonState.value("p_postprocessing_level", p_postprocessing_level);
	bCRTFillWindow = jsonState.value("bCRTFillWindow", bCRTFillWindow);
	integer_scale = jsonState.value("integer_scale", integer_scale);
	bAutoScale = jsonState.value("bAutoScale", bAutoScale);
	p_bzl = jsonState.value("p_bzl", p_bzl);
	p_corner = jsonState.value("p_corner", p_corner);
	p_ext_gamma = jsonState.value("p_ext_gamma", p_ext_gamma);
	p_interlace = jsonState.value("p_interlace", p_interlace);
	p_potato = jsonState.value("p_potato", p_potato);
	p_slot = jsonState.value("p_slot", p_slot);
	p_vig = jsonState.value("p_vig", p_vig);
	p_bgr = jsonState.value("p_bgr", p_bgr);
	p_black = jsonState.value("p_black", p_black);
	p_br_dep = jsonState.value("p_br_dep", p_br_dep);
	p_brightness = jsonState.value("p_brightness", p_brightness);
	p_c_space = jsonState.value("p_c_space", p_c_space);
	p_c_str = jsonState.value("p_c_str", p_c_str);
	p_centerx = jsonState.value("p_centerx", p_centerx);
	p_centery = jsonState.value("p_centery", p_centery);
	p_conv_b = jsonState.value("p_conv_b", p_conv_b);
	p_conv_g = jsonState.value("p_conv_g", p_conv_g);
	p_conv_r = jsonState.value("p_conv_r", p_conv_r);
	p_gb = jsonState.value("p_gb", p_gb);
	p_m_type = jsonState.value("p_m_type", p_m_type);
	p_maskh = jsonState.value("p_maskh", p_maskh);
	p_maskl = jsonState.value("p_maskl", p_maskl);
	p_msize = jsonState.value("p_msize", p_msize);
	p_rb = jsonState.value("p_rb", p_rb);
	p_rg = jsonState.value("p_rg", p_rg);
	p_saturation = jsonState.value("p_saturation", p_saturation);
	p_scanline_weight = jsonState.value("p_scanline_weight", p_scanline_weight);
	p_scanline_type = jsonState.value("p_scanline_type", p_scanline_type);
	p_slotw = jsonState.value("p_slotw", p_slotw);
	p_warpx = jsonState.value("p_warpx", p_warpx);
	p_warpy = jsonState.value("p_warpy", p_warpy);
	p_barrel_distortion = jsonState.value("p_barrel_distortion", p_barrel_distortion);
	p_zoomx = jsonState.value("p_zoomx", p_zoomx);
	p_zoomy = jsonState.value("p_zoomy", p_zoomy);
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
	if (p_postprocessing_level == 0) {		// basic passthrough shader
		shaderProgram = v_ppshaders.at(0);
		shaderProgram.use();
		shaderProgram.setInt("Texture", _PP_INPUT_TEXTURE_UNIT - GL_TEXTURE0);
	}
	else {
		shaderProgram = v_ppshaders.at(1);
		shaderProgram.use();
		// Update uniforms
		shaderProgram.setInt("A2Texture", _PP_INPUT_TEXTURE_UNIT - GL_TEXTURE0);
		shaderProgram.setInt("FrameCount", frame_count);
		shaderProgram.setVec2("ViewportSize", glm::vec2(viewportWidth, viewportHeight));
		shaderProgram.setVec2("InputSize", glm::vec2(texWidth, texHeight));
		shaderProgram.setVec2("TextureSize", glm::vec2(texWidth, texHeight));
		shaderProgram.setVec2("OutputSize", glm::vec2(quadWidth, quadHeight));
		shaderProgram.setVec4("VideoRect", quadViewportCoords);

		shaderProgram.setFloat("POSTPROCESSING_LEVEL", (float)p_postprocessing_level);
		shaderProgram.setFloat("SCANLINE_TYPE", (float)p_scanline_type);
		shaderProgram.setFloat("SCANLINE_WEIGHT", p_scanline_weight);
		shaderProgram.setFloat("INTERLACE", p_interlace ? 1.0f : 0.0f);
		shaderProgram.setFloat("M_TYPE", (float)p_m_type);
		shaderProgram.setFloat("MSIZE", p_msize);
		shaderProgram.setFloat("SLOT", p_slot ? 1.0f : 0.0f);
		shaderProgram.setFloat("SLOTW", p_slotw);
		shaderProgram.setFloat("BGR", p_bgr);
		shaderProgram.setFloat("Maskl", p_maskl);
		shaderProgram.setFloat("Maskh", p_maskh);
		shaderProgram.setFloat("bzl", p_bzl ? 1.0f : 0.0f);
		shaderProgram.setFloat("zoomx", p_zoomx);
		shaderProgram.setFloat("zoomy", p_zoomy);
		shaderProgram.setFloat("centerx", p_centerx);
		shaderProgram.setFloat("centery", p_centery);
		shaderProgram.setFloat("WARPX", p_warpx);
		shaderProgram.setFloat("WARPY", p_warpy);
		shaderProgram.setFloat("BARRELDISTORTION", p_barrel_distortion);
		shaderProgram.setFloat("corner", p_corner / 10000);
		shaderProgram.setFloat("vig", p_vig ? 1.0f : 0.0f);
		shaderProgram.setFloat("BR_DEP", p_br_dep);
		shaderProgram.setFloat("c_space", (float)p_c_space);
		shaderProgram.setFloat("EXT_GAMMA", p_ext_gamma ? 1.0f : 0.0f);
		shaderProgram.setFloat("SATURATION", p_saturation);
		shaderProgram.setFloat("BRIGHTNESs", p_brightness);
		shaderProgram.setFloat("BLACK", p_black);
		shaderProgram.setFloat("RG", p_rg);
		shaderProgram.setFloat("RB", p_rb);
		shaderProgram.setFloat("GB", p_gb);
		shaderProgram.setFloat("C_STR", p_c_str);
		shaderProgram.setFloat("CONV_R", p_conv_r);
		shaderProgram.setFloat("CONV_G", p_conv_g);
		shaderProgram.setFloat("CONV_B", p_conv_b);
		shaderProgram.setFloat("POTATO", p_potato ? 1.0f : 0.0f);
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

	if (bCRTFillWindow && p_postprocessing_level > 1)
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
	}
	else
	{
		shaderProgram.use();
	}

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

		
		/*
		 // enable to reload the shader
		if (ImGui::Button("Reload Shader"))
		{
			v_ppshaders.at(0).build("shaders/a2video_postprocess.glsl", "shaders/a2video_postprocess.glsl");
		}
		*/
		
		/*	// Enable to choose the shader
		if (ImGui::Button("Slot 1 Shader"))
		{
			IGFD::FileDialogConfig config;
			config.path = "./shaders/";
			ImGuiFileDialog::Instance()->OpenDialog("ChooseShader1DlgKey", "Choose File", ".glsl,", config);
		}

		// Display the file dialog
		if (ImGuiFileDialog::Instance()->Display("ChooseShader1DlgKey")) {
			// Check if a file was selected
			if (ImGuiFileDialog::Instance()->IsOk()) {
				v_ppshaders.at(0).build(
					ImGuiFileDialog::Instance()->GetFilePathName().c_str(), 
					ImGuiFileDialog::Instance()->GetFilePathName().c_str()
				);
			}
			ImGuiFileDialog::Instance()->Close();
		}
		*/
		ImGui::PushItemWidth(200);

		// PP Type
		ImGui::Separator();
		ImGui::Text("[ POSTPROCESSING LEVEL ]");
		ImGui::RadioButton("None##PPLEVEL", &p_postprocessing_level, 0); ImGui::SameLine();
		ImGui::RadioButton("Scanline only##PPLEVEL", &p_postprocessing_level, 1); ImGui::SameLine();
		ImGui::RadioButton("Full CRT##PPLEVEL", &p_postprocessing_level, 2);
		ImGui::Separator();
		ImGui::Text("[ BASE INTEGER SCALE ]");
		ImGui::Checkbox("Auto", &bAutoScale);
		if (bAutoScale)
			ImGui::BeginDisabled();
		ImGui::SliderInt("Integer Scale", &integer_scale, 1, max_integer_scale, "%d");
		if (bAutoScale)
			ImGui::EndDisabled();

		if (p_postprocessing_level > 1) {
			ImGui::Separator();
			// Scanline and Interlacing
			ImGui::Text("[ SCANLINE TYPE ]");
			ImGui::RadioButton("None##SCANLINETYPE", &p_scanline_type, 0); ImGui::SameLine();
			ImGui::RadioButton("Simple##SCANLINETYPE", &p_scanline_type, 1); ImGui::SameLine();
			ImGui::RadioButton("Complex##SCANLINETYPE", &p_scanline_type, 2);
			if (p_scanline_type == 2) {
				ImGui::SliderFloat("Scanline Weight", &p_scanline_weight, 0.001f, 0.5f, "%.2f");
				ImGui::Checkbox("Vignette", &p_vig);
				ImGui::Checkbox("Interlacing On/Off", &p_interlace);
			}
			
			ImGui::Separator();
			
			// Mask Settings
			ImGui::Text("[ MASK SETTINGS ]");
			ImGui::RadioButton("None##Mask", &p_m_type, 0); ImGui::SameLine();
			ImGui::RadioButton("CGWG##Mask", &p_m_type, 1); ImGui::SameLine();
			ImGui::RadioButton("RGB##Mask", &p_m_type, 2);
			ImGui::SliderFloat("Mask Size", &p_msize, 1.0f, 2.0f, "%.1f");
			ImGui::Checkbox("Slot Mask On/Off", &p_slot);
			ImGui::SliderFloat("Slot Mask Width", &p_slotw, 2.0f, 3.0f, "%.1f");
			ImGui::SliderFloat("Subpixels BGR/RGB", &p_bgr, 0.0f, 1.0f, "%.1f");
			ImGui::SliderFloat("Mask Brightness Dark", &p_maskl, 0.0f, 1.0f, "%.2f");
			ImGui::SliderFloat("Mask Brightness Bright", &p_maskh, 0.0f, 1.0f, "%.2f");
			ImGui::Separator();
			
			// Geometry Settings
			ImGui::Text("[ GEOMETRY SETTINGS ]");
			if (ImGui::Checkbox("Fill Window", &bCRTFillWindow))
			{
				p_zoomx = 0;
				p_zoomy = 0;
				p_centerx = 0;
				p_centery = 0;

			}
			ImGui::SliderFloat("Zoom Image X", &p_zoomx, -2.0f, 2.0f, "%.3f");
			ImGui::SliderFloat("Zoom Image Y", &p_zoomy, -2.0f, 2.0f, "%.3f");
			ImGui::SliderFloat("Image Center X", &p_centerx, -100.0f, 100.0f, "%.2f");
			ImGui::SliderFloat("Image Center Y", &p_centery, -100.0f, 100.0f, "%.2f");
			ImGui::SliderFloat("Curvature Horizontal", &p_warpx, 0.00f, 0.25f, "%.2f");
			ImGui::SliderFloat("Curvature Vertical", &p_warpy, 0.00f, 0.25f, "%.2f");
			ImGui::SliderFloat("Barrel Distortion", &p_barrel_distortion, -2.00f, 2.00f, "%.2f");
			ImGui::SliderFloat("Corners Cut", &p_corner, 0.f, 90.f, "%.3f");
			ImGui::Separator();
			
			// Color Settings
			ImGui::Text("[ COLOR SETTINGS ]");
			ImGui::SliderFloat("Scan/Mask Brightness Dependence", &p_br_dep, 0.0f, 0.5f, "%.3f");
			ImGui::SliderInt("Color Space: sRGB,PAL,NTSC-U,NTSC-J", &p_c_space, 0, 3, "%1d");
			ImGui::SliderFloat("Saturation", &p_saturation, 0.0f, 2.0f, "%.2f");
			ImGui::SliderFloat("Brightness", &p_brightness, 0.0f, 4.0f, "%.2f");
			ImGui::SliderFloat("Black Level", &p_black, -0.20f, 0.20f, "%.2f");
			ImGui::SliderFloat("Green <-to-> Red Hue", &p_rg, -0.25f, 0.25f, "%.2f");
			ImGui::SliderFloat("Blue <-to-> Red Hue", &p_rb, -0.25f, 0.25f, "%.2f");
			ImGui::SliderFloat("Blue <-to-> Green Hue", &p_gb, -0.25f, 0.25f, "%.2f");
			ImGui::Checkbox("External Gamma In (Glow etc)", &p_ext_gamma);
			ImGui::Separator();
			
			// Convergence Settings
			ImGui::Text("[ CONVERGENCE SETTINGS ]");
			ImGui::SliderFloat("Convergence Overall Strength", &p_c_str, 0.0f, 0.5f, "%.2f");
			ImGui::SliderFloat("Convergence Red X-Axis", &p_conv_r, -3.0f, 3.0f, "%.2f");
			ImGui::SliderFloat("Convergence Green X-axis", &p_conv_g, -3.0f, 3.0f, "%.2f");
			ImGui::SliderFloat("Convergence Blue X-Axis", &p_conv_b, -3.0f, 3.0f, "%.2f");
			ImGui::Checkbox("Potato Boost(Simple Gamma, adjust Mask)", &p_potato);
		}

		ImGui::PopItemWidth();
		ImGui::End();
	}
}
