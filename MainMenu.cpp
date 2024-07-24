#include "MainMenu.h"
#include "common.h"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include "imgui_memory_editor.h"

#include "OpenGLHelper.h"
#include "MemoryManager.h"
#include "A2VideoManager.h"
#include "CycleCounter.h"
#include "SoundManager.h"
#include "MockingboardManager.h"
#include "PostProcessor.h"
#include "EventRecorder.h"
#include "extras/MemoryLoader.h"
#include "extras/ImGuiFileDialog.h"

#include <iostream>

// In main.cpp
extern void Main_ResetFPSCalculations();
extern bool Main_IsFullScreen();
extern void Main_SetFullScreen(bool bIsFullscreen);
extern int Main_GetVsync();
extern void Main_SetVsync(bool _on);
extern void Main_DisplaySplashScreen();
extern void Main_GetBGColor(float outColor[4]);
extern void Main_SetBGColor(const float newColor[4]);

class MainMenu::Gui {
public:
	ImFont* fontDefault;
	ImFont* fontMedium;
	ImFont* fontLarge;

	int iWindowWidth=1200;
	int iWindowHeight=1000;
	bool show_about_window = false;
	
	Gui() {}
};

MainMenu::MainMenu(SDL_GLContext gl_context, SDL_Window* window)
: gl_context_(gl_context), window_(window), pGui(new Gui()) {
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
	
	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
	// ImGui::StyleColorsLight();
	
	// Setup Platform/Renderer backends
	ImGui_ImplSDL2_InitForOpenGL(window_, gl_context_);
	ImGui_ImplOpenGL3_Init(OpenGLHelper::GetInstance()->get_glsl_version()->c_str());

	// Add the fonts
	pGui->fontDefault = io.Fonts->AddFontDefault();
	pGui->fontMedium = io.Fonts->AddFontFromFileTTF("./assets/ProggyTiny.ttf", 14.0f);
	pGui->fontLarge = io.Fonts->AddFontFromFileTTF("./assets/BerkeliumIIHGR.ttf", 16.f);
	
	// Never draw the cursor. Let SDL draw the cursor. In windowed mode, the cursor
	// may always be drawn anyway, and having ImGUI draw it will duplicate it.
	// main.cpp will handle hiding the cursor after some inactivity in fullscreen mode.
	io.MouseDrawCursor = false;
}

MainMenu::~MainMenu() {
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();
}

void MainMenu::Render() {
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL2_NewFrame(window_);
	ImGui::NewFrame();
	
	ImFont* _menuFont;
	ImFont* _itemFont;
	int screen_width, screen_height;
	SDL_GetWindowSize(window_, &screen_width, &screen_height);
	if (screen_width < 1200)
	{
		_menuFont = pGui->fontDefault;
		_itemFont = pGui->fontDefault;
	} else {
		_menuFont = pGui->fontLarge;
		_itemFont = pGui->fontMedium;
	}
	
	if (ImGui::BeginMainMenuBar()) {
		ImGui::PushFont(_menuFont);
		if (ImGui::BeginMenu("SDD")) {
			ImGui::PushFont(_itemFont);
			ShowSDDMenu();
			ImGui::PopFont();
			ImGui::EndMenu();
		}
		ImGui::Spacing();
		if (ImGui::BeginMenu("Motherboard")) {
			ImGui::PushFont(_itemFont);
			ShowMotherboardMenu();
			ImGui::PopFont();
			ImGui::EndMenu();
		}
		ImGui::Spacing();
		if (ImGui::BeginMenu("Video")) {
			ImGui::PushFont(_itemFont);
			ShowVideoMenu();
			ImGui::PopFont();
			ImGui::EndMenu();
		}
		ImGui::Spacing();
		if (ImGui::BeginMenu("Sound")) {
			ImGui::PushFont(_itemFont);
			ShowSoundMenu();
			ImGui::PopFont();
			ImGui::EndMenu();
		}
		ImGui::Spacing();
		if (ImGui::BeginMenu("Samples")) {
			ImGui::PushFont(_itemFont);
			ShowSamplesMenu();
			ImGui::PopFont();
			ImGui::EndMenu();
		}
		ImGui::Spacing();
		if (ImGui::BeginMenu("Developer")) {
			ImGui::PushFont(_itemFont);
			ShowDeveloperMenu();
			ImGui::PopFont();
			ImGui::EndMenu();
		}
		ImGui::PopFont();
		
		ImGui::Text("     ");
		ImGui::PushFont(pGui->fontDefault);
		ImGuiIO& io = ImGui::GetIO(); (void)io;
		ImGui::Text("Avg %.3f ms/f (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
		ImGui::PopFont();
		
		ImGui::EndMainMenuBar();
		
		if (pGui->show_about_window) {
			ImGui::PushFont(_menuFont);
			ImGui::Begin("About", &pGui->show_about_window, ImGuiWindowFlags_AlwaysAutoResize);
			ImGui::Text("Super Duper Display");
			ImGui::Separator();
			ImGui::Text("Version: 0.5.0");
			ImGui::Text("Author: Rikkles, Elltwo");  // Author name
			ImGui::Separator();
			
			ImGui::TextWrapped("SuperDuperDisplay is a hybrid emulation frontend for Appletini, the Apple 2 Bus Card.");
			if (ImGui::Button("OK")) {
				pGui->show_about_window = false;  // Close the "About" window when the OK button is clicked
			}
			ImGui::End();
			ImGui::PopFont();
		}
	}
	
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

bool MainMenu::HandleEvent(SDL_Event& event) {
	bool eventIsHandledInImGui = false;
	ImGui_ImplSDL2_ProcessEvent(&event);
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	
	switch (event.type) {
		case SDL_MOUSEWHEEL:
			if (io.WantCaptureMouse) {
				eventIsHandledInImGui = true;
				// TODO: Handle mouse
			}
			break;
		case SDL_KEYDOWN:
		{
			if (io.WantCaptureKeyboard) {
				eventIsHandledInImGui = true;
				switch (event.key.keysym.sym)
				{
					// TODO: Handle keyboard
					default:
						break;
				};
			}
		}
			break;
		default:
			break;
	}   // switch event.type

	return eventIsHandledInImGui;
}

void MainMenu::ShowSDDMenu() {
#ifndef __APPLE__
	if (ImGui::MenuItem("Fullscreen", "Alt+Enter")) {
		HandleFullscreen();
	}
#endif
	if (ImGui::BeginMenu("Fullscreen Resolution")) {
		ImGui::InputInt("Width", &pGui->iWindowWidth, 10, 100);
		ImGui::InputInt("Height", &pGui->iWindowHeight, 10, 100);
		if (ImGui::Button("Apply"))
		{
			SDL_SetWindowSize(window_, pGui->iWindowWidth, pGui->iWindowHeight);
		}
		ImGui::EndMenu();
	}
	int iMMVsync = Main_GetVsync();
	bool bMMVsync = (iMMVsync == 0 ? 0 : 1);
	if (ImGui::Checkbox("VSYNC", &bMMVsync)) {
		Main_SetVsync(bMMVsync);
		iMMVsync = Main_GetVsync();
	}
	if (iMMVsync == -1) {
		ImGui::SameLine(); ImGui::Text(" (Adaptive)");
	}
	if (ImGui::BeginMenu("Background Color")) {
		float windowBGColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f }; // RGBA
		Main_GetBGColor(windowBGColor);
		if (ImGui::ColorEdit4("", windowBGColor)) {
			Main_SetBGColor(windowBGColor);
		}
		ImGui::EndMenu();
	}
	ImGui::Separator();
	if (ImGui::MenuItem("Reset SDD")) {
		A2VideoManager::GetInstance()->ResetComputer();
		Main_DisplaySplashScreen();
	}
	ImGui::Separator();
	if (ImGui::MenuItem("About")) {
		pGui->show_about_window = true;
	}
	ImGui::Separator();
	if (ImGui::MenuItem("Quit", "Ctrl+C")) {
		HandleQuit();
	}
}

void MainMenu::ShowMotherboardMenu() {
	if (ImGui::BeginMenu("Region")) {
		ImGui::MenuItem("Auto");
		ImGui::MenuItem("PAL");
		ImGui::MenuItem("NTSC");
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("Character ROMs")) {
		if (ImGui::BeginMenu("Regular")) {
			ImGui::MenuItem("00_US-Regular.png");
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Alternate")) {
			ImGui::MenuItem("01_US-Alternate.png");
			ImGui::EndMenu();
		}
		ImGui::EndMenu();
	}
	ImGui::MenuItem("Apple //e Memory");
}

void MainMenu::ShowVideoMenu() {
	ImGui::MenuItem("Borders");
	ImGui::MenuItem("Post Processing");
	ImGui::Separator();
	if (ImGui::BeginMenu("Extra Modes")) {
		ImGui::MenuItem("DHGR COL140Mixed");
		ImGui::MenuItem("HGR SPEC1");
		ImGui::MenuItem("HGR SPEC2");
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("SHR+Legacy")) {
		ImGui::MenuItem("Force SHR Width");
		ImGui::MenuItem("No Wobble");
		ImGui::EndMenu();
	}
}

void MainMenu::ShowSoundMenu() {
	if (ImGui::BeginMenu("HDMI Speaker")) {
		SoundManager::GetInstance()->DisplayImGuiChunk();
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("Mockingboard")) {
		MockingboardManager::GetInstance()->DisplayImGuiChunk();
		ImGui::EndMenu();
	}
}

void MainMenu::ShowSamplesMenu() {
	ImGui::MenuItem("Run Karateka Demo");
	ImGui::MenuItem("DHGR Col140Mixed");
	ImGui::MenuItem("HGR SPEC1");
	ImGui::MenuItem("SHR+Legacy");
}

void MainMenu::ShowDeveloperMenu() {
	ImGui::MenuItem("Event Recorder");
	ImGui::MenuItem("FPS Overlay");
	ImGui::MenuItem("Load File Into Memory");
	if (ImGui::BeginMenu("Soft Switches")) {
		ImGui::MenuItem("SS Window");
		ImGui::MenuItem("Reset");
		ImGui::EndMenu();
	}
	ImGui::MenuItem("Run Vertical Refresh");
	ImGui::Separator();
	if (ImGui::BeginMenu("VRAM Windows")) {
		ImGui::MenuItem("Legacy");
		ImGui::MenuItem("SHR");
		ImGui::MenuItem("Offset Buffer");
		ImGui::EndMenu();
	}
	ImGui::MenuItem("SDD Textures Window");
	ImGui::Separator();
	if (ImGui::BeginMenu("SDHR")) {
		ImGui::MenuItem("Untextured Geometry");
		ImGui::MenuItem("Perspective Projection");
		ImGui::MenuItem("Upload Region Memory Window");
		ImGui::EndMenu();
	}
}

void MainMenu::HandleFullscreen() {
	Main_SetFullScreen(!Main_IsFullScreen());
}

void MainMenu::HandleQuit() {
	std::cout << "Quitting application" << std::endl;
	SDL_Quit();
	exit(0);
}

