// Super Duper Display uses ImGUI and its renderer for SDL2 + OpenGL

#define GL_SILENCE_DEPRECATION // Silence deprecation warnings on macOS for OpenGL

#define IMGUI_USER_CONFIG "../my_imgui_config.h"
#define _USE_STB_TRUETYPE

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
#include <atomic>

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
#include "TimedTextManager.h"
#include "LogTextManager.h"
#include "extras/MemoryLoader.h"
#include "extras/ImGuiFileDialog.h"
#include "PostProcessor.h"
#include "EventRecorder.h"
#include "MainMenu.h"

#if defined(__NETWORKING_APPLE__) || defined (__NETWORKING_LINUX__)
#include <unistd.h>
#include <libgen.h>
#include <pthread.h>
#include <sched.h>
#include <sys/resource.h>
#else
#include <windows.h>
#endif

static SwapInterval_e g_swapInterval = SWAPINTERVAL_ADAPTIVE;
static bool g_quitIsRequested = false;
static uint32_t g_fpsLimit = UINT32_MAX;
bool bUsePNGForScreenshots = true;			// PNG or BMP
float window_bgcolor[4] = { 0.0f, 0.0f, 0.0f, 1.0f }; // RGBA
int g_wx = 100, g_wy = 100, g_ww = 800, g_wh = 600;	// window dimensions when not in fullscreen

static SDL_Window* window;
static MainMenu* menu = nullptr;

static SDL_DisplayMode g_fullscreenMode;

static bool g_isLinuxConsole = false;

// For FPS calculations
static auto pfreq = SDL_GetPerformanceFrequency();
static float fps_worst = 1000000.f;
static uint64_t fps_frame_count = 0;
static uint64_t fps_last_counter_display = 0;
static char fps_str_buf[40];

float fpsAverageTimeWindow = 1.f;	// in seconds

// State booleans to determine what to do
bool bDisplayFPSOnScreen = false;	// Show FPS on screen
bool bIsSwapApple2Bus = false;		// Swap interval is the Apple 2 bus & tini is active
bool bShouldRenderA2Video = true;	// Render the Apple 2 video buffer
bool bA2VideoDidRender = false;		// Did the Apple 2 video render, or no need?
bool bShouldPostProcess = false;	// Send to PP and possibly to flip the frame
bool bShouldSwapFrame = true;		// After PP, frame should be swapped

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

SDL_Window* Main_GetSDLWindow()
{
	return window;
}

void Main_RequestAppQuit()
{
	g_quitIsRequested = true;
}

void Main_ResetFPSCalculations()
{
	fps_worst = 100000.f;
	fps_frame_count = 0;
}

uint32_t Main_GetFPSLimit()
{
	return g_fpsLimit;
}

void Main_SetFPSLimit(uint32_t fps)
{
	g_fpsLimit = fps;
}

SwapInterval_e Main_GetVsync()
{
	return g_swapInterval;
}

void Main_SetVsync(SwapInterval_e _vsync)
{
	int _si = -1;
	switch (_vsync) {
		case SWAPINTERVAL_NONE:
			_si = SDL_GL_SetSwapInterval(0);	// no vsync
			if (_si == 0)
				g_swapInterval = SWAPINTERVAL_NONE;
			break;
		case SWAPINTERVAL_VSYNC:
			_si = SDL_GL_SetSwapInterval(1);	// VSYNC
			if (_si == 0)
				g_swapInterval = SWAPINTERVAL_VSYNC;
			break;
		case SWAPINTERVAL_APPLE2BUS:
			_si = SDL_GL_SetSwapInterval(0);	// no vsync
			if (_si == 0)
				g_swapInterval = SWAPINTERVAL_APPLE2BUS;
			break;
		default:	// SWAPINTERVAL_ADAPTIVE
			// If adaptive vsync is requested, it may not be possible
			_si = SDL_GL_SetSwapInterval(-1);	// VSYNC
			if (_si == 0)
				g_swapInterval = SWAPINTERVAL_ADAPTIVE;
			else
				Main_SetVsync(SWAPINTERVAL_VSYNC);
			break;
	}
	
	Main_ResetFPSCalculations();
}

void Main_DisplaySplashScreen()
{
	if (MemoryLoadSHR("assets/logo.shr"))
	{
		MemoryManager::GetInstance()->SetSoftSwitch(A2SS_SHR, true);
	}
	// Run a refresh to show the first screen
	// going through 3 frames (0/1/0) to really clean the whole thing
	A2VideoManager::GetInstance()->ForceBeamFullScreenRender(3);
}

void Main_DrawFPSOverlay()
{
	auto a2VideoManager = A2VideoManager::GetInstance();
	if (bDisplayFPSOnScreen)
	{
		a2VideoManager->DrawOverlayString("AVERAGE FPS: ", 13, 0b11010010, 0, 0);
		// a2VideoManager->DrawOverlayString("WORST FPS: ", 11, 0b11010010, 2, 1);
	} else {
		a2VideoManager->EraseOverlayRange(20, 0, 0);
		// a2VideoManager->EraseOverlayRange(20, 0, 1);
	}
	a2VideoManager->ForceBeamFullScreenRender();
}

bool Main_IsFPSOverlay() {
	return bDisplayFPSOnScreen;
}

void Main_SetFPSOverlay(bool isFPSOverlay) {
	if (bDisplayFPSOnScreen != isFPSOverlay)
	{
		bDisplayFPSOnScreen = isFPSOverlay;
		Main_DrawFPSOverlay();
	}
}

bool Main_IsLinuxConsole() {
	return g_isLinuxConsole;
}

// True for both SDL_WINDOW_FULLSCREEN and SDL_WINDOW_FULLSCREEN_DESKTOP
bool Main_IsFullScreen() {
	// Assume non-resizable windows are fullscreen
	auto _flags = SDL_GetWindowFlags(window);
	if ((_flags & SDL_WINDOW_RESIZABLE) == 0)
		return true;
	if (_flags & SDL_WINDOW_FULLSCREEN_DESKTOP)
		return true;
	if (_flags & SDL_WINDOW_FULLSCREEN)
		return true;
	return false;
}

void Main_SetFullScreen(bool bWantFullscreen) {
#if defined(__LINUX__)
	// Only change resolution under linux console mode
	if (bWantFullscreen)
	{
		if (Main_IsLinuxConsole()) {
			SDL_SetWindowDisplayMode(window, &g_fullscreenMode);
			return;
		}
	}
#endif
	// Do nothing if it's Apple. Let the user maximize via the OSX UI
	// Because if the user sets fullscreen via the OSX UI we won't know,
	// and later setting fullscreen crashes SDL
#if !defined(__APPLE__)
	auto _flags = SDL_GetWindowFlags(window);
	// Don't do anything if the window isn't resizable
	if ((_flags & SDL_WINDOW_RESIZABLE) == 0)
		return;
	
	bool _fsres = false;
	if (bWantFullscreen)
	{
		if (SDL_SetWindowDisplayMode(window, &g_fullscreenMode) == 0)
		{
			if (SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP) == 0)
				_fsres = true;
		}
	}
	if (_fsres == false)
	{
		SDL_SetWindowFullscreen(window, 0);
		// Make sure the windowed mode shows the menu bar
		if (g_wy < 30)
			g_wy = 30;
		if (g_wx < 10)
			g_wx = 10;
		if ((g_wh + g_wy) > g_fullscreenMode.h)
			g_wh = g_fullscreenMode.h - g_wy;
		SDL_SetWindowBordered(window, SDL_TRUE);
		SDL_SetWindowResizable(window, SDL_TRUE);
		SDL_SetWindowPosition(window, g_wx, g_wy);
		SDL_SetWindowSize(window, g_ww, g_wh);
	}

	Main_ResetFPSCalculations();
#endif

}

SDL_DisplayMode Main_GetFullScreenMode() {
	return g_fullscreenMode;
}

void Main_SetFullScreenMode(SDL_DisplayMode mode) {
	g_fullscreenMode = mode;
	if (Main_IsFullScreen())
	{
		Main_SetFullScreen(false);
		Main_SetFullScreen(true);
	}
}

bool Main_IsImGuiOn()
{
	return (menu != nullptr);
}

MainMenu* Main_GetMenuPtr()
{
	return menu;
}

bool Main_GetbUsePNGForScreenshots() {
	return bUsePNGForScreenshots;
}

void Main_SetbUsePNGForScreenshots(bool bUsePNG) {
	bUsePNGForScreenshots = bUsePNG;
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

static void Main_SetImGui(SDL_GLContext gl_context, bool setState)
{
	if (setState) {	// turn on if off
		if (menu == nullptr) {
			menu = new MainMenu(gl_context, window);
			Main_ResetFPSCalculations();
		}
	} else {	// turn off if on
		if (menu != nullptr) {
			delete menu;
			menu = nullptr;
			Main_ResetFPSCalculations();
		}
	}
}

void Main_ResetA2SS() {
	auto a2VideoManager = A2VideoManager::GetInstance();
	auto memManager = MemoryManager::GetInstance();
	EventRecorder::GetInstance()->StopReplay();

	memManager->SetSoftSwitch(A2SS_SHR, false);
	memManager->SetSoftSwitch(A2SS_TEXT, false);
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
	memManager->SetSoftSwitch(A2SS_GREYSCALE, false);
	a2VideoManager->bUseDHGRCOL140Mixed = false;
	a2VideoManager->bUseHGRSPEC1 = false;
	a2VideoManager->bUseHGRSPEC2 = false;
	a2VideoManager->bDEMOMergedMode = false;
	a2VideoManager->bAlignQuadsToScanline = false;
	memManager->SetSoftSwitch(A2SS_TEXT, true);
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

#if defined(__LINUX__)
	const char* video_driver = SDL_GetCurrentVideoDriver();
	if (strcmp(video_driver, "KMSDRM") == 0) {
		g_isLinuxConsole = true;
	}
#endif


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
		| SDL_WINDOW_HIDDEN);
#elif defined(IMGUI_IMPL_OPENGL_ES2)
	SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL 
		| SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI 
		| SDL_WINDOW_HIDDEN);
#else
	SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL 
		| SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI
		| SDL_WINDOW_HIDDEN);
#endif

	// Special case for Linux console mode, make it fullscreen always
#if defined(__LINUX__)
	video_driver = SDL_GetCurrentVideoDriver();
	if (strcmp(video_driver, "KMSDRM") == 0) {
		window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL
			| SDL_WINDOW_FULLSCREEN | SDL_WINDOW_ALLOW_HIGHDPI
			| SDL_WINDOW_SHOWN);
		g_ww = g_fullscreenMode.w;
		g_wh = g_fullscreenMode.h;
		g_wx = 0;
		g_wy = 0;
	}
#endif

	// Get the actual display size
	if (SDL_GetCurrentDisplayMode(0, &g_fullscreenMode) != 0) {
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
		g_ww, g_wh, window_flags);
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
	
	glDisable(GL_DEPTH_TEST); // We're drawing in order, not using depth
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

	std::atomic<bool> bShouldTerminateNetworking = false;
	std::atomic<bool> bShouldTerminateProcessing = false;
	bool show_metrics_window = false;
	bool show_texture_window = false;
	bool show_a2video_window = true;
	bool show_postprocessing_window = false;
	bool show_recorder_window = false;
	VideoRegion_e vbl_region = VideoRegion_e::NTSC;		// videoregion_e::Unknown is auto
	
	// texture unit used by the main renderer,
	// to send to postprocessing
	GLuint A2VIDEO_TEX_UNIT = 0;

	// Get the instances of all singletons before creating threads
	// This ensures thread safety
	// The OpenGLHelper instance is already acquired
	[[maybe_unused]] auto logTextManager = LogTextManager::GetInstance();
	logTextManager->logPosition = TTLogPosition_e::BOTTOM_LEFT;
	std::cout << "Loaded LogTextManager " << logTextManager << std::endl;
	[[maybe_unused]] auto memManager = MemoryManager::GetInstance();
	std::cout << "Loaded MemoryManager " << memManager << std::endl;
	[[maybe_unused]] auto sdhrManager = SDHRManager::GetInstance();
	std::cout << "Loaded SDHRManager " << sdhrManager << std::endl;
	[[maybe_unused]] auto a2VideoManager = A2VideoManager::GetInstance();
	std::cout << "Loaded A2VideoManager " << a2VideoManager << std::endl;
	[[maybe_unused]] auto postProcessor = PostProcessor::GetInstance();
	std::cout << "Loaded PostProcessor " << postProcessor << std::endl;
	[[maybe_unused]] auto eventRecorder = EventRecorder::GetInstance();
	std::cout << "Loaded EventRecorder " << eventRecorder << std::endl;
	[[maybe_unused]] auto cycleCounter = CycleCounter::GetInstance();
	std::cout << "Loaded CycleCounter " << cycleCounter << std::endl;
	[[maybe_unused]] auto soundManager = SoundManager::GetInstance();
	std::cout << "Loaded SoundManager " << soundManager << std::endl;
	[[maybe_unused]] auto mockingboardManager = MockingboardManager::GetInstance();
	std::cout << "Loaded MockingboardManager " << mockingboardManager << std::endl;

	std::cout << "Renderer Initializing..." << std::endl;
	while (!a2VideoManager->IsReady())
	{
		// Wait for shaders to compile
	}
	std::cout << "Renderer Ready!" << std::endl;

	// Delta Time
	uint64_t dt_NOW = SDL_GetPerformanceCounter();
	uint64_t dt_LAST = 0;
	float deltaTime = 0.f;

	Main_SetVsync(g_swapInterval);

	uint32_t lastMouseMoveTime = SDL_GetTicks();
	[[maybe_unused]] const uint32_t cursorHideDelay = 3000; // After this delay, the mouse cursor disappears

	// Default to activating ImGui
	Main_SetImGui(gl_context, true);

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
		if (settingsState.contains("Log")) {
			logTextManager->DeserializeState(settingsState["Log"]);
		}
		if (settingsState.contains("Main")) {
			SDL_GetWindowPosition(window, &g_wx, &g_wy);
			SDL_GetWindowSize(window, &g_ww, &g_wh);
			auto _sm = settingsState["Main"];
			int _displayIndex = _sm.value("display index", 0);
			g_wx = _sm.value("window x", g_wx);
			g_wy = _sm.value("window y", g_wy);
			g_ww = _sm.value("window width", g_ww);
			g_wh = _sm.value("window height", g_wh);
			g_fpsLimit = _sm.value("fps limit", g_fpsLimit);
			g_swapInterval = (SwapInterval_e)_sm.value("vsync", (int)g_swapInterval);
			Main_SetVsync(g_swapInterval);
			// make sure the requested mode is acceptable
			SDL_DisplayMode newMode;
			newMode.w = _sm.value("fullscreen width", g_fullscreenMode.w);
			newMode.h = _sm.value("fullscreen height", g_fullscreenMode.h);
			newMode.refresh_rate = _sm.value("fullscreen refresh rate", g_fullscreenMode.refresh_rate);
			SDL_GetClosestDisplayMode(_displayIndex, &newMode, &g_fullscreenMode);
			Main_SetFullScreen(_sm.value("fullscreen", false));
			vbl_region = (VideoRegion_e)_sm.value("videoregion", vbl_region);
			if (vbl_region == VideoRegion_e::Unknown)
				cycleCounter->SetVideoRegion(VideoRegion_e::NTSC);
			else
				cycleCounter->SetVideoRegion(vbl_region);
			Main_SetImGui(gl_context, _sm.value("show F1 window", true));
			show_a2video_window = _sm.value("show Apple 2 Video window", show_a2video_window);
			show_postprocessing_window = _sm.value("show Post Processor window", show_postprocessing_window);
			show_recorder_window = _sm.value("show Recorder window", show_recorder_window);
			show_texture_window = _sm.value("show texture window", show_texture_window);
			show_metrics_window = _sm.value("show metrics window", show_metrics_window);
			bUsePNGForScreenshots = _sm.value("use PNG for screenshots", bUsePNGForScreenshots);
			if (_sm.contains("window background color") && _sm["window background color"].is_array()) {
				for (size_t i = 0; i < 4; ++i) {
					window_bgcolor[i] = _sm["window background color"][i].get<float>();
				}
			}
			// update the main window accordingly
			SDL_Rect displayBounds;
			if (SDL_GetDisplayBounds(_displayIndex, &displayBounds) == 0) {
				if ((g_wx < (displayBounds.x + displayBounds.w)) && (g_wy < (displayBounds.y + displayBounds.h)))
					SDL_SetWindowPosition(window, g_wx, g_wy);
				SDL_SetWindowSize(window, g_ww, g_wh);
			}
		}
		std::cout << "Previous state loaded!" << std::endl;
	} else {
		std::cerr << "No saved Settings.json file" << std::endl;
	}
	
	SDL_ShowWindow(window);
	SDL_GetWindowPosition(window, &g_wx, &g_wy);
	SDL_GetWindowSize(window, &g_ww, &g_wh);

	// Display version number
	std::string _vstr("Version ");
	_vstr.append(SDD_VERSION);
	_vstr.append(" - F1 toggles Menu");
	logTextManager->AddLog(_vstr, glm::vec4(.9f, .3f, .85f, 1.f));

	// Load up the first screen in SHR, with green border color
	Main_DisplaySplashScreen();

	if (bDisplayFPSOnScreen)
		Main_DrawFPSOverlay();

	// Run the network thread that will update the internal state as well as the apple 2 memory
	std::thread thread_server(usb_server_thread, &bShouldTerminateNetworking);
	// And run the processing thread
	std::thread thread_processor(process_usb_events_thread, &bShouldTerminateProcessing);

	// Set priority on the app and the usb server thread
#if defined (__NETWORKING_LINUX__)
    if (setpriority(PRIO_PROCESS, 0, -10) != 0) {
        std::cerr << std::endl << "Failed to set general app niceness, needs SUDO" << std::endl;
    }
	sched_param schParams;
	schParams.sched_priority = sched_get_priority_max(SCHED_FIFO);
	pthread_t nativeHandle = thread_server.native_handle();
	if (pthread_setschedparam(nativeHandle, SCHED_FIFO, &schParams) != 0) {
		std::cerr << "Failed to set thread priority for USB server thread, needs SUDO" << std::endl;
	}
#elif defined(__NETWORKING_APPLE__)
	pthread_t nativeHandle = thread_server.native_handle();
	qos_class_t qos = QOS_CLASS_USER_INTERACTIVE;
	int rel_prio = -15; // 0..-15 optional tighter priority within class
	auto server_pthread_qos_override = pthread_override_qos_class_start_np(nativeHandle, qos, rel_prio);
	if (server_pthread_qos_override == NULL) {
		std::cerr << "pthread_override_qos_class_start_np failed\n";
		return 0;
	}
#else
	HANDLE hProc = GetCurrentProcess();
	// Choose a priority class. HIGH_PRIORITY_CLASS is generally safe. REALTIME_PRIORITY_CLASS is dangerous.
	if (!SetPriorityClass(hProc, HIGH_PRIORITY_CLASS))
		std::cerr << "SetPriorityClass failed: " << GetLastError() << "\n";
	HANDLE threadHandle = thread_server.native_handle();
	if (!SetThreadPriority(threadHandle, THREAD_PRIORITY_TIME_CRITICAL))
		std::cerr << "SetThreadPriority failed: " << GetLastError() << "\n";
#endif

	while (!g_quitIsRequested)
	{
		// Check if we should reboot
		if (a2VideoManager->bShouldReboot)
		{
			std::cerr << "Reset detected" << std::endl;
			a2VideoManager->bShouldReboot = false;
			a2VideoManager->ResetComputer();
			if (bDisplayFPSOnScreen)
				Main_DrawFPSOverlay();	// It is wiped by the reset
		}
		a2VideoManager->CheckSetBordersWithReinit();
		bA2VideoDidRender = false;
		bShouldPostProcess = false;
		bShouldSwapFrame = true;
		bIsSwapApple2Bus = (g_swapInterval == SWAPINTERVAL_APPLE2BUS) && tini_is_ok();

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
					Main_RequestAppQuit();
					break;
				case SDL_WINDOWEVENT:
				{
					if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
						if (!Main_IsFullScreen())
						{
							g_ww = event.window.data1;
							g_wh = event.window.data2;
						}
					}
					if (event.window.event == SDL_WINDOWEVENT_MOVED) {
						if (!Main_IsFullScreen())
						{
							g_wx = event.window.data1;
							g_wy = event.window.data2;
						}
					}
					if (event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))
						Main_RequestAppQuit();
					}
					break;
				case SDL_MOUSEMOTION:
					lastMouseMoveTime = SDL_GetTicks();
					if (event.motion.state & SDL_BUTTON_RMASK) {
						// Move the camera when the right mouse button is pressed while moving the mouse
						if (sdhrManager->IsSdhrEnabled())
							sdhrManager->camera.ProcessMouseMovement((float)event.motion.xrel, (float)event.motion.yrel);
					}
					if (SDL_GetRelativeMouseMode()) {
						usb_mouse_send_event(event);
					}
					break;
				case SDL_MOUSEBUTTONDOWN:
					if (SDL_GetRelativeMouseMode()) {
						usb_mouse_send_event(event);
					}
					break;
				case SDL_MOUSEBUTTONUP:
					if (event.button.button == SDL_BUTTON_MIDDLE)
					{
						SDL_SetRelativeMouseMode(SDL_GetRelativeMouseMode() == SDL_TRUE ? SDL_FALSE : SDL_TRUE);
					}
					if (SDL_GetRelativeMouseMode()) {
						usb_mouse_send_event(event);
					}
					break;
				case SDL_MOUSEWHEEL:
					if (sdhrManager->IsSdhrEnabled())
						sdhrManager->camera.ProcessMouseScroll((float)event.wheel.y);
					break;
				case SDL_KEYDOWN:
				{
					if (event.key.keysym.sym == SDLK_F4) {  // Quit on ALT-F4
						if (SDL_GetModState() & KMOD_ALT) {
							Main_RequestAppQuit();
							break;
						}
					}
					else if (event.key.keysym.sym == SDLK_F5) {
						SDL_SetRelativeMouseMode(SDL_GetRelativeMouseMode() == SDL_TRUE ? SDL_FALSE : SDL_TRUE);
					}
					else if (event.key.keysym.sym == SDLK_F1) {  // Toggle ImGUI with F1
						Main_SetImGui(gl_context, !Main_IsImGuiOn());
						
						 // example of using the logTextManager
						 // don't forget to call UpdateAndRender()
						if (Main_IsImGuiOn())
							logTextManager->AddLog("GUI Activated");
						else
							logTextManager->AddLog("GUI Deactivated. Press F1 to reenable the GUI");
						 
					}
					else if (event.key.keysym.sym == SDLK_F6) {	// Screenshot
						std::string _vstr = "SCREENSHOT SAVED - " + glhelper->GetScreenshotSaveFilePath();
						if (SDL_GetModState() & KMOD_SHIFT) {	// before Post Processing
							glhelper->SaveTextureInSlotToFile(_TEXUNIT_POSTPROCESS,
								glhelper->GetScreenshotSaveFilePath(), bUsePNGForScreenshots);
						} else {								// after post processing
							glhelper->SaveFramebufferToFile(glhelper->GetScreenshotSaveFilePath(), bUsePNGForScreenshots);
						}
						logTextManager->AddLog(_vstr);
					}
					else if (event.key.keysym.sym == SDLK_F8) {
						if (SDL_GetModState() & KMOD_SHIFT) {
							// Reset FPS on Shift-F8
							Main_ResetFPSCalculations();
						}
						else {
							Main_SetFPSOverlay(!Main_IsFPSOverlay());
						}
					}
					else if (event.key.keysym.sym == SDLK_F10) {
						if (SDL_GetModState() & KMOD_SHIFT) {
							a2VideoManager->bAlwaysRenderBuffer = !a2VideoManager->bAlwaysRenderBuffer;
						} else {
							a2VideoManager->bAlwaysRenderBuffer = false;
						}
						a2VideoManager->ForceBeamFullScreenRender();
					}
					// Handle fullscreen toggle for Alt+Enter
					else if (event.key.keysym.sym == SDLK_RETURN && (event.key.keysym.mod & KMOD_ALT)) {
						Main_SetFullScreen(!Main_IsFullScreen());
					}
					// Make Alt-Tab in full screen revert to window mode
					else if (event.key.keysym.sym == SDLK_TAB && (event.key.keysym.mod & KMOD_ALT)) {
						if (Main_IsFullScreen())
							Main_SetFullScreen(false);
					}
					if (sdhrManager->IsSdhrEnabled())
					{
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
				}
					break;
				case SDL_USEREVENT:
					if (event.user.code == SDLUSEREVENT_A2NEWFRAME)
					{
						if (bIsSwapApple2Bus)
						{
							// This is a special case for handling the VSYNC with the Apple 2 bus:
							// An A2NEWFRAME event is sent when the beam racing code has just finished a frame.
							// We go through the A2->PP->frameSwap process unless the PP forbids the
							// frame swapping for reasons of frame merging.
							// NOTE: These events could be very close to each other, and multiple ones could
							// be in the queue. We must absolutely process all of them fully to be VSYNC'ed to
							// the bus itself. However, if we get 2+ events in one loop then we're just
							// going to be reusing the last VRAM read buffer for all the frames. It's not ideal but
							// there's no way to do better unless we triple or quadruple buffer the VRAM buffers,
							// or use a variable queue. Still though, if the host is too slow the buffers will
							// always overrun, so we max the # of frame events in the queue at MAX_USEREVENTS_IN_QUEUE
							if (!bShouldRenderA2Video)
								break;
							bA2VideoDidRender = a2VideoManager->Render(A2VIDEO_TEX_UNIT);
							// if (bA2VideoDidRender == false)
								// std::cerr << "Multiple A2 frames in one loop" << std::endl;
							if (A2VIDEO_TEX_UNIT == A2VIDEORENDER_ERROR)
								std::cerr << "ERROR: NO RENDERER OUTPUT!" << std::endl;
							glBindFramebuffer(GL_FRAMEBUFFER, 0);
							glClearColor(
								window_bgcolor[0],
								window_bgcolor[1],
								window_bgcolor[2],
								window_bgcolor[3]);
							glClear(GL_COLOR_BUFFER_BIT);
							postProcessor->Render(window, A2VIDEO_TEX_UNIT, a2VideoManager->ScreenSize().y);
							if (!postProcessor->ShouldFrameBeSkipped())
							{
								if (Main_IsImGuiOn())
								{
									menu->Render();
								}
								else {
									/*	DISABLE HIDDEN MOUSE CURSOR NOW THAT WE HAVE MOUSE LOCKING
									if ((SDL_GetTicks() - lastMouseMoveTime) > cursorHideDelay)
										SDL_ShowCursor(SDL_DISABLE);
									else
										SDL_ShowCursor(SDL_ENABLE);
									 */
								}
								logTextManager->UpdateAndRender(true);
								SDL_GL_SwapWindow(window);
								fps_frame_count++;
							}
						}
					}
					break;
				default:
					break;
			}   // switch event.type
		}   // while SDL_PollEvent

		if (!bIsSwapApple2Bus)
		{
			// if (sdhrManager->IsSdhrEnabled())
			// 		A2VIDEO_TEX_UNIT = sdhrManager->Render();
			// else
			if (bShouldRenderA2Video)
				bA2VideoDidRender = a2VideoManager->Render(A2VIDEO_TEX_UNIT);
			if (A2VIDEO_TEX_UNIT == A2VIDEORENDER_ERROR)
				std::cerr << "ERROR: NO RENDERER OUTPUT!" << std::endl;

			glBindFramebuffer(GL_FRAMEBUFFER, 0);

			glClearColor(
				window_bgcolor[0],
				window_bgcolor[1],
				window_bgcolor[2],
				window_bgcolor[3]);
			glClear(GL_COLOR_BUFFER_BIT);

			// Now run the postprocessing (not for IsSwapApple2Bus)
			postProcessor->Render(window, A2VIDEO_TEX_UNIT, a2VideoManager->ScreenSize().y);

			// Determine if frame should be swapped, or nothing done
			// Do that after the postprocessing phase, because PP may
			// be asking to skip a frame for merging even/odd frames
			if (postProcessor->ShouldFrameBeSkipped())
				bShouldSwapFrame = false;	// PP asking to not display the frame

			if (bShouldSwapFrame)
			{
				// This frame will be shown, so update ImGui and swap
				if (Main_IsImGuiOn())
				{
					menu->Render();
				}
				else {
					/*	DISABLE HIDDEN MOUSE CURSOR NOW THAT WE HAVE MOUSE LOCKING

					// Disable mouse if unused after cursorHideDelay
					// It's possible that the cursor won't get disabled when in windowed mode
					// (MacOS doesn't allow this, for example)
					if ((SDL_GetTicks() - lastMouseMoveTime) > cursorHideDelay)
						SDL_ShowCursor(SDL_DISABLE);
					else
						SDL_ShowCursor(SDL_ENABLE);
					 */
				}
				logTextManager->UpdateAndRender(true);
				SDL_GL_SwapWindow(window);
				fps_frame_count++;
			}
		}	// !bIsSwapApple2Bus

		// DELAY MANAGEMENT (to reduce wasteful CPU usage every loop)
		// Add a delay when not in monitor VSYNC to optimize CPU usage, and allow for custom FPS
		// This does not apply to SWAPINTERVAL_APPLE2BUS and an active Appletini, which must 
		// go as quickly as possible in order to have ideally a maximum of one frame per loop
		auto _newfpsLimit = UINT32_MAX;
		if (g_swapInterval == SWAPINTERVAL_NONE)
			_newfpsLimit = g_fpsLimit;
		else if (g_swapInterval == SWAPINTERVAL_APPLE2BUS)
		{
			if (tini_is_ok())
				_newfpsLimit = UINT32_MAX;	// don't delay
			else
				_newfpsLimit = 60;			// set a fixed 60 fps, no need for more
		}
		if (_newfpsLimit != UINT32_MAX)
		{
			float _frameTicks = pfreq / (float)g_fpsLimit;
			float _regionFps = (a2VideoManager->GetCurrentRegion() == VideoRegion_e::NTSC ? 59.95 : 50.00);
			if (g_swapInterval == SWAPINTERVAL_APPLE2BUS)
				_frameTicks = pfreq / _regionFps;
			uint64_t _elapsedTicks;
			uint32_t delayMs;
			while (true)
			{
				_elapsedTicks = SDL_GetPerformanceCounter() - dt_NOW;
				if (_elapsedTicks >= _frameTicks)
					break;
				double _remainingTicks = _frameTicks - _elapsedTicks;
				delayMs = static_cast<uint32_t>((_remainingTicks * 1000) / pfreq);
				if (delayMs > 1)
				{
					SDL_Delay(delayMs-1);
					// std::cerr << "Delaying " << 1 << std::endl;
				}
			}
			while (true)
			{
				if ((SDL_GetPerformanceCounter() - dt_NOW) >= (_frameTicks - pfreq/1000))
					break;
			}
		}

		// Finalize the frame info
		dt_LAST = dt_NOW;
		dt_NOW = SDL_GetPerformanceCounter();
		deltaTime = 1000.f * (float)((dt_NOW - dt_LAST) / (float)pfreq);
		// Calculate and display frame rate every second
		auto _fps_delta = dt_NOW - fps_last_counter_display;
		if (_fps_delta > (fpsAverageTimeWindow * pfreq))
		{
			float fps = fps_frame_count / ((float)_fps_delta / pfreq);
			if ((fps_worst > fps) && (fps > 0))
				fps_worst = fps;

			if (bDisplayFPSOnScreen)
			{
				snprintf(fps_str_buf, 10,  "%.0f ", fps);
				a2VideoManager->EraseOverlayRange(6, 13, 0);
				a2VideoManager->DrawOverlayString(fps_str_buf, 10, 0b11010010, 13, 0);
				// snprintf(fps_str_buf, 10, "%.0f ", fps_worst);
				// a2VideoManager->EraseOverlayRange(6, 13, 1);
				// a2VideoManager->DrawOverlayString(fps_str_buf, 10, 0b10010010, 13, 1);
			}
			// Reset for next calculation
			fps_frame_count = 0;
			fps_last_counter_display = dt_NOW;
		}

		if ((glerr = glGetError()) != GL_NO_ERROR) {
			std::cerr << "OpenGL end of render error: " << glerr << std::endl;
		}
	}	// main loop

	eventRecorder->StopReplay();
	soundManager->StopPlay();

	// Stop all threads
	bShouldTerminateProcessing = true;
	terminate_processing_thread();
	thread_processor.join();
	bShouldTerminateNetworking = true;
#if defined(__NETWORKING_APPLE__)
	if (server_pthread_qos_override != NULL)
		pthread_override_qos_class_end_np(server_pthread_qos_override);
#endif
	thread_server.join();

	// Serialize settings and save them
	{
		// before serializing settings, remove fullscreen to save correct window size
		bool _isFullscreen = Main_IsFullScreen();
		if (_isFullscreen)
			Main_SetFullScreen(false);
		int _wx, _wy, _ww, _wh;
		SDL_GetWindowPosition(window, &_wx, &_wy);
		SDL_GetWindowSize(window, &_ww, &_wh);
		settingsState["Post Processor"] = postProcessor->SerializeState();
		settingsState["Apple 2 Video"] = a2VideoManager->SerializeState();
		settingsState["Sound"] = soundManager->SerializeState();
		settingsState["Mockingboard"] = mockingboardManager->SerializeState();
		settingsState["Log"] = logTextManager->SerializeState();
		settingsState["Main"] = {
			{"display index", SDL_GetWindowDisplayIndex(window)},
			{"window x", _wx},
			{"window y", _wy},
			{"window width", _ww},
			{"window height", _wh},
			{"fullscreen width", g_fullscreenMode.w},
			{"fullscreen height", g_fullscreenMode.h},
			{"fullscreen refresh rate", g_fullscreenMode.refresh_rate},
			{"fullscreen", _isFullscreen},
			{"fps limit", g_fpsLimit},
			{"vsync", (int)g_swapInterval},
			{"videoregion", (int)cycleCounter->GetVideoRegion()},
			{"use PNG for screenshots", bUsePNGForScreenshots},
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
