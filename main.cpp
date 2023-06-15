// Dear ImGui: standalone example application for SDL2 + OpenGL
// (SDL is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)
// If you are new to Dear ImGui, read documentation from the docs/ folder + read the top of imgui.cpp.
// Read online: https://github.com/ocornut/imgui/tree/master/docs

#define IMGUI_USER_CONFIG "../../my_imgui_config.h"
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
#include <thread>

#include "common.h"
#include "shader.h"
#include "camera.h"
#include "MosaicMesh.h"

#include "SDHRNetworking.h"
#include "SDHRManager.h"
#include "A2VideoManager.h"
#include "OpenGLHelper.h"



// Main code
int main(int, char**)
{
	GLenum glerr;
    // Setup SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
    {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

    // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
    // GL ES 2.0 + GLSL 100
    const char* glsl_version = "#version 100";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#elif defined(__APPLE__)
    // GL 3.2 Core + GLSL 150
    const char* glsl_version = "#version 150";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG); // Always required on Mac
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#else
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    if (SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0) != 0)
        std::cerr << "SDL Error: " << SDL_GetError() << std::endl;
    if (SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE) != 0)
		std::cerr << "SDL Error: " << SDL_GetError() << std::endl;
    if (SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3) != 0)
		std::cerr << "SDL Error: " << SDL_GetError() << std::endl;
    if (SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1) != 0)
		std::cerr << "SDL Error: " << SDL_GetError() << std::endl;
#endif

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
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Window* window = SDL_CreateWindow(_MAINWINDOWNAME, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, _SCREEN_DEFAULT_WIDTH, _SCREEN_DEFAULT_HEIGHT, window_flags);
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1); // Enable vsync

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
    ImGui_ImplOpenGL3_Init(glsl_version);

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

    // Our state
	bool bShouldTerminateNetworking = false;
    bool show_demo_window = false;
    bool show_metrics_window = false;
	bool show_mem_apple2_window = false;
	bool show_mem_upload_window = false;
	bool show_sdhrinfo_window = true;
	bool show_texture_window = false;
    bool bRescaleSHDRFramebuffer = true;
    bool did_press_quit = false;
	int _slotnum = 0;

	static uint32_t fbWidth = 0;
	static uint32_t fbHeight = 0;

	auto sdhrManager = SDHRManager::GetInstance();
    auto a2VideoManager = A2VideoManager::GetInstance();
	auto glhelper = OpenGLHelper::GetInstance();

	// Run the network thread that will update the internal state as well as the apple 2 memory
	std::thread thread_server(socket_server_thread, (uint16_t)_SDHR_SERVER_PORT, &bShouldTerminateNetworking);


    // Delta Time
	uint64_t dt_NOW = SDL_GetPerformanceCounter();
    uint64_t dt_LAST = 0;
	float deltaTime = 0.f;

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
        dt_LAST = dt_NOW;
        dt_NOW = SDL_GetPerformanceCounter();
		deltaTime = 1000.f * (float)((dt_NOW - dt_LAST) / (float)SDL_GetPerformanceFrequency());
        
        // In case the window was program-resized, tell SDL to change the window size
        if (glhelper->GetDidChangeResolution())
        {
            glhelper->get_framebuffer_size(&fbWidth, &fbHeight);
            if (sdhrManager->IsSdhrEnabled())
            {
                SDL_SetWindowSize(window,
                    fbWidth + 2 * sdhrManager->windowMargins,
                    fbHeight + 2 * sdhrManager->windowMargins);
            }
            else {
				SDL_SetWindowSize(window,
					fbWidth + 2 * a2VideoManager->windowMargins,
					fbHeight + 2 * a2VideoManager->windowMargins);
            }
        }


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
				else if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                    // Window was resized. Depending on SDHR or regular A2 video modes, figure out the
                    // max acceptable render size and request of the renderer to change the size
                    uint32_t reqWidth = event.window.data1;
                    uint32_t reqHeight = event.window.data2;
                    if (sdhrManager->IsSdhrEnabled())
                    {
                        reqWidth -= (2 * sdhrManager->windowMargins);
						reqHeight -= (2 * sdhrManager->windowMargins);
                        // TODO: Keep the SDHR ratio provided by SDHR_CMD_CHANGE_RESOLUTION
                        glhelper->rescale_framebuffer(reqWidth, reqHeight);
                    }
                    else {  // Regular Apple 2 video modes
						reqWidth -= (2 * a2VideoManager->windowMargins);
						reqHeight -= (2 * a2VideoManager->windowMargins);
                        a2VideoManager->Resize(reqWidth, reqHeight);
						glhelper->rescale_framebuffer(reqWidth, reqHeight);
                    }
				}
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
				if (event.key.keysym.sym == SDLK_F4) {  // Quit on Alt-F4
					auto state = SDL_GetKeyboardState(NULL);
					if (state[SDL_SCANCODE_LALT]) {
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
		glClearColor(0.f, 0.f, 0.f, 1.0f);
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
                ImGui::Text("Press F1 at any time to toggle this window");
                ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
				ImGui::Text("Camera X:%.2f Y:%.2f Z:%.2f", _pos.x, _pos.y, _pos.z);
				ImGui::Text("Camera Pitch:%.2f Yaw:%.2f Zoom:%.2f", _c.Pitch, _c.Yaw, _c.Zoom);
				ImGui::Separator();
				ImGui::Checkbox("Untextured Geometry", &glhelper->bDebugNoTextures);             // Show textures toggle
				ImGui::Checkbox("Perspective Projection", &glhelper->bUsePerspective);       // Change projection type
                ImGui::Checkbox("Rescale Framebuffer", &bRescaleSHDRFramebuffer);
				ImGui::Separator();
//				ImGui::Checkbox("Demo Window", &show_demo_window);
				ImGui::Checkbox("Textures Window", &show_texture_window);
				ImGui::Checkbox("Metrics Window", &show_metrics_window);
				ImGui::Checkbox("Apple //e Memory Window", &show_mem_apple2_window);
				ImGui::Checkbox("Upload Region Memory Window", &show_mem_upload_window);
                ImGui::Separator();
				did_press_quit = ImGui::Button("Quit App (Alt-F4)");
				if (did_press_quit)
					done = true;
            }
			ImGui::End();
		}

        // Show the metrics window
        if (show_metrics_window)
			ImGui::ShowMetricsWindow(&show_metrics_window);

        // Show the Apple //e memory
        if (show_mem_apple2_window)
        {
            static MemoryEditor mem_edit_a2e;
            mem_edit_a2e.DrawWindow("Memory Editor: Apple 2 Memory (0000-C000)", sdhrManager->GetApple2MemPtr(), 0xc000);
        }

		// Show the upload data region memory
		if (show_mem_upload_window)
		{
			static MemoryEditor mem_edit_upload;
            mem_edit_upload.DrawWindow("Memory Editor: Upload memory", sdhrManager->GetUploadRegionPtr(), _SDHR_UPLOAD_REGION_SIZE);
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
        SDL_GetWindowSize(window, &_w, &_h);
        auto margin = ImVec2(0,0);
        if (sdhrManager->IsSdhrEnabled())
            margin.x = margin.y = sdhrManager->windowMargins;
        else
			margin.x = margin.y = a2VideoManager->windowMargins;
		ImGui::GetBackgroundDrawList()->AddImage(
			(void*)glhelper->get_output_texture_id(),
			margin,
			ImVec2(_w - margin.x, _h - margin.y),
			ImVec2(0, 0),
			ImVec2(1, 1)
		);

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

    // Network cleanup
    bShouldTerminateNetworking = true;
    socket_unblock_accept(_SDHR_SERVER_PORT);
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
