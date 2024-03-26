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
// This example can also compile and run with Emscripten! See 'Makefile.emscripten' for details.
#ifdef __EMSCRIPTEN__
#include "../libs/emscripten/emscripten_mainloop_stub.h"
#endif

#include <cstring>
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
#include "extras/MemoryLoader.h"
#include "PostProcessor.h"
#include "EventRecorder.h"

#if defined(__NETWORKING_APPLE__) || defined (__NETWORKING_LINUX__)
#include <unistd.h>
#include <libgen.h>
#endif

static uint32_t fbWidth = 0;
static uint32_t fbHeight = 0;
static bool g_swapInterval = true;  // VSYNC
static bool g_adaptiveVsync = true;	
static SDL_Window* window;

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

void set_vsync(bool _on)
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
        | SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED | SDL_WINDOW_ALLOW_HIGHDPI
        | SDL_WINDOW_SHOWN);
#elif defined(IMGUI_IMPL_OPENGL_ES2)
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL 
		| SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_SHOWN);
#else
	SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL 
        | SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED | SDL_WINDOW_ALLOW_HIGHDPI
        | SDL_WINDOW_SHOWN);
#endif
	// Get the actual display size
	SDL_DisplayMode displayMode;
	if (SDL_GetDesktopDisplayMode(0, &displayMode) != 0) {
		std::cerr << "SDL_GetDesktopDisplayMode Error: " << SDL_GetError() << std::endl;
		SDL_Quit();
		return 1;
	}
    window = SDL_CreateWindow(_MAINWINDOWNAME, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
		displayMode.w, displayMode.h, window_flags);
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glhelper->get_glsl_version()->c_str());

	if (!gladLoadGL()) {
		std::cout << "Failed to initialize OpenGL context" << std::endl;
		return -1;
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

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return a nullptr. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher quality font rendering.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    // - Our Emscripten build process allows embedding fonts to be accessible at runtime from the "fonts/" folder. See Makefile.emscripten for details.
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf", 18.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());
    //IM_ASSERT(font != nullptr);

    io.Fonts->AddFontFromFileTTF("./assets/BerkeliumIIHGR.ttf", 10.0f);

    // Our state
	static MemoryEditor mem_edit_a2e;
	static MemoryEditor mem_edit_upload;
	static MemoryEditor mem_edit_vram_legacy;
	static MemoryEditor mem_edit_vram_shr;

	mem_edit_a2e.Open = false;
	mem_edit_upload.Open = false;
	mem_edit_vram_legacy.Open = false;
	mem_edit_vram_shr.Open = false;

	static bool bShouldTerminateNetworking = false;
	static bool bShouldTerminateProcessing = false;
    bool show_demo_window = false;
    bool show_metrics_window = false;
	bool show_F1_window = true;
	bool show_texture_window = false;
	bool show_postprocessing_window = false;
	bool show_recorder_window = false;
	int _slotnum = 0;
	bool mem_load_aux_bank = false;
	int mem_load_position = 0;
	bool vbl_region_is_PAL;
	int vbl_slider_val;
	float window_bgcolor[4] = { 0.0f, 0.0f, 0.0f, 1.0f }; // RGBA

	// Get the instances of all singletons before creating threads
	// This ensures thread safety
	// The OpenGLHelper instance is already acquired
	auto memManager = MemoryManager::GetInstance();
	auto sdhrManager = SDHRManager::GetInstance();
    auto a2VideoManager = A2VideoManager::GetInstance();
	auto postProcessor = PostProcessor::GetInstance();
	auto eventRecorder = EventRecorder::GetInstance();
	auto cycleCounter = CycleCounter::GetInstance();

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

	set_vsync(g_swapInterval);

    // Main loop
    bool done = false;
	GLuint out_tex_id = 0;
	
#ifdef __EMSCRIPTEN__
    // For an Emscripten build we are disabling file-system access, so let's not attempt to do a fopen() of the imgui.ini file.
    // You may manually call LoadIniSettingsFromMemory() to load settings from your own storage.
    io.IniFilename = nullptr;
    EMSCRIPTEN_MAINLOOP_BEGIN
#else

	// Run a refresh to show the first screen
	a2VideoManager->ForceBeamFullScreenRender();

    while (!done)
#endif
    {
		// Check if we should reboot
		if (a2VideoManager->bShouldReboot)
		{
			std::cerr << "Reset detected" << std::endl;
			a2VideoManager->bShouldReboot = false;
			a2VideoManager->ResetComputer();
		}

		// Beam renderer does not use VSYNC. It synchronizes to the Apple 2's VBL.
//		if (!(a2VideoManager->ShouldRender() || sdhrManager->IsSdhrEnabled()))
//			continue;

        dt_LAST = dt_NOW;
        dt_NOW = SDL_GetPerformanceCounter();
		deltaTime = 1000.f * (float)((dt_NOW - dt_LAST) / (float)SDL_GetPerformanceFrequency());

        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        SDL_Event event;
		while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL2_ProcessEvent(&event);
            switch (event.type) {
            case SDL_QUIT:
				done = true;
                break;
            case SDL_WINDOWEVENT:
			{
				if (event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))
					done = true;
			}
                break;
            case SDL_MOUSEMOTION:
                if (event.motion.state & SDL_BUTTON_RMASK && !io.WantCaptureMouse) {
                    // Move the camera when the right mouse button is pressed while moving the mouse
                    sdhrManager->camera.ProcessMouseMovement((float)event.motion.xrel, (float)event.motion.yrel);
                }
                break;
            case SDL_MOUSEWHEEL:
				if (!io.WantCaptureMouse) {
					sdhrManager->camera.ProcessMouseScroll((float)event.wheel.y);
				}
                break;
            case SDL_KEYDOWN:
			{
				if (event.key.keysym.sym == SDLK_c) {  // Quit on Ctrl-c
					auto state = SDL_GetKeyboardState(NULL);
					if (state[SDL_SCANCODE_LCTRL]) {
						done = true;
						break;
					}
				}
				else if (event.key.keysym.sym == SDLK_F1) {  // Toggle debug window with F1
					show_F1_window = !show_F1_window;
				}
				// Camera movement!
				if (!io.WantCaptureKeyboard) {
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
			}
                break;
            default:
                break;
            }   // switch event.type
        }   // while SDL_PollEvent
		
        if (sdhrManager->IsSdhrEnabled())
			out_tex_id = sdhrManager->Render();
        else
			out_tex_id = a2VideoManager->Render();

		if (out_tex_id == UINT32_MAX)
			std::cerr << "ERROR: NO RENDERER OUTPUT!" << std::endl;
		
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		
		glClearColor(
			window_bgcolor[0], 
			window_bgcolor[1], 
			window_bgcolor[2],
			window_bgcolor[3]);
		glClear(GL_COLOR_BUFFER_BIT);

		postProcessor->Render(window, out_tex_id);

		// Start the Dear ImGui frame
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL2_NewFrame();
		ImGui::NewFrame();

		// 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
		if (show_demo_window)
			ImGui::ShowDemoWindow(&show_demo_window);

		if (show_F1_window)
		{
			ImGui::Begin("Super Duper Display", &show_F1_window);
			if (!ImGui::IsWindowCollapsed())
			{
				ImGui::PushItemWidth(110);
                ImGui::Text("Press F1 at any time to toggle this window");
                ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
				int _vw, _vh;
				SDL_GL_GetDrawableSize(window, &_vw, &_vh);
				ImGui::Text("Drawable Size: %d x %d", _vw, _vh);
				ImGui::Text("A2 Screen Size: %d x %d", a2VideoManager->ScreenSize().x, a2VideoManager->ScreenSize().y);
				ImGui::Separator();
				ImGui::Text("Packet Pool Count: %lu", get_packet_pool_count());
				ImGui::Text("Max Incoming Packet Queue: %lu", get_max_incoming_packets());
				ImGui::Text("Network Processing Time: %llu ns", get_duration_network_processing_ns());
				ImGui::Text("Packets Processing Time: %llu ns", get_duration_packet_processing_ns());
				vbl_region_is_PAL = (cycleCounter->GetVideoRegion() == VideoRegion_e::PAL);
				if (ImGui::Checkbox("PAL Mode", &vbl_region_is_PAL))
				{
					if (vbl_region_is_PAL)
						cycleCounter->SetVideoRegion(VideoRegion_e::PAL);
					else
						cycleCounter->SetVideoRegion(VideoRegion_e::NTSC);
				}
				vbl_slider_val = cycleCounter->GetScreenCycles();
				if (ImGui::SliderInt("Set VBL Start", &vbl_slider_val, 0, (int)cycleCounter->GetScreenCycles()))
				{
					cycleCounter->SetVBLStart(vbl_slider_val);
				}
				ImGui::Separator();
				if (ImGui::ColorEdit4("Window Color", window_bgcolor)) {
					// std::cerr << "color " << window_bgcolor[0] << std::endl;
				}
				ImGui::Checkbox("PostProcessing Window", &show_postprocessing_window);
				if (ImGui::Checkbox("VSYNC", &g_swapInterval))
					set_vsync(g_swapInterval);
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
				}
				if (ImGui::Button("Quit App (Ctrl-c)"))
					done = true;
				if (ImGui::CollapsingHeader("Windows"))
				{
					ImGui::Checkbox("Event Recorder Window", &show_recorder_window);
					ImGui::Checkbox("Textures Window", &show_texture_window);
					ImGui::Checkbox("Apple //e Memory Window", &mem_edit_a2e.Open);
					ImGui::Checkbox("VRAM Legacy Memory Window", &mem_edit_vram_legacy.Open);
					ImGui::Checkbox("VRAM SHR Memory Window", &mem_edit_vram_shr.Open);
					ImGui::Checkbox("SDHR Upload Region Memory Window", &mem_edit_upload.Open);
					ImGui::Checkbox("ImGui Metrics Window", &show_metrics_window);
					// ImGui::Checkbox("ImGui Demo Window", &show_demo_window);
				}
				if (ImGui::CollapsingHeader("Apple 2 Video"))
				{
					if (ImGui::Button("Run Vertical Refresh"))
						a2VideoManager->ForceBeamFullScreenRender();
					if (ImGui::SliderInt("Border Color (0xC034)", &memManager->switch_c034, 0, 15))
						a2VideoManager->ForceBeamFullScreenRender();
					ImGui::Text("Load Memory Start: ");
					ImGui::SameLine();
					ImGui::InputInt("##mem_load", &mem_load_position, 1, 1024, ImGuiInputTextFlags_CharsHexadecimal);
					mem_load_position = std::clamp(mem_load_position, 0, 0xFFFF);
					ImGui::SameLine();
					ImGui::Checkbox("Aux Bank", &mem_load_aux_bank);
					if (MemoryLoad(mem_load_position, mem_load_aux_bank))
						a2VideoManager->ForceBeamFullScreenRender();
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
				}
				if (ImGui::CollapsingHeader("SDHR"))
				{
					auto _c = sdhrManager->camera;
					auto _pos = _c.Position;
					ImGui::Text("Camera X:%.2f Y:%.2f Z:%.2f", _pos.x, _pos.y, _pos.z);
					ImGui::Text("Camera Pitch:%.2f Yaw:%.2f Zoom:%.2f", _c.Pitch, _c.Yaw, _c.Zoom);
					ImGui::Checkbox("Untextured Geometry", &sdhrManager->bDebugNoTextures);         // Show textures toggle
					ImGui::Checkbox("Perspective Projection", &sdhrManager->bUsePerspective);       // Change projection type
				}
				ImGui::PopItemWidth();
            }
			ImGui::End();
		}
		// Show the postprocessing window
		if (show_postprocessing_window)
			postProcessor->DisplayImGuiPPWindow(&show_postprocessing_window);

        // The VCR event recorder
		if (show_recorder_window)
			eventRecorder->DisplayImGuiRecorderWindow(&show_recorder_window);

        // Show the metrics window
        if (show_metrics_window)
			ImGui::ShowMetricsWindow(&show_metrics_window);

        // Show the Apple //e memory
        if (mem_edit_a2e.Open)
        {
            mem_edit_a2e.DrawWindow("Memory Editor: Apple 2 Memory (0000-C000 x2)", memManager->GetApple2MemPtr(), 2*_A2_MEMORY_SHADOW_END);
        }

		// Show the VRAM legacy window
		if (mem_edit_vram_legacy.Open)
		{
			mem_edit_vram_legacy.DrawWindow("Memory Editor: Beam VRAM Legacy", a2VideoManager->GetLegacyVRAMWritePtr(), _BEAM_VRAM_SIZE_LEGACY);
		}
		
		// Show the VRAM SHR window
		if (mem_edit_vram_shr.Open)
		{
			mem_edit_vram_shr.DrawWindow("Memory Editor: Beam VRAM SHR", a2VideoManager->GetSHRVRAMWritePtr(), _BEAM_VRAM_SIZE_SHR);
		}
		
		// Show the upload data region memory
		if (mem_edit_upload.Open)
		{
            mem_edit_upload.DrawWindow("Memory Editor: Upload memory", memManager->GetApple2MemPtr(), 2*_A2_MEMORY_SHADOW_END);
		}
        
		// Show the 16 textures loaded (which are always bound to GL_TEXTURE2 -> GL_TEXTURE18)
        if (show_texture_window)
		{
			ImGui::Begin("Texture Viewer", &show_texture_window);
			ImVec2 avail_size = ImGui::GetContentRegionAvail();
            ImGui::SliderInt("Texture Slot Number", &_slotnum, 0, _SDHR_MAX_TEXTURES+1, "slot %d", ImGuiSliderFlags_AlwaysClamp);
			GLint _w, _h;
			if (_slotnum < _SDHR_MAX_TEXTURES)
			{
				glBindTexture(GL_TEXTURE_2D, glhelper->get_texture_id_at_slot(_slotnum));
				glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &_w);
				glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &_h);
				ImGui::Text("Texture ID: %d (%d x %d)", (int)glhelper->get_texture_id_at_slot(_slotnum), _w, _h);
				ImGui::Image((void*)glhelper->get_texture_id_at_slot(_slotnum), avail_size, ImVec2(0, 0), ImVec2(1, 1));
			}
			else if (_slotnum == _SDHR_MAX_TEXTURES)
			{
				glBindTexture(GL_TEXTURE_2D, a2VideoManager->GetOutputTextureId());
				glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &_w);
				glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &_h);
				ImGui::Text("Output Texture ID: %d (%d x %d)", (int)a2VideoManager->GetOutputTextureId(), _w, _h);
				ImGui::Image((void*)a2VideoManager->GetOutputTextureId(), avail_size, ImVec2(0, 0), ImVec2(1, 1));
			}
			else if (_slotnum == _SDHR_MAX_TEXTURES+1)
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

		// Rendering
		ImGui::Render();

		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		SDL_GL_SwapWindow(window);

		if ((glerr = glGetError()) != GL_NO_ERROR) {
			std::cerr << "OpenGL end of render error: " << glerr << std::endl;
		}

    }
#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_END;
#endif

    // Stop all threads
	bShouldTerminateProcessing = true;
	terminate_processing_thread();
	thread_processor.join();
    bShouldTerminateNetworking = true;
    thread_server.join();

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
