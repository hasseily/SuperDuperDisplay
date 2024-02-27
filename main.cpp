// Dear ImGui: standalone example application for SDL2 + OpenGL
// (SDL is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)
// If you are new to Dear ImGui, read documentation from the docs/ folder + read the top of imgui.cpp.
// Read online: https://github.com/ocornut/imgui/tree/master/docs

#define IMGUI_USER_CONFIG "../my_imgui_config.h"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include "imgui_memory_editor.h"
#include <SDL.h>
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
static SDL_Window* window;

void callback_resolutionChange(int w, int h)
{
	auto glhelper = OpenGLHelper::GetInstance();
	auto sdhrManager = SDHRManager::GetInstance();
	auto a2videoManager = A2VideoManager::GetInstance();
	// In case the window was program-resized, tell SDL to change the window size
	glhelper->get_framebuffer_size(&fbWidth, &fbHeight);
	auto margins = (sdhrManager->IsSdhrEnabled()
		? sdhrManager->windowMargins
		: a2videoManager->windowMargins);
	SDL_DisplayMode displayMode;
	SDL_GetWindowDisplayMode(window, &displayMode);
    displayMode.w = w + 2 * margins;
    displayMode.h = h + 2 * margins;
    SDL_SetWindowDisplayMode(window, &displayMode);
}

// Main code
int main(int argc, char* argv[])
{
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
        | SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED
        | SDL_WINDOW_SHOWN);
#elif defined(IMGUI_IMPL_OPENGL_ES2)
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN);
#else
	SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL 
        | SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED
        | SDL_WINDOW_SHOWN);
#endif
    window = SDL_CreateWindow(_MAINWINDOWNAME, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1920, 1080, window_flags);
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);

    // Get the actual display size
	SDL_DisplayMode displayMode;
	SDL_GetCurrentDisplayMode(0, &displayMode);

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
		// std::cerr << "gladLoadGL error: " << glerr << std::endl;
	}
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
	
	mem_edit_a2e.Open = false;
	mem_edit_upload.Open = false;
	mem_edit_vram_legacy.Open = false;
	
	static bool bShouldTerminateNetworking = false;
	static bool bShouldTerminateProcessing = false;
    bool show_demo_window = false;
    bool show_metrics_window = false;
	bool show_sdhrinfo_window = false;
	bool show_texture_window = false;
	bool show_postprocessing_window = false;
	bool show_recorder_window = false;
    bool did_press_quit = false;
	int _slotnum = 0;
	bool mem_load_aux_bank = false;
	int mem_load_position = 0;

	// Get the instances of all singletons before creating threads
	// This ensures thread safety
	// The OpenGLHelper instance is already acquired
	auto sdhrManager = SDHRManager::GetInstance();
    auto a2VideoManager = A2VideoManager::GetInstance();
	auto postProcessor = PostProcessor::GetInstance();
	auto eventRecorder = EventRecorder::GetInstance();
	auto cycleCounter = CycleCounter::GetInstance();

	// Run the network thread that will update the internal state as well as the apple 2 memory
	std::thread thread_server(socket_server_thread, (uint16_t)_SDHR_SERVER_PORT, &bShouldTerminateNetworking);
    // And run the processing thread
	std::thread thread_processor(process_events_thread, &bShouldTerminateProcessing);

    // Delta Time
	uint64_t dt_NOW = SDL_GetPerformanceCounter();
    uint64_t dt_LAST = 0;
	float deltaTime = 0.f;

    glhelper->set_callback_changed_resolution(&callback_resolutionChange);

    // Main loop
    bool done = false;
#ifdef __EMSCRIPTEN__
    // For an Emscripten build we are disabling file-system access, so let's not attempt to do a fopen() of the imgui.ini file.
    // You may manually call LoadIniSettingsFromMemory() to load settings from your own storage.
    io.IniFilename = nullptr;
    EMSCRIPTEN_MAINLOOP_BEGIN
#else
    while (!done)
#endif
    {
		// Beam renderer does not use VSYNC. It synchronizes to the Apple 2's VBL.
		SDL_GL_SetSwapInterval(g_swapInterval && (!a2VideoManager->bShouldUseBeamRenderer));
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
                    glhelper->camera.ProcessMouseMovement(event.motion.xrel, event.motion.yrel);
                }
                break;
            case SDL_MOUSEWHEEL:
				if (!io.WantCaptureMouse) {
					glhelper->camera.ProcessMouseScroll(event.wheel.y);
				}
                break;
            case SDL_KEYDOWN:
			{
				if (event.key.keysym.sym == SDLK_c) {  // Quit on Ctrl-c
					auto state = SDL_GetKeyboardState(NULL);
					if (state[SDL_SCANCODE_LCTRL]) {
						done = true;
					}
				}
				else if (event.key.keysym.sym == SDLK_F1) {  // Toggle debug window with F1
					show_sdhrinfo_window = !show_sdhrinfo_window;
				}
				// Camera movement!
				if (!io.WantCaptureKeyboard) {
					switch (event.key.keysym.sym)
					{
					case SDLK_w:
						glhelper->camera.ProcessKeyboard(FORWARD, deltaTime);
						break;
					case SDLK_s:
						glhelper->camera.ProcessKeyboard(BACKWARD, deltaTime);
						break;
					case SDLK_a:
						glhelper->camera.ProcessKeyboard(LEFT, deltaTime);
						break;
					case SDLK_d:
						glhelper->camera.ProcessKeyboard(RIGHT, deltaTime);
						break;
					case SDLK_q:
						glhelper->camera.ProcessKeyboard(CLIMB, deltaTime);
						break;
					case SDLK_z:
						glhelper->camera.ProcessKeyboard(DESCEND, deltaTime);
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
            sdhrManager->Render();
        else
            a2VideoManager->Render();

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
        uint32_t bc = a2VideoManager->color_border;
		glClearColor((bc & 0xFF) / 256.0, (bc >> 8 & 0xFF) / 256.0, (bc >> 16 & 0xFF) / 256.0, (bc >> 24 & 0xFF) / 256.0);
		glClear(GL_COLOR_BUFFER_BIT);

		// Start the Dear ImGui frame
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL2_NewFrame();
		ImGui::NewFrame();
		// 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
		if (show_demo_window)
			ImGui::ShowDemoWindow(&show_demo_window);

		if (show_sdhrinfo_window)
		{
			ImGui::Begin("Super Duper Display Debug", &show_sdhrinfo_window);
			if (!ImGui::IsWindowCollapsed())
			{
				auto _c = glhelper->camera;
                auto _pos = _c.Position;
				ImGui::PushItemWidth(110);
                ImGui::Text("Press F1 at any time to toggle this window");
                ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
				ImGui::Text("Camera X:%.2f Y:%.2f Z:%.2f", _pos.x, _pos.y, _pos.z);
				ImGui::Text("Camera Pitch:%.2f Yaw:%.2f Zoom:%.2f", _c.Pitch, _c.Yaw, _c.Zoom);
				ImGui::Text("Screen Size:%03d x %03d", a2VideoManager->ScreenSize().x, a2VideoManager->ScreenSize().y);
				ImGui::Separator();
				ImGui::Text("VBL Start:%05d", cycleCounter->m_vbl_start);
				ImGui::Separator();
//				ImGui::Checkbox("Demo Window", &show_demo_window);
				ImGui::Checkbox("PostProcessing Window", &show_postprocessing_window);
				ImGui::Checkbox("VSYNC On", &g_swapInterval);
				ImGui::Checkbox("Use Beam Racing Renderer", &a2VideoManager->bShouldUseBeamRenderer);
				if (a2VideoManager->bShouldUseBeamRenderer)
					a2VideoManager->bShouldUseCPURGBRenderer = false;
				ImGui::Checkbox("Use CPU RGB Renderer for L/H/D/GR", &a2VideoManager->bShouldUseCPURGBRenderer);
				if (a2VideoManager->bShouldUseCPURGBRenderer)
					a2VideoManager->bShouldUseBeamRenderer = false;
				ImGui::Checkbox("Event Recorder Window", &show_recorder_window);
				ImGui::Checkbox("Textures Window", &show_texture_window);
				ImGui::Checkbox("Metrics Window", &show_metrics_window);
				ImGui::Checkbox("Apple //e Memory Window", &mem_edit_a2e.Open);
				ImGui::Checkbox("VRAM Legacy Memory Window", &mem_edit_vram_legacy.Open);
				ImGui::Checkbox("Upload Region Memory Window", &mem_edit_upload.Open);
				ImGui::Text("Load Memory Start: ");
				ImGui::SameLine();
				ImGui::InputInt("##mem_load", &mem_load_position, 1, 1024, ImGuiInputTextFlags_CharsHexadecimal);
				mem_load_position = std::clamp(mem_load_position, 0, 0xFFFF);
				ImGui::SameLine();
				ImGui::Checkbox("Aux Bank", &mem_load_aux_bank);
				MemoryLoad(mem_load_position, mem_load_aux_bank);
                ImGui::Separator();
                if (ImGui::Button("Reset")) {
                    a2VideoManager->ResetComputer();
                }
				did_press_quit = ImGui::Button("Quit App (Ctrl-c)");
				if (did_press_quit)
					done = true;
				ImGui::Separator();
				ImGui::Text("[ SDHR ]");
				ImGui::Checkbox("Untextured Geometry", &glhelper->bDebugNoTextures);         // Show textures toggle
				ImGui::Checkbox("Perspective Projection", &glhelper->bUsePerspective);       // Change projection type
                ImGui::Separator();
				ImGui::Text("[ Soft Switches ]");
                bool ssValue0 = a2VideoManager->IsSoftSwitch(A2SS_80STORE);
                if (ImGui::Checkbox("A2SS_80STORE", &ssValue0)) {
                    a2VideoManager->SetSoftSwitch(A2SS_80STORE, ssValue0);
                }
                bool ssValue1 = a2VideoManager->IsSoftSwitch(A2SS_RAMRD);
                if (ImGui::Checkbox("A2SS_RAMRD", &ssValue1)) {
                    a2VideoManager->SetSoftSwitch(A2SS_RAMRD, ssValue1);
                }
                bool ssValue2 = a2VideoManager->IsSoftSwitch(A2SS_RAMWRT);
                if (ImGui::Checkbox("A2SS_RAMWRT", &ssValue2)) {
                    a2VideoManager->SetSoftSwitch(A2SS_RAMWRT, ssValue2);
                }
                bool ssValue3 = a2VideoManager->IsSoftSwitch(A2SS_80COL);
                if (ImGui::Checkbox("A2SS_80COL", &ssValue3)) {
                    a2VideoManager->SetSoftSwitch(A2SS_80COL, ssValue3);
                }
                bool ssValue4 = a2VideoManager->IsSoftSwitch(A2SS_ALTCHARSET);
                if (ImGui::Checkbox("A2SS_ALTCHARSET", &ssValue4)) {
                    a2VideoManager->SetSoftSwitch(A2SS_ALTCHARSET, ssValue4);
                }
                bool ssValue5 = a2VideoManager->IsSoftSwitch(A2SS_INTCXROM);
                if (ImGui::Checkbox("A2SS_INTCXROM", &ssValue5)) {
                    a2VideoManager->SetSoftSwitch(A2SS_INTCXROM, ssValue5);
                }
                bool ssValue6 = a2VideoManager->IsSoftSwitch(A2SS_SLOTC3ROM);
                if (ImGui::Checkbox("A2SS_SLOTC3ROM", &ssValue6)) {
                    a2VideoManager->SetSoftSwitch(A2SS_SLOTC3ROM, ssValue6);
                }
                bool ssValue7 = a2VideoManager->IsSoftSwitch(A2SS_TEXT);
                if (ImGui::Checkbox("A2SS_TEXT", &ssValue7)) {
                    a2VideoManager->SetSoftSwitch(A2SS_TEXT, ssValue7);
                }
                bool ssValue8 = a2VideoManager->IsSoftSwitch(A2SS_MIXED);
                if (ImGui::Checkbox("A2SS_MIXED", &ssValue8)) {
                    a2VideoManager->SetSoftSwitch(A2SS_MIXED, ssValue8);
                }
                bool ssValue9 = a2VideoManager->IsSoftSwitch(A2SS_PAGE2);
                if (ImGui::Checkbox("A2SS_PAGE2", &ssValue9)) {
                    a2VideoManager->SetSoftSwitch(A2SS_PAGE2, ssValue9);
                }
                bool ssValue10 = a2VideoManager->IsSoftSwitch(A2SS_HIRES);
                if (ImGui::Checkbox("A2SS_HIRES", &ssValue10)) {
                    a2VideoManager->SetSoftSwitch(A2SS_HIRES, ssValue10);
                }
                bool ssValue11 = a2VideoManager->IsSoftSwitch(A2SS_DHGR);
                if (ImGui::Checkbox("A2SS_DHGR", &ssValue11)) {
                    a2VideoManager->SetSoftSwitch(A2SS_DHGR, ssValue11);
                }
				bool ssValue12 = a2VideoManager->IsSoftSwitch(A2SS_DHGRMONO);
				if (ImGui::Checkbox("A2SS_DHGRMONO", &ssValue12)) {
					a2VideoManager->SetSoftSwitch(A2SS_DHGRMONO, ssValue12);
				}
				bool ssValue13 = a2VideoManager->IsSoftSwitch(A2SS_SHR);
				if (ImGui::Checkbox("A2SS_SHR", &ssValue13)) {
					a2VideoManager->SetSoftSwitch(A2SS_SHR, ssValue13);
				}
				bool ssValue14 = a2VideoManager->IsSoftSwitch(A2SS_GREYSCALE);
				if (ImGui::Checkbox("A2SS_GREYSCALE", &ssValue14)) {
					a2VideoManager->SetSoftSwitch(A2SS_GREYSCALE, ssValue14);
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
            mem_edit_a2e.DrawWindow("Memory Editor: Apple 2 Memory (0000-C000 x2)", sdhrManager->GetApple2MemPtr(), 2*_A2_MEMORY_SHADOW_END);
        }

		// Show the VRAM legacy window
		if (mem_edit_vram_legacy.Open)
		{
			mem_edit_vram_legacy.DrawWindow("Memory Editor: Upload memory", a2VideoManager->GetLegacyVRAMPtr(), _BEAM_VRAM_SIZE_LEGACY);
		}
		
		// Show the upload data region memory
		if (mem_edit_upload.Open)
		{
            mem_edit_upload.DrawWindow("Memory Editor: Beam VRAM Legacy", sdhrManager->GetApple2MemPtr(), 2*_A2_MEMORY_SHADOW_END);
		}
        
		// Show the 16 textures loaded (which are always bound to GL_TEXTURE2 -> GL_TEXTURE18)
        if (show_texture_window)
		{
			ImGui::Begin("Texture Viewer", &show_texture_window);
            ImGui::SliderInt("Texture Slot Number", &_slotnum, 0, _SDHR_MAX_TEXTURES - 1, "slot %d", ImGuiSliderFlags_AlwaysClamp);
            ImGui::Text("Texture ID: %d", (int)glhelper->get_texture_id_at_slot(_slotnum));
			ImVec2 avail_size = ImGui::GetContentRegionAvail();
 			ImGui::Image((void*)glhelper->get_texture_id_at_slot(_slotnum), avail_size, ImVec2(0, 0), ImVec2(1, 1));
			ImGui::End();
		}

        // Add the rendered image, using borders
        int _w, _h;
        SDL_GL_GetDrawableSize(window, &_w, &_h);
        auto margin = ImVec2(0,0);
        if (sdhrManager->IsSdhrEnabled())
        {
			margin.x = margin.y = sdhrManager->windowMargins;
			ImGui::GetBackgroundDrawList()->AddImage(
				(void*)glhelper->get_output_texture_id(),
				margin,
				ImVec2(_w - margin.x, _h - margin.y),
				ImVec2(0, 0),
				ImVec2(1, 1)
			);
        }
        else {
            // In case of the Apple 2 video modes, let's make sure the
            // rendered image is always in the proper ratio
            // Start with the requested margins
			margin.x = margin.y = a2VideoManager->windowMargins;
            auto _ss = a2VideoManager->ScreenSize();
            float _rreq = (float)_w / _h;    // req ratio to use
            int32_t _maxW = _w - (2 * margin.x);
            int32_t _maxH = _h - (2 * margin.y);
            // Force integer scaling to have totally proper scanlines
            _maxW = _ss.x * (_maxW / (_ss.x));
			_maxH = _ss.y * (_maxH / (_ss.y));
            if (_maxW < _ss.x)
                _maxW = _ss.x;
            if (_maxH < _ss.y)
                _maxH = _ss.y;
            float _r = (float)_ss.x / _ss.y;
            int32_t _newW, _newH;
            if (_r < _rreq)    // requested a wider screen
            {
                _newW = _maxH * _r;
                _newH = _maxH;
            }
            else {      // requested a narrower screen
                _newW = _maxW;
                _newH = _maxW / _r;
            }
			margin.x = (_w - _newW) / 2;
			margin.y = (_h - _newH) / 2;
            ImGui::GetBackgroundDrawList()->AddRectFilled(
                margin,
                ImVec2(margin.x + _newW, margin.y + _newH),
                a2VideoManager->color_background
            );
			ImGui::GetBackgroundDrawList()->AddImage(
				(void*)glhelper->get_output_texture_id(),
				margin,
				ImVec2(_w - margin.x, _h - margin.y),
				ImVec2(0, 0),
				ImVec2(1, 1)
			);
        }

		// Rendering
		ImGui::Render();

		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		SDL_GL_SwapWindow(window);

		if ((glerr = glGetError()) != GL_NO_ERROR) {
			std::cerr << "OpenGL end of render error: " << glerr << std::endl;
		}

        // Update Event Recorder, could be replaying things
        EventRecorder::GetInstance()->Update();
        
        // Check if we should reboot
        if (a2VideoManager->bShouldReboot)
        {
            std::cerr << "reset detected" << std::endl;
            a2VideoManager->bShouldReboot = false;
			a2VideoManager->ResetComputer();
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
