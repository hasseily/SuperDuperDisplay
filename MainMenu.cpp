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
#include "SDHRManager.h"
#include "SDHRNetworking.h"
#include "extras/MemoryLoader.h"
#include "extras/ImGuiFileDialog.h"

#include <iostream>
#include <vector>

// In main.cpp
extern uint32_t Main_GetFPSLimit();
extern void Main_SetFPSLimit(uint32_t fps);
extern void Main_ResetFPSCalculations();
extern SDL_DisplayMode Main_GetFullScreenMode();
extern void Main_SetFullScreenMode(SDL_DisplayMode mode);
extern bool Main_IsLinuxConsole();
extern bool Main_IsFullScreen();
extern void Main_SetFullScreen(bool bIsFullscreen);
extern SwapInterval_e Main_GetVsync();
extern void Main_SetVsync(SwapInterval_e _vsync);
extern void Main_DisplaySplashScreen();
extern void Main_GetBGColor(float outColor[4]);
extern void Main_SetBGColor(const float newColor[4]);
extern void Main_ResetA2SS();
extern bool Main_IsFPSOverlay();
extern void Main_SetFPSOverlay(bool isFPSOverlay);
extern SDL_Window* Main_GetSDLWindow();
extern void Main_RequestAppQuit();

class MainMenu::Gui {
public:
	ImFont* fontDefault = nullptr;
	ImFont* fontMedium = nullptr;
	ImFont* fontLarge = nullptr;

	std::vector<SDL_DisplayMode> v_displayModes;
	int iCurrentDisplayIndex = -1;

	int iFPSLimiter = 0;
	int iWindowWidth=1200;
	int iWindowHeight=1000;
	bool bShowAboutWindow = false;
	int iTextureSlotIdx = 0;
	bool bShowTextureWindow = false;
	bool bShowA2VideoWindow = false;
	bool bShowPPWindow = false;
	bool bShowSSWindow = false;
	bool bShowEventRecorderWindow = false;
	bool bShowLoadFileWindow = false;
	bool bShowImGuiMetricsWindow = false;
	bool bShowMemoryHeatMap = false;
	
	bool bSampleRunKarateka = false;

	MemoryEditor mem_edit_a2e;
	MemoryEditor mem_edit_sdhr_upload;
	
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
	
	pGui->mem_edit_a2e.Open = false;
	pGui->mem_edit_a2e.HighlightFn = Memory_HighlightWriteFunction;
	pGui->mem_edit_sdhr_upload.Open = false;
}

MainMenu::~MainMenu() {
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();
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
			switch (event.key.keysym.sym)
			{
				case SDLK_F2:
					pGui->bShowA2VideoWindow = !pGui->bShowA2VideoWindow;
					eventIsHandledInImGui = true;
					break;
				case SDLK_F3:
					pGui->bShowPPWindow = !pGui->bShowPPWindow;
					eventIsHandledInImGui = true;
					break;
				case SDLK_F9:
					pGui->bShowSSWindow = !pGui->bShowSSWindow;
					eventIsHandledInImGui = true;
					break;
				default:
					break;
			};
		}
			break;
		default:
			break;
	}   // switch event.type
	
	return eventIsHandledInImGui;
}

void MainMenu::Render() {
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL2_NewFrame(window_);
	ImGui::NewFrame();
	
	auto a2VideoManager = A2VideoManager::GetInstance();
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
		ImGui::Text("Screen: %dx%d (%dx%d) - ", 
			a2VideoManager->ScreenSize().x, a2VideoManager->ScreenSize().y,
			screen_width, screen_height
		);
		ImGuiIO& io = ImGui::GetIO(); (void)io;
		// If the frame rate is halved, io.Framerate would display twice the framerate
		// because we're flipping backbuffers half as much. So we need to divide by 4
		// to get the real frame rate
		auto realFrameRate = (PostProcessor::GetInstance()->IsFrameRateHalved() ? io.Framerate / 2.0f : io.Framerate);
		ImGui::Text("FrameID: %d, Avg %.3f ms/f (%.1f FPS)",
					A2VideoManager::GetInstance()->GetVRAMReadId(),
					1000.0f / realFrameRate, realFrameRate);
		ImGui::PopFont();
		
		ImGui::EndMainMenuBar();
		
		// Show about window
		if (pGui->bShowAboutWindow) {
			ImGui::PushFont(_menuFont);
			ImGui::Begin("About", &pGui->bShowAboutWindow, ImGuiWindowFlags_AlwaysAutoResize);
			ImGui::Text("Super Duper Display");
			ImGui::Separator();
			ImGui::Text("Version: 0.5.1");
			ImGui::Text("Software: Henri \"Rikkles\" Asseily");
			ImGui::Text("Design & Firmware: John \"Elltwo\" Flanagan");
			ImGui::Text("Appletini logo by Rikkles+Fatdog");
			ImGui::Separator();
			
			ImGui::TextWrapped("SuperDuperDisplay is a hybrid emulation frontend for Appletini, the Apple 2 Bus Card.");
			ImGui::Separator();
			// Retrieve OpenGL version info
			const GLubyte* renderer = glGetString(GL_RENDERER);
			const GLubyte* version = glGetString(GL_VERSION);
			GLint major, minor;
			glGetIntegerv(GL_MAJOR_VERSION, &major);
			glGetIntegerv(GL_MINOR_VERSION, &minor);
			GLint accelerated = 0;
			SDL_GL_GetAttribute(SDL_GL_ACCELERATED_VISUAL, &accelerated);
			ImGui::Text("Renderer: %s", renderer);
			ImGui::Text("OpenGL version: %s", version);
			ImGui::Text("Major version: %d", major);
			ImGui::Text("Minor version: %d", minor);
			ImGui::Text("Hardware Acceleration: %s", accelerated ? "Enabled" : "Disabled");
			ImGui::Separator();
			ImGui::TextColored(ImColor(220, 150, 0), "Press F1 to show/hide the UI");
			ImGui::PopFont();
			ImGui::End();
		}
		// Show the Apple //e memory
		if (pGui->mem_edit_a2e.Open)
		{
			pGui->mem_edit_a2e.DrawWindow("Memory Editor: Apple 2 Memory (0000-C000 x2)", MemoryManager::GetInstance()->GetApple2MemPtr(), 2 * _A2_MEMORY_SHADOW_END);
		}
		if (pGui->bShowMemoryHeatMap)
		{
			ImGui::SetNextWindowSize(ImVec2(624,862));
			if (ImGui::Begin("Memory Heat Map", &pGui->bShowMemoryHeatMap, ImGuiWindowFlags_NoResize)) {
				auto drawList = ImGui::GetWindowDrawList();
				ImVec2 oPos = ImGui::GetCursorScreenPos();	// origin, i.e. "layouting position" where items are submitted
				ImColor yellowColor(1.0f, 1.0f, 0.0f, 1.0f);
				ImColor memLinesColor(1.0f, 1.0f, 0.0f, 0.5f);
				float mmultw = 1.f;							// width of each pixel represneting a byte
				float mmulth = 3.f;							// height of each pixel representing a byte
				float memDrawW = 256 * mmultw;
				float memDrawH = 256 * mmulth;

				// Draw the mem rectangles
				float memY = 30.f;		// Y top margin for mem rectangles
				float labelsW = 30.f;	// X left margin that will have the labels
				ImVec2 mainRectMin = ImVec2(oPos.x + labelsW, oPos.y + memY);
				ImVec2 mainRectMax = ImVec2(mainRectMin.x + memDrawW + 2.f, mainRectMin.y + memDrawH + 2.f);
				drawList->AddRect(mainRectMin, mainRectMax, yellowColor);
				ImVec2 auxRectMin = ImVec2(mainRectMax.x + labelsW, mainRectMin.y);
				ImVec2 auxRectMax = ImVec2(mainRectMax.x + labelsW + memDrawW + 2.f, mainRectMax.y);
				drawList->AddRect(auxRectMin, auxRectMax, yellowColor);
				
				// Draw the mem rectangles titles
				ImGui::PushFont(pGui->fontLarge);
				ImVec2 mainLabelPos = ImVec2(CalcCenteredTextX("MAIN MEMORY", mainRectMin.x, mainRectMax.x), mainRectMin.y - 20.f);
				drawList->AddText(mainLabelPos, yellowColor, "MAIN MEMORY");
				ImVec2 auxLabelPos = ImVec2(CalcCenteredTextX("AUX MEMORY", auxRectMin.x, auxRectMax.x), auxRectMin.y - 20.f);
				drawList->AddText(auxLabelPos, yellowColor, "AUX MEMORY");
				ImGui::PopFont();
				
				// Draw the heat map
				auto currT = CycleCounter::GetInstance()->GetCycleTimestamp();
				auto memMgr = MemoryManager::GetInstance();
				for (auto j=0; j < 2; ++j) {
					for (auto i=0; i < _A2_MEMORY_SHADOW_END; ++i) {
						auto tdiff = currT - memMgr->GetMemWriteTimestamp(i + j*_A2_MEMORY_SHADOW_END);
						if (tdiff < (pGui->mem_edit_a2e.OptHighlightFnSeconds * 1'000'000)) {
							auto writeColor = ImColor(1.f - ((float)tdiff / (pGui->mem_edit_a2e.OptHighlightFnSeconds * 1'000'000)), 0.f, 0.f, 1.f);
							auto rectMin = (j == 0 ? mainRectMin : auxRectMin);
							auto pMin = ImVec2(rectMin.x + 1 + (i % 0x100) * mmultw, rectMin.y + 1 + (i / 0x100) * mmulth);
							auto pMax = ImVec2(pMin.x + mmultw, pMin.y + (mmulth - 1.f));
							drawList->AddRectFilled(pMin, pMax, writeColor);
						}
					}
				}
				
				// Labels on the left and memory chunk lines
				const int labelsHex[] = { 0x0, 0x400, 0x800, 0xC00, 0x2000, 0x4000, 0x6000, 0x8000, 0xA000, 0xC000, 0xD000, 0xE000 };
				const size_t ctLabels = sizeof(labelsHex) / sizeof(labelsHex[0]);
				char bufLabels[ctLabels];
				for (int i = 0; i < ctLabels; ++i)
				{
					snprintf(bufLabels, sizeof(bufLabels), "%04X", labelsHex[i]);
					float yDelta = (float)labelsHex[i] * mmulth / 0x100;
					drawList->AddText(ImVec2(oPos.x, mainRectMin.y + yDelta), yellowColor, bufLabels);
					drawList->AddLine(ImVec2(mainRectMin.x - 20.f, mainRectMin.y + yDelta), ImVec2(auxRectMax.x, mainRectMin.y + yDelta), memLinesColor);
				}
			}
			ImGui::End();
			
		}
		// Show the textures starting at _TEXUNIT_IMAGE_ASSETS_START
		if (pGui->bShowTextureWindow)
		{
			ImGui::SetNextWindowSizeConstraints(ImVec2(300, 250), ImVec2(FLT_MAX, FLT_MAX));
			ImGui::Begin("Texture Viewer", &pGui->bShowTextureWindow);
			ImVec2 avail_size = ImGui::GetContentRegionAvail();
			ImGui::SliderInt("Texture Slot Number", &pGui->iTextureSlotIdx, 0, _SDHR_MAX_TEXTURES + 3, "slot %d", ImGuiSliderFlags_AlwaysClamp);
			GLint _w, _h;
			auto glhelper = OpenGLHelper::GetInstance();
			if (pGui->iTextureSlotIdx < _SDHR_MAX_TEXTURES)
			{
				glBindTexture(GL_TEXTURE_2D, glhelper->get_texture_id_at_slot(pGui->iTextureSlotIdx));
				glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &_w);
				glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &_h);
				ImGui::Text("Texture ID: %d (%d x %d)", (int)glhelper->get_texture_id_at_slot(pGui->iTextureSlotIdx), _w, _h);
				ImGui::Image(reinterpret_cast<void*>(glhelper->get_texture_id_at_slot(pGui->iTextureSlotIdx)),
							 ImVec2(avail_size.x, avail_size.y - 30), ImVec2(0, 0), ImVec2(1, 1));
			}
			else if (pGui->iTextureSlotIdx == _SDHR_MAX_TEXTURES)
			{
				glBindTexture(GL_TEXTURE_2D, a2VideoManager->GetOutputTextureId());
				glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &_w);
				glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &_h);
				ImGui::Text("Output Texture ID: %d (%d x %d)", (int)a2VideoManager->GetOutputTextureId(), _w, _h);
				ImGui::Image(reinterpret_cast<void*>(a2VideoManager->GetOutputTextureId()), avail_size, ImVec2(0, 0), ImVec2(1, 1));
			}
			else if (pGui->iTextureSlotIdx == _SDHR_MAX_TEXTURES + 1)
			{
				glActiveTexture(_TEXUNIT_PP_BEZEL);
				GLint target_tex_id = 0;
				glGetIntegerv(GL_TEXTURE_BINDING_2D, &target_tex_id);
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, target_tex_id);
				glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &_w);
				glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &_h);
				ImGui::Text("_TEXUNIT_PP_BEZEL: %d (%d x %d)", target_tex_id, _w, _h);
				ImGui::Image(reinterpret_cast<void*>(target_tex_id), avail_size, ImVec2(0, 0), ImVec2(1, 1));
			}
			else if (pGui->iTextureSlotIdx == _SDHR_MAX_TEXTURES + 2)
			{
				glActiveTexture(_TEXUNIT_PP_PREVIOUS);
				GLint target_tex_id = 0;
				glGetIntegerv(GL_TEXTURE_BINDING_2D, &target_tex_id);
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, target_tex_id);
				glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &_w);
				glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &_h);
				ImGui::Text("_TEXUNIT_PP_PREVIOUS: %d (%d x %d)", target_tex_id, _w, _h);
				ImGui::Image(reinterpret_cast<void*>(target_tex_id), avail_size, ImVec2(0, 0), ImVec2(1, 1));
			}
			else if (pGui->iTextureSlotIdx == _SDHR_MAX_TEXTURES + 3)
			{
				glActiveTexture(_TEXUNIT_POSTPROCESS);
				GLint target_tex_id = 0;
				glGetIntegerv(GL_TEXTURE_BINDING_2D, &target_tex_id);
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, target_tex_id);
				glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &_w);
				glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &_h);
				ImGui::Text("_TEXUNIT_POSTPROCESS: %d (%d x %d)", target_tex_id, _w, _h);
				ImGui::Image(reinterpret_cast<void*>(target_tex_id), avail_size, ImVec2(0, 0), ImVec2(1, 1));
			}
			glActiveTexture(GL_TEXTURE0);
			ImGui::End();
		}
		
		if (pGui->bShowA2VideoWindow)
			A2VideoManager::GetInstance()->DisplayImGuiWindow(&pGui->bShowA2VideoWindow);
		if (pGui->bShowLoadFileWindow)
			A2VideoManager::GetInstance()->DisplayImGuiLoadFileWindow(&pGui->bShowLoadFileWindow);
		if (pGui->bShowPPWindow)
			PostProcessor::GetInstance()->DisplayImGuiWindow(&pGui->bShowPPWindow);
		if (pGui->bShowEventRecorderWindow)
			EventRecorder::GetInstance()->DisplayImGuiWindow(&pGui->bShowEventRecorderWindow);

		if (pGui->bShowSSWindow) {
			ImGui::SetNextWindowSizeConstraints(ImVec2(170, 410), ImVec2(FLT_MAX, FLT_MAX));
			ImGui::Begin("Soft Switches", &pGui->bShowSSWindow);
			auto memManager = MemoryManager::GetInstance();
			bool ssValue0 = memManager->IsSoftSwitch(A2SS_80STORE);
			if (ImGui::Checkbox("A2SS_80STORE", &ssValue0)) {
				memManager->SetSoftSwitch(A2SS_80STORE, ssValue0);
				a2VideoManager->ForceBeamFullScreenRender();
			}
			bool ssValue1 = memManager->IsSoftSwitch(A2SS_RAMRD);
			if (ImGui::Checkbox("A2SS_RAMRD", &ssValue1)) {
				memManager->SetSoftSwitch(A2SS_RAMRD, ssValue1);
				a2VideoManager->ForceBeamFullScreenRender();
			}
			bool ssValue2 = memManager->IsSoftSwitch(A2SS_RAMWRT);
			if (ImGui::Checkbox("A2SS_RAMWRT", &ssValue2)) {
				memManager->SetSoftSwitch(A2SS_RAMWRT, ssValue2);
				a2VideoManager->ForceBeamFullScreenRender();
			}
			bool ssValue3 = memManager->IsSoftSwitch(A2SS_80COL);
			if (ImGui::Checkbox("A2SS_80COL", &ssValue3)) {
				memManager->SetSoftSwitch(A2SS_80COL, ssValue3);
				a2VideoManager->ForceBeamFullScreenRender();
			}
			bool ssValue4 = memManager->IsSoftSwitch(A2SS_ALTCHARSET);
			if (ImGui::Checkbox("A2SS_ALTCHARSET", &ssValue4)) {
				memManager->SetSoftSwitch(A2SS_ALTCHARSET, ssValue4);
				a2VideoManager->ForceBeamFullScreenRender();
			}
			bool ssValue5 = memManager->IsSoftSwitch(A2SS_INTCXROM);
			if (ImGui::Checkbox("A2SS_INTCXROM", &ssValue5)) {
				memManager->SetSoftSwitch(A2SS_INTCXROM, ssValue5);
				a2VideoManager->ForceBeamFullScreenRender();
			}
			bool ssValue6 = memManager->IsSoftSwitch(A2SS_SLOTC3ROM);
			if (ImGui::Checkbox("A2SS_SLOTC3ROM", &ssValue6)) {
				memManager->SetSoftSwitch(A2SS_SLOTC3ROM, ssValue6);
				a2VideoManager->ForceBeamFullScreenRender();
			}
			bool ssValue7 = memManager->IsSoftSwitch(A2SS_TEXT);
			if (ImGui::Checkbox("A2SS_TEXT", &ssValue7)) {
				memManager->SetSoftSwitch(A2SS_TEXT, ssValue7);
				a2VideoManager->ForceBeamFullScreenRender();
			}
			bool ssValue8 = memManager->IsSoftSwitch(A2SS_MIXED);
			if (ImGui::Checkbox("A2SS_MIXED", &ssValue8)) {
				memManager->SetSoftSwitch(A2SS_MIXED, ssValue8);
				a2VideoManager->ForceBeamFullScreenRender();
			}
			bool ssValue9 = memManager->IsSoftSwitch(A2SS_PAGE2);
			if (ImGui::Checkbox("A2SS_PAGE2", &ssValue9)) {
				memManager->SetSoftSwitch(A2SS_PAGE2, ssValue9);
				a2VideoManager->ForceBeamFullScreenRender();
			}
			bool ssValue10 = memManager->IsSoftSwitch(A2SS_HIRES);
			if (ImGui::Checkbox("A2SS_HIRES", &ssValue10)) {
				memManager->SetSoftSwitch(A2SS_HIRES, ssValue10);
				a2VideoManager->ForceBeamFullScreenRender();
			}
			bool ssValue11 = memManager->IsSoftSwitch(A2SS_DHGR);
			if (ImGui::Checkbox("A2SS_DHGR", &ssValue11)) {
				memManager->SetSoftSwitch(A2SS_DHGR, ssValue11);
				a2VideoManager->ForceBeamFullScreenRender();
			}
			bool ssValue12 = memManager->IsSoftSwitch(A2SS_DHGRMONO);
			if (ImGui::Checkbox("A2SS_DHGRMONO", &ssValue12)) {
				memManager->SetSoftSwitch(A2SS_DHGRMONO, ssValue12);
				a2VideoManager->ForceBeamFullScreenRender();
			}
			bool ssValue13 = memManager->IsSoftSwitch(A2SS_SHR);
			if (ImGui::Checkbox("A2SS_SHR", &ssValue13)) {
				memManager->SetSoftSwitch(A2SS_SHR, ssValue13);
				a2VideoManager->ForceBeamFullScreenRender();
			}
			bool ssValue14 = memManager->IsSoftSwitch(A2SS_GREYSCALE);
			if (ImGui::Checkbox("A2SS_GREYSCALE", &ssValue14)) {
				memManager->SetSoftSwitch(A2SS_GREYSCALE, ssValue14);
				a2VideoManager->ForceBeamFullScreenRender();
			}
			ImGui::Separator();
			if (ImGui::Button("Reset Soft Switches")) {
				Main_ResetA2SS();
				a2VideoManager->ForceBeamFullScreenRender();
			}
			ImGui::End();
		}
		
		A2VideoManager::GetInstance()->DisplayImGuiExtraWindows();
		
		if (pGui->mem_edit_sdhr_upload.Open)
		{
			auto memManager = MemoryManager::GetInstance();
			pGui->mem_edit_sdhr_upload.DrawWindow("Memory Editor: SDHR Upload Region", memManager->GetApple2MemPtr(), 2 * _A2_MEMORY_SHADOW_END);
		}
		
		if (pGui->bShowImGuiMetricsWindow)
			ImGui::ShowMetricsWindow(&pGui->bShowImGuiMetricsWindow);
	}
	
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}


void MainMenu::ShowSDDMenu() {
	/*
	 // List video drivers for debugging
	 if (ImGui::BeginMenu("Video Drivers"))
	 {
	 auto _n = SDL_GetNumVideoDrivers();
	 for (size_t i = 0; i < _n; i++)
	 {
	 ImGui::Text(SDL_GetVideoDriver(i));
	 }
	 ImGui::EndMenu();
	 }
	 */
#ifndef __APPLE__
	// For OSX, don't let SDL handle fullscreen. It has the potential to crash if the user
	// maximizes the window to fullscreen as well. So completely hide all fullscreen options in OSX
	if (ImGui::MenuItem("Fullscreen", "Alt+Enter", Main_IsFullScreen())) {
		Main_SetFullScreen(!Main_IsFullScreen());
	}
	if (ImGui::BeginMenu("Fullscreen Resolution")) {
		// FIXME: Figure out the display index for full screen mode
		int displayIndex = SDL_GetWindowDisplayIndex(Main_GetSDLWindow());
		SDL_DisplayMode currentDisplayMode = Main_GetFullScreenMode();
		if (pGui->iCurrentDisplayIndex != displayIndex)
		{
			// display changed, let's get its info
			int numDisplayModes = SDL_GetNumDisplayModes(displayIndex);
			SDL_DisplayMode _lastMode;
			_lastMode.w = _lastMode.h = _lastMode.refresh_rate = 0;

			pGui->v_displayModes.clear();
			for (int i = 0; i < numDisplayModes; ++i) {
				SDL_DisplayMode mode;
				if (SDL_GetDisplayMode(displayIndex, i, &mode) != 0) {
					std::cerr << "SDL_GetDisplayMode failed: " << SDL_GetError() << std::endl;
					continue;
				}
				// Only store the highest refresh rate modes
				if (!(_lastMode.w == mode.w && _lastMode.h == mode.h && _lastMode.refresh_rate > mode.refresh_rate))
				{
					pGui->v_displayModes.push_back(mode);
					_lastMode = mode;
				}
			}
		}

		bool foundCurrentMode = false;
		bool isCurrentMode = false;
		char modeDescription[200];
		for (int i = 0; i < pGui->v_displayModes.size(); ++i)
		{
			auto mode = pGui->v_displayModes[i];
			snprintf(modeDescription, 199, "%dx%d @ %dHz", mode.w, mode.h, mode.refresh_rate);
			if (foundCurrentMode == false)
			{
				isCurrentMode = (
					mode.w == currentDisplayMode.w &&
					mode.h == currentDisplayMode.h &&
					mode.refresh_rate == currentDisplayMode.refresh_rate);
				isCurrentMode ? foundCurrentMode = true : foundCurrentMode = false;
			}
			else
				isCurrentMode = false;
			if (!Main_IsLinuxConsole())
				ImGui::BeginDisabled();
			if (ImGui::MenuItem(modeDescription, "", isCurrentMode))
				Main_SetFullScreenMode(mode);
			if (!Main_IsLinuxConsole())
				ImGui::EndDisabled();
		}
		if (foundCurrentMode == false)
		{
			// Couldn't find a mode
			Main_SetFullScreenMode(pGui->v_displayModes[0]);
		}
		ImGui::EndMenu();
	}
#endif
	SwapInterval_e iMMVsync = Main_GetVsync();
	int bMMVsync = 0;
	if (iMMVsync == SWAPINTERVAL_VSYNC || iMMVsync == SWAPINTERVAL_ADAPTIVE)
		bMMVsync = 1;
	else if (iMMVsync == SWAPINTERVAL_APPLE2BUS)
		bMMVsync = 2;
	if (ImGui::RadioButton("VSYNC Monitor", &bMMVsync, 1)) {
		Main_SetVsync(SWAPINTERVAL_ADAPTIVE);
		iMMVsync = Main_GetVsync();
	}
	ImGui::SetItemTooltip("Standard monitor VSYNC, uses adaptive VSYNC when available. \nThis is the default. The Apple 2 renderer still renders at the speed of the \nApple 2 bus, but the final postprocessing is done at the monitor's VSYNC speed");
	if (iMMVsync == SWAPINTERVAL_VSYNC || iMMVsync == SWAPINTERVAL_ADAPTIVE)
	{
		if (iMMVsync == SWAPINTERVAL_ADAPTIVE)
		{
			ImGui::SameLine();
			ImGui::Text("(Adaptive)");
		}
	}
	if (ImGui::RadioButton("VSYNC Appletini", &bMMVsync, 2)) {
		Main_SetVsync(SWAPINTERVAL_APPLE2BUS);
	}
	ImGui::SetItemTooltip("Syncs to the Apple 2's bus. Everything will run at PAL \nor NTSC refresh rates, depending on your Apple 2 version");
	if (ImGui::RadioButton("No VSYNC", &bMMVsync, 0)) {
		Main_SetVsync(SWAPINTERVAL_NONE);
	}
	ImGui::SetItemTooltip("VSYNC disabled. You can choose your own postprocessing refresh speed. \nNot recommended unless you're testing your setup's maximum performance, \nor want to see what would happen if your Apple 2 were to be connected \nto a very slow refresh rate monitor");
	if (bMMVsync > 0)
		ImGui::BeginDisabled(true);
	ImGui::SameLine();ImGui::Spacing();ImGui::SameLine();
	if (ImGui::BeginMenu("FPS Limiter")) {
		pGui->iFPSLimiter = Main_GetFPSLimit();
		if (ImGui::RadioButton("Disabled##FPSLIMIT", &pGui->iFPSLimiter, UINT32_MAX))
			Main_SetFPSLimit(UINT32_MAX);
		if (ImGui::RadioButton("15 Hz##FPSLIMIT", &pGui->iFPSLimiter, 15))
			Main_SetFPSLimit(15);
		if (ImGui::RadioButton("20 Hz##FPSLIMIT", &pGui->iFPSLimiter, 20))
			Main_SetFPSLimit(20);
		if (ImGui::RadioButton("30 Hz##FPSLIMIT", &pGui->iFPSLimiter, 30))
			Main_SetFPSLimit(30);
		if (ImGui::RadioButton("45 Hz##FPSLIMIT", &pGui->iFPSLimiter, 45))
			Main_SetFPSLimit(45);
		if (ImGui::RadioButton("50 Hz##FPSLIMIT", &pGui->iFPSLimiter, 50))
			Main_SetFPSLimit(50);
		if (ImGui::RadioButton("60 Hz##FPSLIMIT", &pGui->iFPSLimiter, 60))
			Main_SetFPSLimit(60);
		if (ImGui::RadioButton("100 Hz##FPSLIMIT", &pGui->iFPSLimiter, 100))
			Main_SetFPSLimit(100);
		if (ImGui::RadioButton("120 Hz##FPSLIMIT", &pGui->iFPSLimiter, 120))
			Main_SetFPSLimit(120);
		ImGui::EndMenu();
	}
	if (bMMVsync)
		ImGui::EndDisabled();
	if (ImGui::BeginMenu("Background Color")) {
		float windowBGColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f }; // RGBA
		Main_GetBGColor(windowBGColor);
		if (ImGui::ColorEdit4("", windowBGColor)) {
			Main_SetBGColor(windowBGColor);
		}
		ImGui::EndMenu();
	}
	ImGui::Separator();
	if (ImGui::BeginMenu("Appletini")) {
		ImGui::Text("%s", get_tini_name_string().c_str());
		if (get_tini_last_error() == 19)	// FT_TIMEOUT
			ImGui::Text("%s", "No data (Apple 2 is off?)");
		else
			ImGui::Text("%s", get_tini_last_error_string().c_str());
		ImGui::EndMenu();
	}
	ImGui::Separator();
	ImGui::Separator();
	if (ImGui::MenuItem("Reset SDD")) {
		auto switch_c034 = MemoryManager::GetInstance()->switch_c034;
		A2VideoManager::GetInstance()->ResetComputer();
		MemoryManager::GetInstance()->switch_c034 = switch_c034;
		Main_DisplaySplashScreen();
	}
	ImGui::Separator();
	ImGui::MenuItem("About", "", &pGui->bShowAboutWindow);
	ImGui::Separator();
	if (ImGui::MenuItem("Quit", "Alt+F4")) {
		Main_RequestAppQuit();
	}
}

void MainMenu::ShowMotherboardMenu() {
	if (ImGui::BeginMenu("Region")) {
		auto cycleCounter = CycleCounter::GetInstance();
		int vbl_region;
		vbl_region = (cycleCounter->GetVideoRegion() == VideoRegion_e::PAL ? 1 : 2);

		if (ImGui::RadioButton("PAL##REGION", &vbl_region, 1))
			cycleCounter->SetVideoRegion(VideoRegion_e::PAL);

		ImGui::SameLine();
		if (ImGui::RadioButton("NTSC##REGION", &vbl_region, 2))
			cycleCounter->SetVideoRegion(VideoRegion_e::NTSC);
		int vbl_slider_val = (int)cycleCounter->GetScreenCycles();
		if (ImGui::InputInt("VBL Start Shift", &vbl_slider_val, 1, (CYCLES_TOTAL_PAL - CYCLES_TOTAL_NTSC) / 10))
		{
			cycleCounter->SetVBLStart(vbl_slider_val);
		}
		ImGui::SetItemTooltip("Press the '-' button to keep shifting the VBL earlier in the frame \nto realign it manually");
		ImGui::Separator();
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("Character ROMs")) {
		A2VideoManager::GetInstance()->DisplayCharRomsImGuiChunk();
		ImGui::EndMenu();
	}

	ImGui::MenuItem("Apple //e Memory", "", &pGui->mem_edit_a2e.Open);
	ImGui::MenuItem("Apple //e Memory Heat Map", "", &pGui->bShowMemoryHeatMap);
}

void MainMenu::ShowVideoMenu() {
	ImGui::MenuItem("Apple 2 Video Settings", "F2", &pGui->bShowA2VideoWindow);
	ImGui::MenuItem("Post Processor Settings", "F3", &pGui->bShowPPWindow);
	ImGui::Separator();
	if (ImGui::MenuItem("On-Screen FPS", "F8", Main_IsFPSOverlay())) {
		Main_SetFPSOverlay(!Main_IsFPSOverlay());
		Main_ResetFPSCalculations();
		A2VideoManager::GetInstance()->ForceBeamFullScreenRender();
	}
	if (ImGui::MenuItem("Reset FPS", "Shift+F8")) {
		Main_ResetFPSCalculations();
		A2VideoManager::GetInstance()->ForceBeamFullScreenRender();
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
	auto memManager = MemoryManager::GetInstance();
	auto a2VideoManager = A2VideoManager::GetInstance();
	auto eventRecorder = EventRecorder::GetInstance();

	if (ImGui::MenuItem("Text & HGR")) {
		Main_ResetA2SS();
		memManager->SetSoftSwitch(A2SS_SHR, false);
		memManager->SetSoftSwitch(A2SS_TEXT, true);
		memManager->SetSoftSwitch(A2SS_HIRES, false);
		MemoryLoad("samples/tomahawk2_hgr.bin", 0, false);
		a2VideoManager->ForceBeamFullScreenRender();
	}
	if (ImGui::MenuItem("DHGR")) {
		Main_ResetA2SS();
		memManager->SetSoftSwitch(A2SS_SHR, false);
		memManager->SetSoftSwitch(A2SS_TEXT, false);
		memManager->SetSoftSwitch(A2SS_80COL, true);
		memManager->SetSoftSwitch(A2SS_HIRES, true);
		memManager->SetSoftSwitch(A2SS_DHGR, true);
		memManager->SetSoftSwitch(A2SS_MIXED, true);
		MemoryLoad("samples/e0_ultima2_dhgr.bin", 0, false);
		MemoryLoad("samples/e1_ultima2_dhgr.bin", 0, true);
		a2VideoManager->ForceBeamFullScreenRender();
	}
	if (ImGui::MenuItem("GS Snapshot")) {
		Main_ResetA2SS();
		memManager->SetSoftSwitch(A2SS_SHR, true);
		memManager->SetSoftSwitch(A2SS_TEXT, false);
		memManager->SetSoftSwitch(A2SS_HIRES, true);
		MemoryLoad("samples/bank_e0_0_bfff.bin", 0, false);
		MemoryLoad("samples/bank_e1_0_bfff.bin", 0, true);
		a2VideoManager->ForceBeamFullScreenRender();
	}
	if (ImGui::MenuItem("HGR SPEC1")) {
		Main_ResetA2SS();
		memManager->SetSoftSwitch(A2SS_SHR, false);
		memManager->SetSoftSwitch(A2SS_TEXT, false);
		memManager->SetSoftSwitch(A2SS_HIRES, true);
		a2VideoManager->bUseHGRSPEC1 = true;
		MemoryLoadHGR("samples/arcticfox.hgr");
		a2VideoManager->ForceBeamFullScreenRender();
	}
	if (ImGui::MenuItem("DHGR Col140Mixed")) {
		Main_ResetA2SS();
		memManager->SetSoftSwitch(A2SS_SHR, false);
		memManager->SetSoftSwitch(A2SS_TEXT, false);
		memManager->SetSoftSwitch(A2SS_80COL, true);
		memManager->SetSoftSwitch(A2SS_HIRES, true);
		memManager->SetSoftSwitch(A2SS_DHGR, true);
		a2VideoManager->bUseDHGRCOL140Mixed = true;
		MemoryLoadDHR("samples/extasie0_140mix.dhr");
		a2VideoManager->ForceBeamFullScreenRender();
	}
	if (ImGui::MenuItem("SHR+Legacy")) {
		Main_ResetA2SS();
		memManager->SetSoftSwitch(A2SS_SHR, true);
		MemoryLoadSHR("samples/paintworks.shr");
		std::ifstream legacydemo("./samples/tomahawk2_hgr.bin", std::ios::binary);
		legacydemo.seekg(0, std::ios::beg); // Go back to the start of the file
		legacydemo.read(reinterpret_cast<char*>(MemoryManager::GetInstance()->GetApple2MemPtr()), 0x4000);
		a2VideoManager->bDEMOMergedMode = true;
		a2VideoManager->bAlignQuadsToScanline = true;
		a2VideoManager->ForceBeamFullScreenRender();
	}
	if (ImGui::MenuItem("SHR RGGB (Bayer) 320@16")) {
		Main_ResetA2SS();
		memManager->SetSoftSwitch(A2SS_SHR, true);
		memManager->SetSoftSwitch(A2SS_TEXT, false);
		memManager->SetSoftSwitch(A2SS_HIRES, false);
		MemoryLoadSHR("samples/SHR RGGB/320_16_abstracteyear99#C10000");
		a2VideoManager->ForceBeamFullScreenRender();
	}
	if (ImGui::MenuItem("SHR RGGB (Bayer) 640@4")) {
		Main_ResetA2SS();
		memManager->SetSoftSwitch(A2SS_SHR, true);
		memManager->SetSoftSwitch(A2SS_TEXT, false);
		memManager->SetSoftSwitch(A2SS_HIRES, false);
		MemoryLoadSHR("samples/SHR RGGB/640_04_abstracteyear99#C10000");
		a2VideoManager->ForceBeamFullScreenRender();
	}
	if (ImGui::MenuItem("SHR Animation (PWA $C2)")) {
		Main_ResetA2SS();
		memManager->SetSoftSwitch(A2SS_SHR, true);
		memManager->SetSoftSwitch(A2SS_TEXT, false);
		memManager->SetSoftSwitch(A2SS_HIRES, false);
		std::ifstream animationFile("recordings/anim00032#C20000", std::ios::binary);
		pGui->bShowEventRecorderWindow = true;
		eventRecorder->ReadPaintWorksAnimationsFile(animationFile);
	}
	auto _smtext = (pGui->bSampleRunKarateka ? "Stop Karateka Demo" : "Run Karateka Demo");
	if (ImGui::MenuItem(_smtext)) {
		pGui->bSampleRunKarateka = !pGui->bSampleRunKarateka;
		if (pGui->bSampleRunKarateka) {
			std::ifstream karatekafile("./recordings/karateka.vcr", std::ios::binary);
			Main_ResetA2SS();
			memManager->SetSoftSwitch(A2SS_SHR, false);
			eventRecorder->ReadRecordingFile(karatekafile);
			eventRecorder->StartReplay();
			memManager->SetSoftSwitch(A2SS_TEXT, false);
			memManager->SetSoftSwitch(A2SS_HIRES, true);
			a2VideoManager->ForceBeamFullScreenRender();
		} else {
			eventRecorder->StopReplay();
		}
		Main_ResetFPSCalculations();
		a2VideoManager->ForceBeamFullScreenRender();
	}
	if (ImGui::MenuItem("Speech Demo")) {
		MockingboardManager::GetInstance()->Util_SpeakDemoPhrase();
	}
}

void MainMenu::ShowDeveloperMenu() {
	auto a2VideoManager = A2VideoManager::GetInstance();
	if (ImGui::MenuItem("Run Vertical Refresh", "F10"))
		A2VideoManager::GetInstance()->ForceBeamFullScreenRender();
	if (ImGui::MenuItem("Continuous Refresh", "Shift-F10", &a2VideoManager->bAlwaysRenderBuffer)) {
		A2VideoManager::GetInstance()->ForceBeamFullScreenRender();
	}
	ImGui::MenuItem("Load File Into Memory", "", &pGui->bShowLoadFileWindow);
	ImGui::Separator();
	ImGui::MenuItem("Soft Switches", "F9", &pGui->bShowSSWindow);
	ImGui::MenuItem("Event Recorder", "", &pGui->bShowEventRecorderWindow);
	if (ImGui::BeginMenu("Graphics Modes Windows")) {
		ImGui::MenuItem("TEXT1", "", &a2VideoManager->bRenderTEXT1);
		ImGui::MenuItem("TEXT2", "", &a2VideoManager->bRenderTEXT2);
		ImGui::MenuItem("HGR1", "", &a2VideoManager->bRenderHGR1);
		ImGui::MenuItem("HGR2", "", &a2VideoManager->bRenderHGR2);
		ImGui::EndMenu();
	}
	/*
	if (ImGui::BeginMenu("Shaders")) {
		ImGui::Text("Legacy Shader");
		const char* _legshaders[] = { "0 - Full",};
		static int _legshader_current = 0;
		if (ImGui::ListBox("##LegacyShader", &_legshader_current, _legshaders, IM_ARRAYSIZE(_legshaders), 4))
		{
			a2VideoManager->SelectLegacyShader(_legshader_current);
			Main_ResetFPSCalculations();
			a2VideoManager->ForceBeamFullScreenRender();
		}
		ImGui::Text("SHR Shader");
		const char* _shrshaders[] = { "0 - Full" };
		static int _shrshader_current = 0;
		if (ImGui::ListBox("##SHRShader", &_shrshader_current, _shrshaders, IM_ARRAYSIZE(_shrshaders), 3))
		{
			a2VideoManager->SelectSHRShader(_shrshader_current);
			Main_ResetFPSCalculations();
			a2VideoManager->ForceBeamFullScreenRender();
		}
		ImGui::EndMenu();
	}
	 */
	if (ImGui::BeginMenu("VRAMs")) {
		ImGui::MenuItem("Legacy", "", &a2VideoManager->mem_edit_vram_legacy.Open);
		ImGui::MenuItem("SHR", "", &a2VideoManager->mem_edit_vram_shr.Open);
		ImGui::MenuItem("Offset Buffer", "", &a2VideoManager->mem_edit_offset_buffer.Open);
		ImGui::EndMenu();
	}
	ImGui::MenuItem("SDD Textures", "", &pGui->bShowTextureWindow);
	ImGui::Separator();
	if (ImGui::BeginMenu("SDHR")) {
		auto sdhrManager = SDHRManager::GetInstance();
		ImGui::MenuItem("Untextured Geometry", "", &sdhrManager->bDebugNoTextures);
		ImGui::MenuItem("Perspective Projection", "", &sdhrManager->bUsePerspective);
		ImGui::MenuItem("Upload Region Memory Window", "", &pGui->mem_edit_sdhr_upload.Open);
		ImGui::EndMenu();
	}
	ImGui::Separator();
	ImGui::MenuItem("ImGui Metrics Window", "", &pGui->bShowImGuiMetricsWindow);
}

// UTILITY

float MainMenu::CalcCenteredTextX(const char* text, float minX, float maxX) {
	ImVec2 textSize = ImGui::CalcTextSize(text);
	float centerX = (minX + maxX) / 2.0f;
	float textStartPosX = centerX - (textSize.x / 2.0f);
	return textStartPosX;
}
