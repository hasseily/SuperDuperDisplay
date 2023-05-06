// Dear ImGui: standalone example application for SDL2 + OpenGL
// (SDL is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)
// If you are new to Dear ImGui, read documentation from the docs/ folder + read the top of imgui.cpp.
// Read online: https://github.com/ocornut/imgui/tree/master/docs

#define _SERVER_PORT 8080

#define IMGUI_USER_CONFIG "../../my_imgui_config.h"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include "imgui_memory_editor.h"
#include <stdio.h>
#include <SDL.h>
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <SDL_opengles2.h>
#define GL2_PROTOTYPES 1
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2platform.h>
#else
#include <GL/glew.h>
#include <SDL_opengl.h>
#endif

// This example can also compile and run with Emscripten! See 'Makefile.emscripten' for details.
#ifdef __EMSCRIPTEN__
#include "../libs/emscripten/emscripten_mainloop_stub.h"
#endif

#include <cstring>
#include <thread>
#include "SDHRNetworking.h"
#include "SDHRManager.h"

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

	GLenum err = glewInit();
	if (GLEW_OK != err)
	{
		/* Problem: glewInit failed, something is seriously wrong. */
		fprintf(stderr, "Error: %s\n", glewGetErrorString(err));
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

	// Create an image texture in which we'll copy the generated pixels
    // Also create a second texture to which we'll copy the temp CPU buffer
    auto sdhrManager = SDHRManager::GetInstance();
	GLuint image_textures[2];
	glGenTextures(2, image_textures);
    sdhr_image image_struct = sdhr_image();
    image_struct.texture_id = image_textures[0];
    sdhrManager->SetSDHRImage(image_struct);

    // Initialize it to zero
	glBindTexture(GL_TEXTURE_2D, image_struct.texture_id);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image_struct.width, image_struct.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);

	// Run the network thread that will update the internal state as well as the apple 2 memory
	std::thread thread_server(socket_server_thread, _SERVER_PORT, &bShouldTerminateNetworking);

	typedef struct
	{
		float x;
		float y;
	} vector2;

	typedef struct
	{
		float x;
		float y;
		float z;
	} vector3;

	typedef struct
	{
		vector3 position;
		vector2 textureCoordinate;
	} vertex;

#define CUBE_TRIANGLE_COUNT 12
	vertex cube[CUBE_TRIANGLE_COUNT * 3];

	const char* vertexSource = R"glsl(
attribute vec4 position;                // vertex position attribute
attribute vec2 texCoord;                // vertex texture coordinate attribute
 
uniform mat4 modelView;                 // shader modelview matrix uniform
uniform mat4 projection;                // shader projection matrix uniform
 
varying vec2 texCoordVar;               // vertex texture coordinate varying
 
void main()
{
    vec4 p = modelView * position;      // transform vertex position with modelview matrix
    gl_Position = projection * p;       // project the transformed position and write it to gl_Position
    texCoordVar = texCoord;             // assign the texture coordinate attribute to its varying
}
)glsl";
	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, 1, &vertexSource, NULL);
    glCompileShader(vertexShader);
    while ((err = glGetError()) != GL_NO_ERROR) {
        std::cerr << "OpenGL error compiling vertex shader: " << err << std::endl;
    }

	const char* fragmentSource = R"glsl(
precision mediump float;        // set default precision for floats to medium
 
uniform sampler2D texture;      // shader texture uniform
 
varying vec2 texCoordVar;       // fragment texture coordinate varying
 
void main()
{
    // sample the texture at the interpolated texture coordinate
    // and write it to gl_FragColor 
    gl_FragColor = texture2D( texture, texCoordVar);
}
)glsl";
	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
	glCompileShader(fragmentShader);
	while ((err = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL error compiling fragment shader: " << err << std::endl;
	}

	GLuint shaderProgram = glCreateProgram();
	glAttachShader(shaderProgram, vertexShader);
	glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    glUseProgram(shaderProgram);
	while ((err = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL error at glUseProgram: " << err << std::endl;
	}
	// enable and send vertex position attribute data
    GLuint positionIndex = 0;
	glEnableVertexAttribArray(positionIndex);
	glVertexAttribPointer(positionIndex, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), &cube[0].position);
	while ((err = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL error at vertex attributes: " << err << std::endl;
	}

	// enable and send vertex texture coordinate attribute data
	GLuint textureCoordIndex = 1;
	glEnableVertexAttribArray(textureCoordIndex);
	glVertexAttribPointer(textureCoordIndex, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), &cube[0].textureCoordinate);
	while ((err = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL error at texture attributes: " << err << std::endl;
	}

    // Create uniforms
	GLfloat modelViewMatrix[] = {
	    0.5f, 0.0f, 0.0f, 0.00f,
		0.0f, 0.5f, 0.0f, 0.00f,
		0.0f, 0.0f, 0.5f, 0.00f,
		0.25f, 0.5f, 0.75f, 1.0f,
	};
	GLfloat projectionMatrix[] = {
		0.5f, 0.0f, 0.0f, 0.00f,
		0.0f, 0.5f, 0.0f, 0.00f,
		0.0f, 0.0f, 0.5f, 0.00f,
		0.25f, 0.5f, 0.75f, 1.0f,
	};
	
    GLuint modelViewLocation = glGetUniformLocation(shaderProgram, "modelView");
    GLuint projectionLocation = glGetUniformLocation(shaderProgram, "projection");
    GLuint textureLocation = glGetUniformLocation(shaderProgram, "texture");

	// set uniforms
	glUniformMatrix4fv(modelViewLocation, 1, GL_FALSE, modelViewMatrix);       // set modelView matrix
	glUniformMatrix4fv(projectionLocation, 1, GL_FALSE, projectionMatrix);     // set projection matrix
	glUniform1i(textureLocation, 0);                                           // set texture unit to sample
	while ((err = glGetError()) != GL_NO_ERROR) {
		std::cerr << "OpenGL error at set uniforms: " << err << std::endl;
	}

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
            if (event.key.keysym.sym == SDLK_F4) {  // Quit on Alt-F4
                auto state = SDL_GetKeyboardState(NULL);
                if (state[SDL_SCANCODE_LALT]) {
                    done = true;
                }
            }
        }

		// Start the Dear ImGui frame
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL2_NewFrame();
		ImGui::NewFrame();

		// 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
		if (show_demo_window)
			ImGui::ShowDemoWindow(&show_demo_window);

		if (sdhrManager->threadState == THREADCOMM_e::COMMAND_PROCESSED)
		{
			sdhrManager->threadState = THREADCOMM_e::MAIN_LOCK;
			sdhrManager->DrawWindowsIntoScreenImage(image_struct.texture_id);
			sdhrManager->threadState = THREADCOMM_e::IDLE;
		}

		// 2. Show a window with the SDHR Output from a glTexSubImage2D pixel copy
		{
			ImGui::Begin("glTexSubImage2D Technique (1 pixel at a time)");
            sdhrManager->shouldUseSubImage2D = !ImGui::IsWindowCollapsed();
			if (!ImGui::IsWindowCollapsed())
			{
				ImGui::Text("size = %d x %d", image_struct.width, image_struct.height);
				ImGui::Image((void*)(intptr_t)image_struct.texture_id, ImVec2(image_struct.width, image_struct.height));

				ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
				// ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
				ImGui::Checkbox("Memory Window", &show_memory_window);      // Edit bools storing our window open/close state
				did_press_quit = ImGui::Button("Quit App (Alt-F4)");
				if (did_press_quit)
					done = true;
            }
			ImGui::End();
		}

		// 3. Show a test window using the cpubuffer
		{
            ImGui::SetNextWindowPos(ImVec2(600, 400), ImGuiCond_FirstUseEver);
			ImGui::Begin("Temp CPU Buffer Technique");
            sdhrManager->shouldUseCpuBuffer = !ImGui::IsWindowCollapsed();
            if (!ImGui::IsWindowCollapsed())
            {
				glBindTexture(GL_TEXTURE_2D, image_textures[1]);

				// Setup filtering parameters for display
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // This is required on WebGL for non power-of-two textures
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); // Same

				// Upload pixels into texture
#if defined(GL_UNPACK_ROW_LENGTH) && !defined(__EMSCRIPTEN__)
				glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, _SDHR_WIDTH, _SDHR_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, sdhrManager->cpubuffer);
				GLenum err;
				while ((err = glGetError()) != GL_NO_ERROR) {
					std::cerr << "OpenGL error: " << err << std::endl;
				}
				ImGui::Text("size = %d x %d", _SDHR_WIDTH, _SDHR_HEIGHT);
				ImGui::Image((void*)(intptr_t)image_textures[1], ImVec2(_SDHR_WIDTH, _SDHR_HEIGHT));
				ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
            }

			ImGui::End();
		}

        // 4. Show a memory editor
        if (show_memory_window)
        {
            static MemoryEditor mem_edit_1;
            mem_edit_1.DrawWindow("Memory Editor", sdhrManager->cpubuffer, _SDHR_WIDTH * _SDHR_HEIGHT *4);
        }


        //ImVec2 window_pos = ImGui::GetWindowPos();
        //ImVec2 window_size = ImGui::GetWindowSize();
        //ImVec2 window_center = ImVec2(window_pos.x + window_size.x * 0.5f, window_pos.y + window_size.y * 0.5f);
        //ImGui::GetBackgroundDrawList()->AddCircle(window_center, window_size.x * 0.6f, IM_COL32(255, 0, 0, 200), 0, 10 + 4);
        //ImGui::GetForegroundDrawList()->AddCircle(window_center, window_size.y * 0.6f, IM_COL32(0, 255, 0, 200), 0, 10);
        //auto _txtstr = "DRAWING a string!!!!";
        //ImGui::GetForegroundDrawList()->AddText(window_center, IM_COL32(100, 100, 0, 255), _txtstr);

		// Rendering
		ImGui::Render();
		glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
		glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		glDrawArrays(GL_TRIANGLES, 0, CUBE_TRIANGLE_COUNT * 3);
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
