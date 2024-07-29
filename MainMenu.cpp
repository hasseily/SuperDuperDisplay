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
extern void Main_ResetA2SS();
extern bool Main_IsFPSOverlay();
extern void Main_SetFPSOverlay(bool isFPSOverlay);

class MainMenu::Gui {
public:
	ImFont* fontDefault;
	ImFont* fontMedium;
	ImFont* fontLarge;

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
		ImGui::Text("FrameID: %d, Avg %.3f ms/f (%.1f FPS)",
					A2VideoManager::GetInstance()->GetVRAMReadId(),
					1000.0f / io.Framerate, io.Framerate);
		ImGui::PopFont();
		
		ImGui::EndMainMenuBar();
		
		// Show about window
		if (pGui->bShowAboutWindow) {
			ImGui::PushFont(_menuFont);
			ImGui::Begin("About", &pGui->bShowAboutWindow, ImGuiWindowFlags_AlwaysAutoResize);
			ImGui::Text("Super Duper Display");
			ImGui::Separator();
			ImGui::Text("Version: 0.5.0");
			ImGui::Text("Author: Rikkles, Elltwo");  // Author name
			ImGui::Separator();
			
			ImGui::TextWrapped("SuperDuperDisplay is a hybrid emulation frontend for Appletini, the Apple 2 Bus Card.");
			if (ImGui::Button("OK")) {
				pGui->bShowAboutWindow = false;  // Close the "About" window when the OK button is clicked
			}
			ImGui::End();
			ImGui::PopFont();
		}
		// Show the Apple //e memory
		if (pGui->mem_edit_a2e.Open)
		{
			pGui->mem_edit_a2e.DrawWindow("Memory Editor: Apple 2 Memory (0000-C000 x2)", MemoryManager::GetInstance()->GetApple2MemPtr(), 2 * _A2_MEMORY_SHADOW_END);
		}
		// Show the 16 textures loaded (which are always bound to GL_TEXTURE2 -> GL_TEXTURE18)
		if (pGui->bShowTextureWindow)
		{
			ImGui::SetNextWindowSizeConstraints(ImVec2(300, 250), ImVec2(FLT_MAX, FLT_MAX));
			ImGui::Begin("Texture Viewer", &pGui->bShowTextureWindow);
			ImVec2 avail_size = ImGui::GetContentRegionAvail();
			ImGui::SliderInt("Texture Slot Number", &pGui->iTextureSlotIdx, 0, _SDHR_MAX_TEXTURES + 1, "slot %d", ImGuiSliderFlags_AlwaysClamp);
			GLint _w, _h;
			auto glhelper = OpenGLHelper::GetInstance();
			auto a2VideoManager = A2VideoManager::GetInstance();
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
				glActiveTexture(_PP_INPUT_TEXTURE_UNIT);
				GLint target_tex_id = 0;
				glGetIntegerv(GL_TEXTURE_BINDING_2D, &target_tex_id);
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, target_tex_id);
				glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &_w);
				glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &_h);
				ImGui::Text("_PP_INPUT_TEXTURE_UNIT: %d (%d x %d)", target_tex_id, _w, _h);
				ImGui::Image(reinterpret_cast<void*>(target_tex_id), avail_size, ImVec2(0, 0), ImVec2(1, 1));
			}
			glBindTexture(GL_TEXTURE_2D, 0);
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
			auto a2VideoManager = A2VideoManager::GetInstance();
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
#ifndef __APPLE__
	if (ImGui::MenuItem("Fullscreen", "Alt+Enter", Main_IsFullScreen())) {
		Main_SetFullScreen(!Main_IsFullScreen());
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
	// std::string _s_vsync = (iMMVsync == -1 ? "VSYNC (Adaptive)" : "VSYNC");
	// if (ImGui::MenuItem(_s_vsync.c_str(), "", &bMMVsync)) {
	if (ImGui::Checkbox("VSYNC", &bMMVsync)) {
		Main_SetVsync(bMMVsync);
		iMMVsync = Main_GetVsync();
	}
	if (bMMVsync)
	{
		ImGui::SameLine();
		ImGui::Text("On");
		if (iMMVsync)
		{
			ImGui::SameLine();
			ImGui::Text("(Adaptive)");
		}
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
		auto switch_c034 = MemoryManager::GetInstance()->switch_c034;
		A2VideoManager::GetInstance()->ResetComputer();
		MemoryManager::GetInstance()->switch_c034 = switch_c034;
		Main_DisplaySplashScreen();
	}
	ImGui::Separator();
	ImGui::MenuItem("About", "", &pGui->bShowAboutWindow);
	ImGui::Separator();
	if (ImGui::MenuItem("Quit", "Ctrl+C")) {
		HandleQuit();
	}
}

void MainMenu::ShowMotherboardMenu() {
	if (ImGui::BeginMenu("Region")) {
		auto cycleCounter = CycleCounter::GetInstance();
		int vbl_region;
		if (cycleCounter->isVideoRegionDynamic)
			vbl_region = 0;
		else
			vbl_region = (cycleCounter->GetVideoRegion() == VideoRegion_e::PAL ? 1 : 2);

		if (ImGui::RadioButton("Auto##REGION", &vbl_region, 0))
		{
			cycleCounter->isVideoRegionDynamic = true;
		}
		if (vbl_region == 0)
		{
			ImGui::SameLine();
			(cycleCounter->GetVideoRegion() == VideoRegion_e::PAL
			 ? ImGui::Text(" (P)")
			 : ImGui::Text(" (N)"));
		}
		ImGui::SameLine();
		if (ImGui::RadioButton("PAL##REGION", &vbl_region, 1))
		{
			cycleCounter->isVideoRegionDynamic = false;
			cycleCounter->SetVideoRegion(VideoRegion_e::PAL);
		}
		ImGui::SameLine();
		if (ImGui::RadioButton("NTSC##REGION", &vbl_region, 2))
		{
			cycleCounter->isVideoRegionDynamic = false;
			cycleCounter->SetVideoRegion(VideoRegion_e::NTSC);
		}
		if (!cycleCounter->isVideoRegionDynamic)
		{
			int vbl_slider_val = (int)cycleCounter->GetScreenCycles();
			if (ImGui::InputInt("VBL Start Shift", &vbl_slider_val, 1, (CYCLES_TOTAL_PAL-CYCLES_TOTAL_NTSC)/10))
			{
				cycleCounter->SetVBLStart(vbl_slider_val);
			}
		}
		ImGui::Separator();
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("Character ROMs")) {
		A2VideoManager::GetInstance()->DisplayCharRomsImGuiChunk();
		ImGui::EndMenu();
	}

	ImGui::MenuItem("Apple //e Memory", "", &pGui->mem_edit_a2e.Open);
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
	if (ImGui::MenuItem("Reset FPS", "")) {
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
		MemoryLoad("scripts/tomahawk2_hgr.bin", 0, false);
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
		MemoryLoad("scripts/e0_ultima2_dhgr.bin", 0, false);
		MemoryLoad("scripts/e1_ultima2_dhgr.bin", 0, true);
		a2VideoManager->ForceBeamFullScreenRender();
	}
	if (ImGui::MenuItem("GS Snapshot")) {
		Main_ResetA2SS();
		memManager->SetSoftSwitch(A2SS_SHR, true);
		memManager->SetSoftSwitch(A2SS_TEXT, false);
		memManager->SetSoftSwitch(A2SS_HIRES, true);
		MemoryLoad("scripts/bank_e0_0_bfff.bin", 0, false);
		MemoryLoad("scripts/bank_e1_0_bfff.bin", 0, true);
		a2VideoManager->ForceBeamFullScreenRender();
	}
	if (ImGui::MenuItem("HGR SPEC1")) {
		Main_ResetA2SS();
		memManager->SetSoftSwitch(A2SS_SHR, false);
		memManager->SetSoftSwitch(A2SS_TEXT, false);
		memManager->SetSoftSwitch(A2SS_HIRES, true);
		a2VideoManager->bUseHGRSPEC1 = true;
		MemoryLoadHGR("scripts/arcticfox.hgr");
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
		MemoryLoadDHR("scripts/extasie0_140mix.dhr");
		a2VideoManager->ForceBeamFullScreenRender();
	}
	if (ImGui::MenuItem("SHR+Legacy")) {
		Main_ResetA2SS();
		memManager->SetSoftSwitch(A2SS_SHR, true);
		MemoryLoadSHR("scripts/paintworks.shr");
		std::ifstream legacydemo("./scripts/tomahawk2_hgr.bin", std::ios::binary);
		legacydemo.seekg(0, std::ios::beg); // Go back to the start of the file
		legacydemo.read(reinterpret_cast<char*>(MemoryManager::GetInstance()->GetApple2MemPtr()), 0x4000);
		a2VideoManager->bDEMOMergedMode = true;
		a2VideoManager->bForceSHRWidth = true;
		a2VideoManager->bNoMergedModeWobble = true;
		a2VideoManager->ForceBeamFullScreenRender();
	}
	if (ImGui::MenuItem("Run Karateka Demo", "", &pGui->bSampleRunKarateka)) {
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
}

void MainMenu::ShowDeveloperMenu() {
	auto a2VideoManager = A2VideoManager::GetInstance();
	if (ImGui::MenuItem("Run Vertical Refresh", "F10"))
		A2VideoManager::GetInstance()->ForceBeamFullScreenRender();
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

void MainMenu::HandleQuit() {
	std::cout << "Quitting application" << std::endl;
	SDL_Quit();
	exit(0);
}

