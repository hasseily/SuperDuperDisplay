// Dear ImGui: standalone example application for SDL2 + OpenGL
// (SDL is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)
// If you are new to Dear ImGui, read documentation from the docs/ folder + read the top of imgui.cpp.
// Read online: https://github.com/ocornut/imgui/tree/master/docs

#define _SERVER_PORT 8080

#define GLM_MESSAGES    // Shows the GLM configuration at build time

#define IMGUI_USER_CONFIG "../../my_imgui_config.h"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include "imgui_memory_editor.h"
#include <SDL.h>
#include "stb_image.h"

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
#include "OpenGLHelper.h"



// Main code
int main(int, char**)
{
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
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
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
    SDL_Window* window = SDL_CreateWindow(_MAINWINDOWNAME, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1); // Enable vsync

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
	// glEnable(GL_DEPTH_TEST); // TODO: Check if necessary
    glEnable(GL_CULL_FACE);
	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_NOTEQUAL, 0.0);
    GLenum glerr;
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
	bool show_memory_window = false;
    bool did_press_quit = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
	int _slotnum = 0;

	auto sdhrManager = SDHRManager::GetInstance();
	auto glhelper = OpenGLHelper::GetInstance();

	// Run the network thread that will update the internal state as well as the apple 2 memory
	std::thread thread_server(socket_server_thread, (uint16_t)_SERVER_PORT, &bShouldTerminateNetworking);


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

        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                done = true;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))
                done = true;
            if (event.type == SDL_KEYDOWN) {
				if (event.key.keysym.sym == SDLK_F4) {  // Quit on Alt-F4
					auto state = SDL_GetKeyboardState(NULL);
					if (state[SDL_SCANCODE_LALT]) {
						done = true;
					}
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
            else if (event.type == SDL_MOUSEMOTION && event.motion.state & SDL_BUTTON_RMASK && !io.WantCaptureMouse) {
                // Move the camera when the right mouse button is pressed while moving the mouse
                sdhrManager->camera.ProcessMouseMovement(event.motion.xrel, event.motion.yrel);
            }
            else if (event.type == SDL_MOUSEWHEEL && !io.WantCaptureMouse) {
                sdhrManager->camera.ProcessMouseScroll(event.wheel.preciseY);
            }
        }

/*
        // TEST STUFF
		glBindFramebuffer(GL_FRAMEBUFFER, FBO);
		glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		shd.use();
		glBindVertexArray(VAO);
		glDrawArrays(GL_TRIANGLES, 0, 3);
		// END TEST STUFF
*/

		sdhrManager->Render();

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		// Start the Dear ImGui frame
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL2_NewFrame();
		ImGui::NewFrame();

		// 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
		if (show_demo_window)
			ImGui::ShowDemoWindow(&show_demo_window);

		{
			ImGui::Begin("Super Duper Display Options");
			if (!ImGui::IsWindowCollapsed())
			{
				auto _c = sdhrManager->camera;
                auto _pos = _c.Position;
                ImGui::Text("Camera X:%.2f Y:%.2f Z:%.2f", _pos.x, _pos.y, _pos.z);
				ImGui::Text("Camera Pitch:%.2f Yaw:%.2f Zoom:%.2f", _c.Pitch, _c.Yaw, _c.Zoom);
				ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
//				ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
				ImGui::Checkbox("Memory Window", &show_memory_window);      // Edit bools storing our window open/close state
                ImGui::Separator();
                ImGui::Checkbox("Textured Geometry", &sdhrManager->bDebugTextures);             // Show textures toggle
				ImGui::Checkbox("Perspective Projection", &sdhrManager->bUsePerspective);       // Change projection type
				ImGui::Separator();
				did_press_quit = ImGui::Button("Quit App (Alt-F4)");
				if (did_press_quit)
					done = true;
            }
			ImGui::End();
		}

        // Show a memory editor
        if (show_memory_window)
        {
            static MemoryEditor mem_edit_1;
            mem_edit_1.DrawWindow("Memory Editor: Apple 2 Memory (0000-C000)", sdhrManager->GetApple2MemPtr(), 0xc000);
        }
        
		// Show one of the textures loaded
		{
			ImGui::Begin("Texture Viewer");
            ImGui::SliderInt("Texture Slot Number", &_slotnum, 0, _SDHR_MAX_TEXTURES - 1, "slot %d", ImGuiSliderFlags_AlwaysClamp);
            ImGui::Text("Texture ID: %d", glhelper->get_texture_id_at_slot(_slotnum));
			ImVec2 avail_size = ImGui::GetContentRegionAvail();
			ImVec2 window_pos = ImGui::GetWindowPos();
			ImVec2 window_size = ImGui::GetWindowSize();
			ImVec2 window_center = ImVec2(window_pos.x + window_size.x * 0.5f, window_pos.y + window_size.y * 0.5f);

			// glhelper->rescale_framebuffer((uint32_t)window_size.x, (uint32_t)window_size.y);
 			ImGui::Image((void*)glhelper->get_texture_id_at_slot(_slotnum), avail_size, ImVec2(0, 0), ImVec2(1, 1));
			ImGui::End();
		}

        int sdlwidth, sdlheight;
		SDL_GetWindowSize(window, &sdlwidth, &sdlheight);
		glhelper->bind_framebuffer();
		glhelper->rescale_framebuffer(sdlwidth, sdlheight);
		glhelper->unbind_framebuffer();

		ImGui::GetBackgroundDrawList()->AddImage(
			(void*)glhelper->get_output_texture_id(),
			ImVec2(0, 0),
			ImVec2(sdlwidth, sdlheight),
			ImVec2(0, 0),
			ImVec2(1, 1)
		);

		// Get available space
        // For the main window it's a never the window size but the screen
        // ImVec2 available = ImGui::GetContentRegionAvail();
        // ImVec2 window_pos = ImGui::GetWindowPos();
        // ImVec2 window_size = ImGui::GetWindowSize();

        // ImVec2 window_center = ImVec2(window_pos.x + window_size.x * 0.5f, window_pos.y + window_size.y * 0.5f);
        // ImGui::GetBackgroundDrawList()->AddCircle(window_center, window_size.x * 0.6f, IM_COL32(255, 0, 0, 200), 0, 10 + 4);
        // ImGui::GetForegroundDrawList()->AddCircle(window_center, window_size.y * 0.6f, IM_COL32(0, 255, 0, 200), 0, 10);
        //auto _txtstr = "DRAWING a string!!!!";
        //ImGui::GetForegroundDrawList()->AddText(window_center, IM_COL32(100, 100, 0, 255), _txtstr);

		// Rendering
		ImGui::Render();

		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		SDL_GL_SwapWindow(window);

    }
#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_END;
#endif

    // Network cleanup
    bShouldTerminateNetworking = true;
    socket_unblock_accept(_SERVER_PORT);
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
