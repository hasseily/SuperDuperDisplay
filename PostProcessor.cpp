#include "PostProcessor.h"
#include "OpenGLHelper.h"
#include "imgui.h"
#include "ImGuiFileDialog.h"

// below because "The declaration of a static data member in its class definition is not a definition"
PostProcessor* PostProcessor::s_instance;

static OpenGLHelper* oglHelper;

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
int p_m_type = -1;
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

}

//////////////////////////////////////////////////////////////////////////
// Main methods
//////////////////////////////////////////////////////////////////////////

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
		std::cerr << "OpenGL error 0: " << glerr << std::endl;
	}
	v_ppshaders.at(0).use();
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL error 1: " << glerr << std::endl;
	}
	uint32_t w,h;
	oglHelper->get_framebuffer_size(&w, &h);
	auto shaderProgram = v_ppshaders.at(0);
	// Slot 4 for scanline regular modes, 5 for SHR
	int _slSlot = (w == 560 ? 4 : 5);
	shaderProgram.setInt("HorizScanlineTexture", _SDHR_START_TEXTURES + _slSlot - GL_TEXTURE0);
	shaderProgram.setInt("BezelTexture", _SDHR_START_TEXTURES + 6 - GL_TEXTURE0);
	shaderProgram.setInt("FrameCount", 2);
	shaderProgram.setVec2("InputSize", glm::vec2(w, h));
	shaderProgram.setVec2("TextureSize", glm::vec2(w, h));
	shaderProgram.setVec2("OutputSize", glm::vec2(w, h));
	shaderProgram.setMat4("MVPMatrix", glm::mat4(1));
	
	// Update uniforms
	shaderProgram.setFloat("SCANLINEWEIGHT", p_scanline_weight);
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
		std::cerr << "OpenGL error 2: " << glerr << std::endl;
	}

	// Setup fullscreen quad VAO and VBO
	GLuint quadVAO, quadVBO;
	glGenVertexArrays(1, &quadVAO);
	glGenBuffers(1, &quadVBO);
	glBindVertexArray(quadVAO);
	glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL error 3: " << glerr << std::endl;
	}

	GLfloat quadVertices[] = {
		// Positions        // Texture Coords
		-1.f, 1.0f, 0.0f,  0.0f, 1.0f,
		-1.f, -1.f, 0.0f,  0.0f, 0.0f,
		1.0f, -1.f, 0.0f,  1.0f, 0.0f,

		-1.f, 1.0f, 0.0f,  0.0f, 1.0f,
		1.0f, -1.f, 0.0f,  1.0f, 0.0f,
		1.0f, 1.0f, 0.0f,  1.0f, 1.0f
	};
	glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL error 4: " << glerr << std::endl;
	}
	// Position attribute
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (GLvoid*)0);
	glEnableVertexAttribArray(0);
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL error 5: " << glerr << std::endl;
	}
	// Texture coordinate attribute
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
	glEnableVertexAttribArray(1);
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL error 6: " << glerr << std::endl;
	}
	// Unbind the VAO
	glBindVertexArray(0);
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL error 7: " << glerr << std::endl;
	}
	// Render the fullscreen quad
	glBindVertexArray(quadVAO);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindVertexArray(0);
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL error 8: " << glerr << std::endl;
	}
	// Cleanup
	glDeleteVertexArrays(1, &quadVAO);
	glDeleteBuffers(1, &quadVBO);
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL error 9: " << glerr << std::endl;
	}
}

void PostProcessor::DisplayImGuiPPWindow(bool* p_open)
{
	if (p_open)
	{
		ImGui::Begin("Post Processing CRT Shader", p_open);
		ImGui::Checkbox("Post Processing Enabled", &enabled);
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
		ImGui::Separator();
		

		// Scanline and Interlacing
		ImGui::RadioButton("None", &p_scanline_type, 0); ImGui::SameLine();
		ImGui::RadioButton("Simple", &p_scanline_type, 1); ImGui::SameLine();
		ImGui::RadioButton("CRT", &p_scanline_type, 2);
		ImGui::SliderFloat("Scanline Weight", &p_scanline_weight, 0.001f, 0.5f, "%.2f");
		ImGui::Checkbox("Interlacing On/Off", &p_interlace);

		// Mask Settings
		ImGui::Text("[ MASK SETTINGS ]");
		ImGui::RadioButton("None", &p_m_type, 0); ImGui::SameLine();
		ImGui::RadioButton("CGWG", &p_m_type, 1); ImGui::SameLine();
		ImGui::RadioButton("RGB", &p_m_type, 2);
		ImGui::SliderFloat("Mask Size", &p_msize, 1.0f, 2.0f, "%.1f");
		ImGui::Checkbox("Slot Mask On/Off", &p_slot);
		ImGui::SliderFloat("Slot Mask Width", &p_slotw, 2.0f, 3.0f, "%.1f");
		ImGui::SliderFloat("Subpixels BGR/RGB", &p_bgr, 0.0f, 1.0f, "%.1f");
		ImGui::SliderFloat("Mask Brightness Dark", &p_maskl, 0.0f, 1.0f, "%.2f");
		ImGui::SliderFloat("Mask Brightness Bright", &p_maskh, 0.0f, 1.0f, "%.2f");

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

		// Convergence Settings
		ImGui::Text("[ CONVERGENCE SETTINGS ]");
		ImGui::SliderFloat("Convergence Overall Strength", &p_c_str, 0.0f, 0.5f, "%.2f");
		ImGui::SliderFloat("Convergence Red X-Axis", &p_conv_r, -3.0f, 3.0f, "%.2f");
		ImGui::SliderFloat("Convergence Green X-axis", &p_conv_g, -3.0f, 3.0f, "%.2f");
		ImGui::SliderFloat("Convergence Blue X-Axis", &p_conv_b, -3.0f, 3.0f, "%.2f");
		ImGui::Checkbox("Potato Boost(Simple Gamma, adjust Mask)", &p_potato);

		ImGui::End();
	}
}

