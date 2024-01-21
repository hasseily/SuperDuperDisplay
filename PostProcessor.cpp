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
	v_ppshaders.at(0).setInt("FrameCount", 2);
	v_ppshaders.at(0).setVec2("InputSize", glm::vec2(w, h));
	v_ppshaders.at(0).setVec2("TextureSize", glm::vec2(w, h));
	v_ppshaders.at(0).setVec2("OutputSize", glm::vec2(w, h));
	v_ppshaders.at(0).setMat4("MVPMatrix", oglHelper->mat_proj);
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL error 2: " << glerr << std::endl;
	}

	// Setup fullscreen quad VAO and VBO if not already done
	GLuint quadVAO, quadVBO;
	glGenVertexArrays(1, &quadVAO);
	glGenBuffers(1, &quadVBO);
	glBindVertexArray(quadVAO);
	glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL error 3: " << glerr << std::endl;
	}

	float wn = -1.f * (w / 2);
	float wp =  1.f * (w / 2);
	float hn = -1.f * (h / 2);
	float hp =  1.f * (h / 2);
	GLfloat quadVertices[] = {
		// Positions        // Texture Coords
		wn, hp, 0.0f,  0.0f, 1.0f,
		wn, hn, 0.0f,  0.0f, 0.0f,
		wp, hn, 0.0f,  1.0f, 0.0f,

		wn, hp, 0.0f,  0.0f, 1.0f,
		wp, hn, 0.0f,  1.0f, 0.0f,
		wp, hp, 0.0f,  1.0f, 1.0f
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
	// Unbind the VAO (it's always a good practice to unbind any buffer/array to prevent strange bugs)
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
	// Cleanup (do this when you're completely done with the VAO and VBO)
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

