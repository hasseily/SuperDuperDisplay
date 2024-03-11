#include "PostProcessor.h"
#include "OpenGLHelper.h"
#include "imgui.h"
#include "imgui_internal.h"		// for PushItemFlag
#include "extras/ImGuiFileDialog.h"
// For save/restore of presets
#include "nlohmann/json.hpp"
#include <fstream>
#include <sstream>

// below because "The declaration of a static data member in its class definition is not a definition"
PostProcessor* PostProcessor::s_instance;

static OpenGLHelper* oglHelper;

// Buffers and vertices for a fullscreen quad
static GLuint quadVAO = UINT_MAX;
static GLuint quadVBO = UINT_MAX;
static GLfloat quadVertices[] = {
	// Positions	// Texture Coords
	-1.f, 1.0f,  	0.0f, 1.0f,
	1.0f, -1.f,  	1.0f, 0.0f,
	1.0f, 1.0f,  	1.0f, 1.0f,
	
	-1.f, 1.0f,  	0.0f, 1.0f,
	-1.f, -1.f,  	0.0f, 0.0f,
	1.0f, -1.f,  	1.0f, 0.0f
};

static int frame_count = 0;	// Frame count for interlacing
static int v_presets = 0;	// Preset chosen

// Shader parameter variables
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
int p_scanline_type = 0;
float p_slotw = 3.0f;
float p_warpx = 0.0f;
float p_warpy = 0.0f;
float p_zoomx = 0.0f;
float p_zoomy = 0.0f;


//////////////////////////////////////////////////////////////////////////
// Basic singleton methods
//////////////////////////////////////////////////////////////////////////

void PostProcessor::Initialize()
{
	v_ppshaders.at(0).build("shaders/a2video_postprocess.glsl", "shaders/a2video_postprocess.glsl");
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

void PostProcessor::SaveState(int profile_id) {
	nlohmann::json jsonState = {
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
		{"p_zoomx", p_zoomx},
		{"p_zoomy", p_zoomy}
	};
	
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
		
		p_bzl = jsonState["p_bzl"];
		p_corner = jsonState["p_corner"];
		p_ext_gamma = jsonState["p_ext_gamma"];
		p_interlace = jsonState["p_interlace"];
		p_potato = jsonState["p_potato"];
		p_slot = jsonState["p_slot"];
		p_vig = jsonState["p_vig"];
		p_bgr = jsonState["p_bgr"];
		p_black = jsonState["p_black"];
		p_br_dep = jsonState["p_br_dep"];
		p_brightness = jsonState["p_brightness"];
		p_c_space = jsonState["p_c_space"];
		p_c_str = jsonState["p_c_str"];
		p_centerx = jsonState["p_centerx"];
		p_centery = jsonState["p_centery"];
		p_conv_b = jsonState["p_conv_b"];
		p_conv_g = jsonState["p_conv_g"];
		p_conv_r = jsonState["p_conv_r"];
		p_gb = jsonState["p_gb"];
		p_m_type = jsonState["p_m_type"];
		p_maskh = jsonState["p_maskh"];
		p_maskl = jsonState["p_maskl"];
		p_msize = jsonState["p_msize"];
		p_rb = jsonState["p_rb"];
		p_rg = jsonState["p_rg"];
		p_saturation = jsonState["p_saturation"];
		p_scanline_weight = jsonState["p_scanline_weight"];
		p_scanline_type = jsonState["p_scanline_type"];
		p_slotw = jsonState["p_slotw"];
		p_warpx = jsonState["p_warpx"];
		p_warpy = jsonState["p_warpy"];
		p_zoomx = jsonState["p_zoomx"];
		p_zoomy = jsonState["p_zoomy"];
	}
}


void PostProcessor::Render()
{
	/*
		libretro shaders have the following uniforms that have to be set:
		BOTH
		uniform vec2 TextureSize;	// Size of the incoming framebuffer

		VECTOR
		uniform mat4 MVPMatrix;		// Transform matrix
		uniform vec2 InputSize;		// Same as TextureSize (??)
		uniform vec2 OutputSize;	// Outgoing framebuffer screen size

		FRAGMENT
		uniform sampler2D Texture;	// Incoming framebuffer
	*
	*/
	if (!enabled)
		return;
	if (oglHelper == nullptr)
		oglHelper = OpenGLHelper::GetInstance();
	GLenum glerr;
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL error PP 0: " << glerr << std::endl;
	}
	v_ppshaders.at(0).use();
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL error PP 1: " << glerr << std::endl;
	}
	uint32_t w,h;
	oglHelper->get_framebuffer_size(&w, &h);
	auto shaderProgram = v_ppshaders.at(0);
	shaderProgram.setInt("BezelTexture", _SDHR_START_TEXTURES + 7 - GL_TEXTURE0);
	shaderProgram.setInt("FrameCount", frame_count);
	shaderProgram.setVec2("InputSize", glm::vec2(w, h));
	shaderProgram.setVec2("TextureSize", glm::vec2(w, h));
	shaderProgram.setVec2("OutputSize", glm::vec2(w, h));
	shaderProgram.setMat4("MVPMatrix", glm::mat4(1));
	
	// Update uniforms
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
	shaderProgram.setFloat("corner", p_corner ? 1.0f : 0.0f);
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


	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL error PP 2: " << glerr << std::endl;
	}

	// Setup fullscreen quad VAO and VBO
	if (quadVAO == UINT_MAX)
	{
		glGenVertexArrays(1, &quadVAO);
		glGenBuffers(1, &quadVBO);
		glBindVertexArray(quadVAO);
		glBindBuffer(GL_ARRAY_BUFFER, quadVBO);

		glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);

			// Position attribute
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (GLvoid*)0);

			// Texture coordinate attribute
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (GLvoid*)(2 * sizeof(GLfloat)));

			// Unbind the VAO
		glBindVertexArray(0);
	}

	// Render the fullscreen quad
	glBindVertexArray(quadVAO);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindVertexArray(0);
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL error PP 3: " << glerr << std::endl;
	}
	
	++frame_count;
}

void PostProcessor::DisplayImGuiPPWindow(bool* p_open)
{
	if (p_open)
	{
		ImGui::Begin("Post Processing CRT Shader", p_open);
		ImGui::Checkbox("Post Processing Enabled", &enabled);
		
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
			v_ppshaders.at(0).build("shaders/a2video_postprocess.glsl", "shaders/a2video_postprocess.glsl");
		}
		
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
		ImGui::Separator();
		
		// Scanline and Interlacing
		ImGui::Text("[ SCANLINE SETTINGS ]");
		ImGui::RadioButton("None##Scanline", &p_scanline_type, 0); ImGui::SameLine();
		ImGui::RadioButton("Simple##Scanline", &p_scanline_type, 1); ImGui::SameLine();
		ImGui::RadioButton("CRT##Scanline", &p_scanline_type, 2);
		if (p_scanline_type == 2)
		{
			ImGui::SliderFloat("Scanline Weight", &p_scanline_weight, 0.001f, 0.5f, "%.2f");
			ImGui::Checkbox("Interlacing On/Off", &p_interlace);

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
			ImGui::Checkbox("Bezel On/Off", &p_bzl);
			ImGui::SliderFloat("Zoom Image X", &p_zoomx, -1.0f, 1.0f, "%.3f");
			ImGui::SliderFloat("Zoom Image Y", &p_zoomy, -1.0f, 1.0f, "%.3f");
			ImGui::SliderFloat("Image Center X", &p_centerx, -3.0f, 3.0f, "%.2f");
			ImGui::SliderFloat("Image Center Y", &p_centery, -3.0f, 3.0f, "%.2f");
			ImGui::SliderFloat("Curvature Horizontal", &p_warpx, 0.00f, 0.25f, "%.2f");
			ImGui::SliderFloat("Curvature Vertical", &p_warpy, 0.00f, 0.25f, "%.2f");
			ImGui::Checkbox("Corners Cut", &p_corner);
			ImGui::Checkbox("Vignette On/Off", &p_vig);
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
		}	// p_scanline_type == 2

		ImGui::PopItemWidth();
		ImGui::End();
	}
}

