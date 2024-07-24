// Super Duper Display uses ImGUI and its renderer for SDL2 + OpenGL

#define GL_SILENCE_DEPRECATION // Silence deprecation warnings on macOS for OpenGL

#define IMGUI_USER_CONFIG "../my_imgui_config.h"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include "imgui_memory_editor.h"
#pragma warning(push, 0) // disables all warnings
#include <SDL.h>
#pragma warning(pop)

#include <cstring>
#include <cstdio>
#include <algorithm>
#include <thread>

#include "common.h"
#include "shader.h"
#include "camera.h"
#include "MosaicMesh.h"

#include "SDHRNetworking.h"
#include "MemoryManager.h"
#include "SDHRManager.h"
#include "A2VideoManager.h"
#include "OpenGLHelper.h"
#include "CycleCounter.h"
#include "SoundManager.h"
#include "MockingboardManager.h"
#include "extras/MemoryLoader.h"
#include "extras/ImGuiFileDialog.h"
#include "PostProcessor.h"
#include "EventRecorder.h"
#include "MainMenu.h"

#if defined(__NETWORKING_APPLE__) || defined (__NETWORKING_LINUX__)
#include <unistd.h>
#include <libgen.h>
#endif

static uint32_t fbWidth = 0;
static uint32_t fbHeight = 0;
static bool g_swapInterval = true;  // VSYNC
static bool g_adaptiveVsync = true;
float window_bgcolor[4] = { 0.0f, 0.0f, 0.0f, 1.0f }; // RGBA

static SDL_Window* window;
static MainMenu* menu = nullptr;

// For FPS calculations
static float fps_worst = 1000000.f;
static uint64_t fps_frame_count = 0;
static auto fps_start_time = SDL_GetTicks();
static char fps_str_buf[40];

bool _M8DBG_bDisableVideoRender = false;
bool _M8DBG_bDisablePPRender = false;
bool _M8DBG_bDisplayFPSOnScreen = false;
float _M8DBG_average_fps_window = 1.f;	// in seconds
bool _M8DBG_bShowF8Window = true;
bool _M8DBG_bRunKarateka = false;
bool _M8DBG_bKaratekaLoadFailed = false;
bool _m8ssSHR = false;
int _M8DBG_windowWidth = 800;
int _M8DBG_windowHeight = 600;

// OpenGL Debug callback function
void GLAPIENTRY DebugCallbackKHR(GLenum source,
								 GLenum type,
								 GLuint id,
								 GLenum severity,
								 GLsizei length,
								 const GLchar* message,
								 const void* userParam) {
	(void)source;		// mark as unused
	(void)id;			// mark as unused
	(void)length;		// mark as unused
	(void)userParam;	// mark as unused
	std::cerr << "GL CALLBACK: " << (type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : "")
	<< " type = " << type << ", severity = " << severity << ", message = " << message << std::endl;
}

bool initialize_glad() {
	if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
		std::cerr << "Failed to initialize GLAD" << std::endl;
		return false;
	}
	return true;
}

void Main_ResetFPSCalculations()
{
	fps_worst = 100000.f;
	fps_frame_count = 0;
	fps_start_time = SDL_GetTicks();
}

int Main_GetVsync()
{
	return SDL_GL_GetSwapInterval();
}

void Main_SetVsync(bool _on)
{
	// If vsync requested, try to make it adaptive vsync first
	if (_on)
	{
		g_adaptiveVsync = (SDL_GL_SetSwapInterval(-1) == 0);	// adaptive
		if (!g_adaptiveVsync) {
			g_swapInterval = (SDL_GL_SetSwapInterval(1) == 0);	// VSYNC
		}
	}
	else
		g_swapInterval = (SDL_GL_SetSwapInterval(0) != 0);		// no VSYNC
	
	Main_ResetFPSCalculations();
}

void Main_DisplaySplashScreen()
{
	if (MemoryLoadSHR("assets/logo.shr"))
	{
		MemoryManager::GetInstance()->SetSoftSwitch(A2SoftSwitch_e::A2SS_SHR, true);
	}
	// Run a refresh to show the first screen
	A2VideoManager::GetInstance()->ForceBeamFullScreenRender();
}

void Main_DrawFPSOverlay()
{
	auto a2VideoManager = A2VideoManager::GetInstance();
	if (_M8DBG_bDisplayFPSOnScreen)
	{
		a2VideoManager->DrawOverlayString("AVERAGE FPS: ", 13, 0b11010010, 0, 0);
		a2VideoManager->DrawOverlayString("WORST FPS: ", 11, 0b11010010, 2, 1);
	} else {
		a2VideoManager->EraseOverlayRange(20, 0, 0);
		a2VideoManager->EraseOverlayRange(20, 0, 1);
	}
}

// True for both SDL_WINDOW_FULLSCREEN and SDL_WINDOW_FULLSCREEN_DESKTOP
bool Main_IsFullScreen() {
	// Assume non-resizable windows are fullscreen
	auto _flags = SDL_GetWindowFlags(window);
	if ((_flags & SDL_WINDOW_RESIZABLE) == 0)
		return true;
	return (_flags & SDL_WINDOW_FULLSCREEN);
}

void Main_SetFullScreen(bool bIsFullscreen) {
	// Don't do anything if it's already in the requested state.
	if (Main_IsFullScreen() == bIsFullscreen)
		return;
	// Do nothing if it's Apple. Let the user maximize via the OSX UI
	// Because if the user sets fullscreen via the OSX UI we won't know,
	// and later setting fullscreen completely messes up SDL2
#ifdef __APPLE__
	return;
#endif
	auto _flags = SDL_GetWindowFlags(window);
	// Don't do anything if the window isn't resizable
	if ((_flags & SDL_WINDOW_RESIZABLE) == 0)
		return;
	SDL_SetWindowFullscreen(window, bIsFullscreen ? SDL_WINDOW_FULLSCREEN : 0);
	Main_ResetFPSCalculations();
}

bool Main_IsImGuiOn()
{
	return (menu != nullptr);
}

void Main_GetBGColor(float outColor[4]) {
	for (int i = 0; i < 4; ++i) {
		outColor[i] = window_bgcolor[i];
	}
}

void Main_SetBGColor(const float newColor[4]) {
	for (int i = 0; i < 4; ++i) {
		window_bgcolor[i] = newColor[i];
	}
}

static void Main_ToggleImGui(SDL_GLContext gl_context)
{
	if (menu == nullptr) {
		menu = new MainMenu(gl_context, window);
	} else if (menu != nullptr) {
		delete menu;
		menu = nullptr;
	}
	Main_ResetFPSCalculations();
}

static void ResetA2SS() {
	auto a2VideoManager = A2VideoManager::GetInstance();
	auto memManager = MemoryManager::GetInstance();

	memManager->SetSoftSwitch(A2SS_TEXT, true);
	memManager->SetSoftSwitch(A2SS_80STORE, false);
	memManager->SetSoftSwitch(A2SS_RAMRD, false);
	memManager->SetSoftSwitch(A2SS_RAMWRT, false);
	memManager->SetSoftSwitch(A2SS_80COL, false);
	memManager->SetSoftSwitch(A2SS_ALTCHARSET, false);
	memManager->SetSoftSwitch(A2SS_INTCXROM, false);
	memManager->SetSoftSwitch(A2SS_SLOTC3ROM, false);
	memManager->SetSoftSwitch(A2SS_MIXED, false);
	memManager->SetSoftSwitch(A2SS_PAGE2, false);
	memManager->SetSoftSwitch(A2SS_HIRES, false);
	memManager->SetSoftSwitch(A2SS_DHGR, false);
	memManager->SetSoftSwitch(A2SS_DHGRMONO, false);
	memManager->SetSoftSwitch(A2SS_SHR, false);
	memManager->SetSoftSwitch(A2SS_GREYSCALE, false);
	a2VideoManager->bUseDHGRCOL140Mixed = false;
	a2VideoManager->bUseHGRSPEC1 = false;
	a2VideoManager->bUseHGRSPEC2 = false;
	a2VideoManager->bDEMOMergedMode = false;
}

// Main code
int main(int argc, char* argv[])
{
	(void)argc;		// mark as unused
	(void)argv;		// mark as unused
#if defined(__NETWORKING_APPLE__) || defined (__NETWORKING_LINUX__)
    // when double-clicking the app, change to its working directory
    char *dir = dirname(strdup(argv[0]));
    chdir(dir);
#endif
	GLenum glerr;
    // Setup SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
    {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

    // Decide GL+GLSL versions
    auto glhelper = OpenGLHelper::GetInstance();
    glhelper->set_gl_version();

    // From 2.0.18: Enable native IME.
#ifdef SDL_HINT_IME_SHOW_UI
    SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
#endif

#ifdef DEBUG
#define _MAINWINDOWNAME "Super Duper Display (DEBUG)"
#else
#define _MAINWINDOWNAME "Super Duper Display"
#endif
    // Create window with graphics context
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
#if defined(__APPLE__)
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL
        | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI
        | SDL_WINDOW_SHOWN);
#elif defined(IMGUI_IMPL_OPENGL_ES2)
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL 
		| SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI 
		| SDL_WINDOW_SHOWN);
#else
	SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL 
        | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI
        | SDL_WINDOW_SHOWN);
#endif
	// Get the actual display size
	SDL_DisplayMode displayMode;
	if (SDL_GetCurrentDisplayMode(0, &displayMode) != 0) {
		std::cerr << "SDL_GetCurrentDisplayMode Error: " << SDL_GetError() << std::endl;
		SDL_Quit();
		return 1;
	}

#if defined(IMGUI_IMPL_OPENGL_ES2)
	// Here we can do specific things for the Raspberry Pi
	// and other low power devices for increasing FPS, such as
	// switch display mode to 1200x1000
#endif

    window = SDL_CreateWindow(_MAINWINDOWNAME, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		displayMode.w, displayMode.h, window_flags);
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);

    // Initialize GLAD
    if (!initialize_glad()) {
        SDL_GL_DeleteContext(gl_context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

	while ((glerr = glGetError()) != GL_NO_ERROR) {
        // reset and clear error
		std::cerr << "gladLoadGL error: " << glerr << std::endl;
	}
	
#ifdef DEBUG
	if (GLAD_GL_KHR_debug) {
		glDebugMessageCallback(DebugCallbackKHR, nullptr);
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
		glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
	} else {
		std::cerr << "GL_KHR_debug not supported." << std::endl;
	}
#endif
	
    // glEnable(GL_DEPTH_TEST); // TODO: Check if necessary
    glEnable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	if ((glerr = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL glEnable error: " << glerr << std::endl;
	}

    // Our state
	static MemoryEditor mem_edit_a2e;
	static MemoryEditor mem_edit_upload;

	mem_edit_a2e.Open = false;
	mem_edit_upload.Open = false;

	static bool bShouldTerminateNetworking = false;
	static bool bShouldTerminateProcessing = false;
    bool show_demo_window = false;
    bool show_metrics_window = false;
	bool show_texture_window = false;
	bool show_a2video_window = true;
	bool show_postprocessing_window = false;
	bool show_recorder_window = false;
	int _slotnum = 0;
	int vbl_region = 2;		// Default to NTSC. 0 is auto, 1 is PAL, 2 is NTSC
	int vbl_slider_val;

	// Get the instances of all singletons before creating threads
	// This ensures thread safety
	// The OpenGLHelper instance is already acquired
	auto memManager = MemoryManager::GetInstance();
	auto sdhrManager = SDHRManager::GetInstance();
    auto a2VideoManager = A2VideoManager::GetInstance();
	auto postProcessor = PostProcessor::GetInstance();
	auto eventRecorder = EventRecorder::GetInstance();
	auto cycleCounter = CycleCounter::GetInstance();
	auto soundManager = SoundManager::GetInstance();
	auto mockingboardManager = MockingboardManager::GetInstance();

	std::cout << "Renderer Initializing..." << std::endl;
	while (!a2VideoManager->IsReady())
	{
		// Wait for shaders to compile
	}
	std::cout << "Renderer Ready!" << std::endl;

	// Run the network thread that will update the internal state as well as the apple 2 memory
	std::thread thread_server(socket_server_thread, (uint16_t)_SDHR_SERVER_PORT, &bShouldTerminateNetworking);
    // And run the processing thread
	std::thread thread_processor(process_events_thread, &bShouldTerminateProcessing);

    // Delta Time
	uint64_t dt_NOW = SDL_GetPerformanceCounter();
    uint64_t dt_LAST = 0;
	float deltaTime = 0.f;

	Main_SetVsync(g_swapInterval);

	uint32_t lastMouseMoveTime = SDL_GetTicks();
	const uint32_t cursorHideDelay = 3000; // After this delay, the mouse cursor disappears

    // Main loop
    bool done = false;
	GLuint out_tex_id = 0;
	
	// Get the saved states from previous runs
	std::cout << "Loading previous state..." << std::endl;
	nlohmann::json settingsState;
	std::ifstream inFile("Settings.json");
	if (inFile.is_open()) {
		inFile >> settingsState;
		inFile.close();
		if (settingsState.contains("Post Processor")) {
			postProcessor->DeserializeState(settingsState["Post Processor"]);
		}
		if (settingsState.contains("Apple 2 Video")) {
			a2VideoManager->DeserializeState(settingsState["Apple 2 Video"]);
		}
		if (settingsState.contains("Sound")) {
			soundManager->DeserializeState(settingsState["Sound"]);
		}
		if (settingsState.contains("Mockingboard")) {
			mockingboardManager->DeserializeState(settingsState["Mockingboard"]);
		}
		if (settingsState.contains("Main")) {
			int _wx, _wy, _ww, _wh;
			SDL_GetWindowPosition(window, &_wx, &_wy);
			SDL_GetWindowSize(window, &_ww, &_wh);
			auto _sm = settingsState["Main"];
			int _displayIndex = _sm.value("display index", 0);
			_wx = _sm.value("window x", _wx);
			_wy = _sm.value("window y", _wy);
			_ww = _sm.value("window width", _ww);
			_wh = _sm.value("window height", _wh);
			g_swapInterval = _sm.value("vsync", g_swapInterval);
			Main_SetVsync(g_swapInterval);
			Main_SetFullScreen(_sm.value("fullscreen", false));
			vbl_region = _sm.value("videoregion", vbl_region);
			if (vbl_region == 0)
			{
				cycleCounter->isVideoRegionDynamic = true;
			}
			else {
				cycleCounter->isVideoRegionDynamic = false;
				cycleCounter->SetVideoRegion(vbl_region == 1 ? VideoRegion_e::PAL : VideoRegion_e::NTSC);
			}
			if (_sm.value("show F1 window", true))
				Main_ToggleImGui(gl_context);
			show_a2video_window = _sm.value("show Apple 2 Video window", show_a2video_window);
			show_postprocessing_window = _sm.value("show Post Processor window", show_postprocessing_window);
			show_recorder_window = _sm.value("show Recorder window", show_recorder_window);
			show_texture_window = _sm.value("show texture window", show_texture_window);
			show_metrics_window = _sm.value("show metrics window", show_metrics_window);
			if (_sm.contains("window background color") && _sm["window background color"].is_array()) {
				for (size_t i = 0; i < 4; ++i) {
					window_bgcolor[i] = _sm["window background color"][i].get<float>();
				}
			}
			// update the main window accordingly
			SDL_Rect displayBounds;
			if (SDL_GetDisplayBounds(_displayIndex, &displayBounds) == 0) {
				if ((_wx < (displayBounds.x + displayBounds.w)) && (_wy < (displayBounds.y + displayBounds.h)))
					SDL_SetWindowPosition(window, _wx, _wy);
				SDL_SetWindowSize(window, _ww, _wh);
			}
		}
	} else {
		std::cerr << "No saved Settings.json file" << std::endl;
	}
	
	std::cout << "Previous state loaded!" << std::endl;

	// Load up the first screen in SHR, with green border color
	Main_DisplaySplashScreen();

	SDL_GetWindowSize(window, &_M8DBG_windowWidth, &_M8DBG_windowHeight);

	if (_M8DBG_bDisplayFPSOnScreen)
		Main_DrawFPSOverlay();
		
    while (!done)
	{
		// Check if we should reboot
		if (a2VideoManager->bShouldReboot)
		{
			std::cerr << "Reset detected" << std::endl;
			a2VideoManager->bShouldReboot = false;
			a2VideoManager->ResetComputer();
		}

        dt_LAST = dt_NOW;
        dt_NOW = SDL_GetPerformanceCounter();
		deltaTime = 1000.f * (float)((dt_NOW - dt_LAST) / (float)SDL_GetPerformanceFrequency());

		if (!eventRecorder->IsInReplayMode())
			eventRecorder->StartReplay();

        SDL_Event event;
		while (SDL_PollEvent(&event))
        {
			if (Main_IsImGuiOn())
			{
				// handled in imgui
				if (menu->HandleEvent(event))
					continue;
			}
            switch (event.type) {
            case SDL_QUIT:
				done = true;
                break;
            case SDL_WINDOWEVENT:
			{
				if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
					int width = event.window.data1;
					int height = event.window.data2;
					glViewport(0, 0, width, height);
				}
				if (event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))
					done = true;
			}
                break;
            case SDL_MOUSEMOTION:
				lastMouseMoveTime = SDL_GetTicks();
                if (event.motion.state & SDL_BUTTON_RMASK) {
                    // Move the camera when the right mouse button is pressed while moving the mouse
                    sdhrManager->camera.ProcessMouseMovement((float)event.motion.xrel, (float)event.motion.yrel);
                }
                break;
            case SDL_MOUSEWHEEL:
				sdhrManager->camera.ProcessMouseScroll((float)event.wheel.y);
                break;
            case SDL_KEYDOWN:
			{
				if (event.key.keysym.sym == SDLK_c) {  // Quit on Ctrl-c
					if (SDL_GetModState() & KMOD_CTRL) {
						done = true;
						break;
					}
				}
				else if (event.key.keysym.sym == SDLK_F1) {  // Toggle ImGUI with F1
					Main_ToggleImGui(gl_context);
				}
				else if (event.key.keysym.sym == SDLK_F2) {
					show_postprocessing_window = !show_postprocessing_window;
				}
				else if (event.key.keysym.sym == SDLK_F3) {
					show_a2video_window = !show_a2video_window;
				}
				else if (event.key.keysym.sym == SDLK_F8) {
					_M8DBG_bShowF8Window = !_M8DBG_bShowF8Window;
				}
				// Handle fullscreen toggle for Alt+Enter
				else if (event.key.keysym.sym == SDLK_RETURN && (event.key.keysym.mod & KMOD_ALT)) {
					Main_SetFullScreen(!Main_IsFullScreen());
				}
				// Camera movement!
				switch (event.key.keysym.sym)
				{
				case SDLK_w:
					sdhrManager->camera.ProcessKeyboard(FORWARD, deltaTime);
					break;
				case SDLK_s:
					sdhrManager->camera.ProcessKeyboard(BACKWARD, deltaTime);
					break;
				case SDLK_a:
					sdhrManager->camera.ProcessKeyboard(LEFT, deltaTime);
					break;
				case SDLK_d:
					sdhrManager->camera.ProcessKeyboard(RIGHT, deltaTime);
					break;
				case SDLK_q:
					sdhrManager->camera.ProcessKeyboard(CLIMB, deltaTime);
					break;
				case SDLK_z:
					sdhrManager->camera.ProcessKeyboard(DESCEND, deltaTime);
					break;
				default:
					break;
				};
			}
                break;
            default:
                break;
            }   // switch event.type
			
        }   // while SDL_PollEvent

		if (!_M8DBG_bDisableVideoRender)
		{
			if (sdhrManager->IsSdhrEnabled())
				out_tex_id = sdhrManager->Render();
			else
				out_tex_id = a2VideoManager->Render();
		}

		if (out_tex_id == UINT32_MAX)
			std::cerr << "ERROR: NO RENDERER OUTPUT!" << std::endl;
		
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		
		glClearColor(
			window_bgcolor[0], 
			window_bgcolor[1], 
			window_bgcolor[2],
			window_bgcolor[3]);
		glClear(GL_COLOR_BUFFER_BIT);

		if (!_M8DBG_bDisablePPRender)
			postProcessor->Render(window, out_tex_id);

		// Disable mouse if unused after cursorHideDelay
		// It's possible that the cursor won't get disabled when in windowed mode
		// (MacOS doesn't allow this, for example)
		if ((SDL_GetTicks() - lastMouseMoveTime) > cursorHideDelay)
			SDL_ShowCursor(SDL_DISABLE);
		else
			SDL_ShowCursor(SDL_ENABLE);
		
		if (Main_IsImGuiOn())
			menu->Render();
		
		/*
		if (false)
		{
			// 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
			if (show_demo_window)
				ImGui::ShowDemoWindow(&show_demo_window);

			ImGui::Begin("Super Duper Display", &show_F1_window);
			if (!ImGui::IsWindowCollapsed())
			{
				ImGui::PushItemWidth(110);
				ImGui::Text("Press F1 at any time to toggle the GUI");
				ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
				ImGui::Text("Worst Frame rate %.3f ms/frame", 1000.0f / fps_worst);
				int _vw, _vh;
				SDL_GL_GetDrawableSize(window, &_vw, &_vh);
				ImGui::Text("Drawable Size: %d x %d", _vw, _vh);
				ImGui::Text("A2 Screen Size: %d x %d", a2VideoManager->ScreenSize().x, a2VideoManager->ScreenSize().y);
				ImGui::Separator();
				ImGui::Text("Packet Pool Count: %lu", get_packet_pool_count());
				ImGui::Text("Max Incoming Packet Queue: %lu", get_max_incoming_packets());
				ImGui::Text("Network Processing Time: %llu ns", get_duration_network_processing_ns());
				ImGui::Text("Packets Processing Time: %llu ns", get_duration_packet_processing_ns());
				ImGui::Separator();
				ImGui::Text("Region: ");  ImGui::SameLine();
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
					vbl_slider_val = cycleCounter->GetScreenCycles();
					if (ImGui::InputInt("VBL Start Shift", &vbl_slider_val, 1, (CYCLES_TOTAL_PAL-CYCLES_TOTAL_NTSC)/10))
					{
						cycleCounter->SetVBLStart(vbl_slider_val);
					}
				}
				ImGui::Separator();
				ImGui::PushItemWidth(140);
				if (ImGui::ColorEdit4("Window Color", window_bgcolor)) {
					// std::cerr << "color " << window_bgcolor[0] << std::endl;
				}
				ImGui::PopItemWidth();

				ImGui::Checkbox("PostProcessing Window (F2)", &show_postprocessing_window);
				ImGui::Checkbox("Apple 2 Video Modes Window (F3)", &show_a2video_window);
				ImGui::Checkbox("KFest Window (F8)", &_M8DBG_bShowF8Window);
				ImGui::Checkbox("Fullscreen (F11 or Alt-Enter)", &bIsFullscreen);
				if (ImGui::Checkbox("VSYNC", &g_swapInterval))
				{
					set_vsync(g_swapInterval);
					Main_ResetFPSCalculations();
				}
				if (g_swapInterval)
				{
					ImGui::SameLine();
					ImGui::Text("On");
					if (g_adaptiveVsync)
					{
						ImGui::SameLine();
						ImGui::Text("(Adaptive)");
					}
				}
				ImGui::Separator();
				if (ImGui::Button("Reset")) {
					a2VideoManager->ResetComputer();
					Main_DisplaySplashScreen();
				}
				if (ImGui::Button("Quit App (Ctrl-c)"))
					done = true;
				if (ImGui::CollapsingHeader("Other Windows"))
				{
					ImGui::Checkbox("Event Recorder Window", &show_recorder_window);
					ImGui::Checkbox("Textures Window", &show_texture_window);
					ImGui::Checkbox("Apple //e Memory Window", &mem_edit_a2e.Open);
					ImGui::Checkbox("ImGui Metrics Window", &show_metrics_window);
					// ImGui::Checkbox("ImGui Demo Window", &show_demo_window);
				}
				if (ImGui::CollapsingHeader("SDHR"))
				{
					auto _c = sdhrManager->camera;
					auto _pos = _c.Position;
					ImGui::Text("Camera X:%.2f Y:%.2f Z:%.2f", _pos.x, _pos.y, _pos.z);
					ImGui::Text("Camera Pitch:%.2f Yaw:%.2f Zoom:%.2f", _c.Pitch, _c.Yaw, _c.Zoom);
					ImGui::Checkbox("Untextured Geometry", &sdhrManager->bDebugNoTextures);         // Show textures toggle
					ImGui::Checkbox("Perspective Projection", &sdhrManager->bUsePerspective);       // Change projection type
					ImGui::Checkbox("SDHR Upload Region Memory Window", &mem_edit_upload.Open);
				}
				ImGui::PopItemWidth();
			}
			ImGui::End();
			
			// Show the postprocessing window
			if (show_postprocessing_window)
				postProcessor->DisplayImGuiWindow(&show_postprocessing_window);

			// Show the a2VideoManager window
			if (show_a2video_window)
				a2VideoManager->DisplayImGuiWindow(&show_a2video_window);

			// The VCR event recorder
			if (show_recorder_window)
				eventRecorder->DisplayImGuiWindow(&show_recorder_window);

			// Show the metrics window
			if (show_metrics_window)
				ImGui::ShowMetricsWindow(&show_metrics_window);

			// Show the Apple //e memory
			if (mem_edit_a2e.Open)
			{
				mem_edit_a2e.DrawWindow("Memory Editor: Apple 2 Memory (0000-C000 x2)", memManager->GetApple2MemPtr(), 2 * _A2_MEMORY_SHADOW_END);
			}

			// Show the upload data region memory
			if (mem_edit_upload.Open)
			{
				mem_edit_upload.DrawWindow("Memory Editor: Upload memory", memManager->GetApple2MemPtr(), 2 * _A2_MEMORY_SHADOW_END);
			}

			// Show the 16 textures loaded (which are always bound to GL_TEXTURE2 -> GL_TEXTURE18)
			if (show_texture_window)
			{
				ImGui::Begin("Texture Viewer", &show_texture_window);
				ImVec2 avail_size = ImGui::GetContentRegionAvail();
				ImGui::SliderInt("Texture Slot Number", &_slotnum, 0, _SDHR_MAX_TEXTURES + 1, "slot %d", ImGuiSliderFlags_AlwaysClamp);
				GLint _w, _h;
				if (_slotnum < _SDHR_MAX_TEXTURES)
				{
					glBindTexture(GL_TEXTURE_2D, glhelper->get_texture_id_at_slot(_slotnum));
					glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &_w);
					glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &_h);
					ImGui::Text("Texture ID: %d (%d x %d)", (int)glhelper->get_texture_id_at_slot(_slotnum), _w, _h);
					ImGui::Image((void*)glhelper->get_texture_id_at_slot(_slotnum),
						ImVec2(avail_size.x, avail_size.y - 30), ImVec2(0, 0), ImVec2(1, 1));
				}
				else if (_slotnum == _SDHR_MAX_TEXTURES)
				{
					glBindTexture(GL_TEXTURE_2D, a2VideoManager->GetOutputTextureId());
					glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &_w);
					glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &_h);
					ImGui::Text("Output Texture ID: %d (%d x %d)", (int)a2VideoManager->GetOutputTextureId(), _w, _h);
					ImGui::Image((void*)a2VideoManager->GetOutputTextureId(), avail_size, ImVec2(0, 0), ImVec2(1, 1));
				}
				else if (_slotnum == _SDHR_MAX_TEXTURES + 1)
				{
					glActiveTexture(_PP_INPUT_TEXTURE_UNIT);
					GLint target_tex_id = 0;
					glGetIntegerv(GL_TEXTURE_BINDING_2D, &target_tex_id);
					glActiveTexture(GL_TEXTURE0);
					glBindTexture(GL_TEXTURE_2D, target_tex_id);
					glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &_w);
					glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &_h);
					ImGui::Text("_PP_INPUT_TEXTURE_UNIT: %d (%d x %d)", target_tex_id, _w, _h);
					ImGui::Image((void*)target_tex_id, avail_size, ImVec2(0, 0), ImVec2(1, 1));
				}
				glBindTexture(GL_TEXTURE_2D, 0);
				ImGui::End();
			}

			if (_M8DBG_bShowF8Window)
			{
				ImGui::Begin("KFest 2024", &_M8DBG_bShowF8Window);
				if (!ImGui::IsWindowCollapsed())
				{
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
					ImGui::PushItemWidth(80);
					ImGui::InputInt("Width", &_M8DBG_windowWidth, 10, 100); ImGui::SameLine();
					ImGui::InputInt("Height", &_M8DBG_windowHeight, 10, 100); ImGui::SameLine();
					if (ImGui::Button("Apply"))
					{
						SDL_SetWindowSize(window, _M8DBG_windowWidth, _M8DBG_windowHeight);
					}
					ImGui::Separator();
					ImGui::PopItemWidth();
					ImGui::PushItemWidth(110);
					if (ImGui::Checkbox("Display FPS on screen", &_M8DBG_bDisplayFPSOnScreen))
					{
						Main_ResetFPSCalculations();
						Main_DrawFPSOverlay();
						a2VideoManager->ForceBeamFullScreenRender();
					}
					ImGui::Dummy(ImVec2(20, 1)); ImGui::SameLine();
					if (ImGui::Checkbox("Without Sine Wobble", &a2VideoManager->bNoMergedModeWobble))
						a2VideoManager->ForceBeamFullScreenRender();
					ImGui::SliderFloat("Average FPS range (s)", &_M8DBG_average_fps_window, 0.1f, 10.f, "%.1f");
					if (ImGui::Button("Reset FPS numbers"))
						Main_ResetFPSCalculations();
					ImGui::Separator();
					if (ImGui::Button("Reset A2SS")) {
						ResetA2SS();
						a2VideoManager->ForceBeamFullScreenRender();
					}
					ImGui::SameLine();
					_m8ssSHR = memManager->IsSoftSwitch(A2SS_SHR);
					if (ImGui::Checkbox("A2SS_SHR##M8", &_m8ssSHR)) {
						memManager->SetSoftSwitch(A2SS_SHR, _m8ssSHR);
						Main_ResetFPSCalculations();
						a2VideoManager->ForceBeamFullScreenRender();
					}
					ImGui::Separator();
					
					if (ImGui::Checkbox("Disable Apple 2 Video render", &_M8DBG_bDisableVideoRender))
						ResetFPSCalculations(a2VideoManager);
					if (ImGui::Checkbox("Disable PostProcessing render", &_M8DBG_bDisablePPRender))
						ResetFPSCalculations(a2VideoManager);
					if (ImGui::Checkbox("Force render even if VRAM unchanged", &a2VideoManager->bAlwaysRenderBuffer))
						ResetFPSCalculations(a2VideoManager);
					 
					if (ImGui::Checkbox("VSYNC##M8", &g_swapInterval))
					{
						set_vsync(g_swapInterval);
						Main_ResetFPSCalculations();
						a2VideoManager->ForceBeamFullScreenRender();
					}
					if (g_swapInterval)
					{
						ImGui::SameLine();
						ImGui::Text("On");
						if (g_adaptiveVsync)
						{
							ImGui::SameLine();
							ImGui::Text("(Adaptive)");
						}
					}
					if (ImGui::Button("Run Vertical Refresh"))
						a2VideoManager->ForceBeamFullScreenRender();
					ImGui::SameLine();
					ImGui::Text("Frame ID: %d", a2VideoManager->GetVRAMReadId());
					ImGui::SeparatorText("[ SAMPLES ]");
					if (ImGui::Checkbox("Run Karateka Demo", &_M8DBG_bRunKarateka))
					{
						if (_M8DBG_bRunKarateka)
						{
							std::ifstream karatekafile("./recordings/karateka.vcr", std::ios::binary);
							if (!karatekafile.is_open()) {
								_M8DBG_bKaratekaLoadFailed = true;
							}
							else {
								ResetA2SS();
								memManager->SetSoftSwitch(A2SS_SHR, false);
								eventRecorder->ReadRecordingFile(karatekafile);
								eventRecorder->StartReplay();
								memManager->SetSoftSwitch(A2SS_TEXT, false);
								memManager->SetSoftSwitch(A2SS_HIRES, true);
								a2VideoManager->ForceBeamFullScreenRender();
							}
						}
						else {
							eventRecorder->StopReplay();
						}
						Main_ResetFPSCalculations();
						a2VideoManager->ForceBeamFullScreenRender();
					}
					if (ImGui::Button("DHGR Col 140 Mixed"))
					{
						ResetA2SS();
						memManager->SetSoftSwitch(A2SS_SHR, false);
						memManager->SetSoftSwitch(A2SS_TEXT, false);
						memManager->SetSoftSwitch(A2SS_80COL, true);
						memManager->SetSoftSwitch(A2SS_HIRES, true);
						memManager->SetSoftSwitch(A2SS_DHGR, true);
						a2VideoManager->bUseDHGRCOL140Mixed = true;
						MemoryLoadDHR("scripts/extasie0_140mix.dhr");
						a2VideoManager->ForceBeamFullScreenRender();
					}
					if (ImGui::Button("HGR SPEC1"))
					{
						ResetA2SS();
						memManager->SetSoftSwitch(A2SS_SHR, false);
						memManager->SetSoftSwitch(A2SS_TEXT, false);
						memManager->SetSoftSwitch(A2SS_HIRES, true);
						a2VideoManager->bUseHGRSPEC1 = true;
						MemoryLoadHGR("scripts/arcticfox.hgr");
						a2VideoManager->ForceBeamFullScreenRender();
					}
					if (ImGui::Button("\"LEGASHR\" Wobbly"))
					{
						ResetA2SS();
						memManager->SetSoftSwitch(A2SS_SHR, true);
						MemoryLoadSHR("scripts/paintworks.shr");
						std::ifstream legacydemo("./scripts/tomahawk2_hgr.bin", std::ios::binary);
						legacydemo.seekg(0, std::ios::beg); // Go back to the start of the file
						legacydemo.read(reinterpret_cast<char*>(MemoryManager::GetInstance()->GetApple2MemPtr()), 0x4000);
						a2VideoManager->bDEMOMergedMode = true;
						a2VideoManager->ForceBeamFullScreenRender();
					}
					
					ImGui::Separator();
					ImGui::Text("Legacy Shader");
					const char* _legshaders[] = { "0 - Full" };
					static int _legshader_current = 0;
					if (ImGui::ListBox("##LegacyShader", &_legshader_current, _legshaders, IM_ARRAYSIZE(_legshaders), 4))
					{
						a2VideoManager->SelectLegacyShader(_legshader_current);
						ResetFPSCalculations(a2VideoManager);
						a2VideoManager->ForceBeamFullScreenRender();
					}
					ImGui::Text("SHR Shader");
					const char* _shrshaders[] = { "0 - Full" };
					static int _shrshader_current = 0;
					if (ImGui::ListBox("##SHRShader", &_shrshader_current, _shrshaders, IM_ARRAYSIZE(_shrshaders), 3))
					{
						a2VideoManager->SelectSHRShader(_shrshader_current);
						ResetFPSCalculations(a2VideoManager);
						a2VideoManager->ForceBeamFullScreenRender();
					}
					ImGui::PopItemWidth();
					 

				}

				if (_M8DBG_bKaratekaLoadFailed)
				{
					ImGui::OpenPopup("File Loading Error");
					if (ImGui::BeginPopupModal("File Loading Error", NULL, ImGuiWindowFlags_AlwaysAutoResize))
					{
						ImGui::Text("Failed to open the file ./recordings/karateka.vcr");
						if (ImGui::Button("OK", ImVec2(120, 0))) {
							ImGui::CloseCurrentPopup();
							_M8DBG_bKaratekaLoadFailed = false;
							_M8DBG_bRunKarateka = false;
						}
						ImGui::EndPopup();
					}
				}
				ImGui::End();
			}

			// Rendering
			ImGui::Render();

			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		}	// show F1 window
		*/

		SDL_GL_SwapWindow(window);

		// FPS overlay - Calculate frame rates
		fps_frame_count++;
		uint32_t currentTime = SDL_GetTicks();
		uint32_t elapsedTime = currentTime - fps_start_time;
		// Calculate frame rate every second
		if (elapsedTime > (_M8DBG_average_fps_window * 1000))
		{
			float fps = fps_frame_count / (elapsedTime / 1000.0f);
			if ((fps_worst > fps) && (fps > 0))
				fps_worst = fps;

			//if (false)
			if (_M8DBG_bDisplayFPSOnScreen)
			{
				snprintf(fps_str_buf, 10,  "%.0f ", fps);
				a2VideoManager->EraseOverlayRange(6, 13, 0);
				a2VideoManager->DrawOverlayString(fps_str_buf, 10, 0b11010010, 13, 0);
				snprintf(fps_str_buf, 10, "%.0f ", fps_worst);
				a2VideoManager->EraseOverlayRange(6, 13, 1);
				a2VideoManager->DrawOverlayString(fps_str_buf, 10, 0b10010010, 13, 1);
			}


			// Reset for next calculation
			fps_start_time = currentTime;
			fps_frame_count = 0;
		}

		if ((glerr = glGetError()) != GL_NO_ERROR) {
			std::cerr << "OpenGL end of render error: " << glerr << std::endl;
		}
    }

    // Stop all threads
	bShouldTerminateProcessing = true;
	terminate_processing_thread();
	thread_processor.join();
    bShouldTerminateNetworking = true;
    thread_server.join();

	eventRecorder->StopReplay();
	soundManager->StopPlay();

	// Serialize settings and save them
	{
		int _wx, _wy, _ww, _wh;
		SDL_GetWindowPosition(window, &_wx, &_wy);
		SDL_GetWindowSize(window, &_ww, &_wh);
		settingsState["Post Processor"] = postProcessor->SerializeState();
		settingsState["Apple 2 Video"] = a2VideoManager->SerializeState();
		settingsState["Sound"] = soundManager->SerializeState();
		settingsState["Mockingboard"] = mockingboardManager->SerializeState();
		settingsState["Main"] = {
			{"display index", SDL_GetWindowDisplayIndex(window)},
			{"window x", _wx},
			{"window y", _wy},
			{"window width", _ww},
			{"window height", _wh},
			{"fullscreen", Main_IsFullScreen()},
			{"vsync", g_swapInterval},
			{"videoregion", vbl_region},
			{"window background color", window_bgcolor},
			{"show F1 window", Main_IsImGuiOn()},
			{"show Apple 2 Video window", show_a2video_window},
			{"show Post Processor window", show_postprocessing_window},
			{"show Recorder window", show_recorder_window},
			{"show texture window", show_texture_window},
			{"show metrics window", show_metrics_window},
		};
		std::ofstream outFile("Settings.json");
		if (outFile.is_open()) {
			outFile << settingsState.dump(4);	// 4 spaces indent
			outFile.close();
		} else {
			std::cerr << "Unable to save Settings.json file" << std::endl;
		}
	}
	
    // Cleanup
	delete menu;
	
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
