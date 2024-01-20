#include "PostProcessor.h"
#include "OpenGLHelper.h"
#include "imgui.h"
#include "ImGuiFileDialog.h"

// below because "The declaration of a static data member in its class definition is not a definition"
PostProcessor* PostProcessor::s_instance;

static OpenGLHelper* oglHelper;

//////////////////////////////////////////////////////////////////////////
// Basic singleton methods
//////////////////////////////////////////////////////////////////////////

void PostProcessor::Initialize()
{

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
	if (oglHelper == nullptr)
		oglHelper = OpenGLHelper::GetInstance();

}

void PostProcessor::DisplayImGuiPPWindow(bool* p_open)
{
	if (p_open)
	{
		ImGui::Begin("PostProcessing", p_open);
		ImGui::Checkbox("PostProcessing enabled", &enabled);
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
				v_ppshaders.at(0).build_combined(ImGuiFileDialog::Instance()->GetFilePathName().c_str());
			}
			ImGuiFileDialog::Instance()->Close();
		}
		ImGui::End();
	}
}

