#pragma once

#ifndef A2VIDEOMANAGER_H
#define A2VIDEOMANAGER_H

#include <stdint.h>
#include <stddef.h>
#include <vector>

#include "common.h"
#include "A2WindowBeam.h"
#include "VidHdWindowBeam.h"
#include "CycleCounter.h"
#include "imgui.h"
#include "imgui_memory_editor.h"

enum class A2Mode_e
{
	NONE = 0,
	LEGACY,
	SHR,
	MERGED,
	A2MODE_TOTAL_COUNT
};

enum class BeamState_e
{
	UNKNOWN = 0,
	NBHBLANK,			// HBLANK not in border
	NBVBLANK,			// VBLANK not in border
	BORDER_LEFT,
	BORDER_RIGHT,
	BORDER_TOP,
	BORDER_BOTTOM,
	CONTENT,
	BEAMSTATE_TOTAL_COUNT
};

[[maybe_unused]] static std::string BeamStateToString(BeamState_e state) {
	switch(state) {
		case BeamState_e::UNKNOWN: return "UNKNOWN";
		case BeamState_e::NBHBLANK: return "NBHBLANK";
		case BeamState_e::NBVBLANK: return "NBVBLANK";
		case BeamState_e::BORDER_LEFT: return "BORDER_LEFT";
		case BeamState_e::BORDER_RIGHT: return "BORDER_RIGHT";
		case BeamState_e::BORDER_TOP: return "BORDER_TOP";
		case BeamState_e::BORDER_BOTTOM: return "BORDER_BOTTOM";
		case BeamState_e::CONTENT: return "CONTENT";
		default: return "INVALID_STATE";
	}
}

// There could be anywhere up to 6 or 7 cycles for horizontal borders
// and a lot more for vertical borders.
// But SHR starts VBLANK just like legacy modes, at scanline 192. Hence
// it has 8 less bottom border scanlines than legacy.
// Vertical borders must be in multiples of 8 to ensure the shader works
// properly given the text modes
constexpr uint32_t _BORDER_WIDTH_MAX_CYCLES = 7;
constexpr uint32_t _BORDER_HEIGHT_MAX_MULT8 = 3;	// 3*8 scanlines

// Our arbitrary start of a new frame. It should be inside VBLANK and
// after the maximum bottom border size, but before the top border
constexpr uint32_t _SCANLINE_START_FRAME = 200 + (_BORDER_HEIGHT_MAX_MULT8 * 8) + 1;

constexpr int _OVERLAY_CHAR_WIDTH = 40;		// Max width of overlay in chars (40 for TEXT)

constexpr int _INTERLACE_MULTIPLIER = 2;	// How much to multiply the size of buffers for interlacing

// Legacy mode VRAM is 4 bytes (main, aux, flags, colors)
// for each "byte" of screen use
// colors are 4-bit each of fg and bg colors as in the Apple 2gs


// SHR mode VRAM is the standard bytes of screen use ($2000 to $9CFF)
// plus, for each of the 200 lines, at the beginning of the line draw
// (at end of HBLANK) the SCB (1 byte) and palette (32 bytes) of that line
// SHR mode VRAM looks like this:

// SCB        PALETTE                    COLOR BYTES
// [0 [(1 2) (3 4) ... (31 32)] [[L_BORDER] [  TOP_BORDER ] [R_BORDER]]]	// line 0
//                         .												// top border lines
//                         .
// [0 [(1 2) (3 4) ... (31 32)] [[L_BORDER] 0 ......... 159 [R_BORDER]]]	// line top_border*8 + 0
// [0 [(1 2) (3 4) ... (31 32)] [[L_BORDER] 0 ......... 159 [R_BORDER]]]	// line top_border*8 + 1
//                         .
//                         .
//                         .
//                         .
// [0 [(1 2) (3 4) ... (31 32)] [[L_BORDER] 0 ......... 159 [R_BORDER]]]	// line top_border*8 + 199
// [0 [(1 2) (3 4) ... (31 32)] [[L_BORDER] [ BOTM_BORDER ] [R_BORDER]]]	// line top_border*8 + 199 + bottom_border
//                         .												// bottom border lines
//                         .
// [0 [(1 2) (3 4) ... (31 32)] [[L_BORDER] [ BOTM_BORDER ] [R_BORDER]]]	// line top_border*8 + 199 + bottom_border*8 

// The BORDER bytes have the exact border color in their lower 4 bits
// Each SHR cycle is 4 bytes, and each byte is 4 pixels (2x2 when in 320 mode)
constexpr uint32_t _COLORBYTESOFFSET = 1 + 32;	// the color bytes are offset every line by 33 (after SCBs and palette)


constexpr uint32_t A2VIDEORENDER_ERROR = UINT32_MAX;			// render error
constexpr int32_t SDLUSEREVENT_A2NEWFRAME = 1001;				// user code for new frame event
constexpr int32_t MAX_USEREVENTS_IN_QUEUE = 4;					// maximum frames in queue when VSYNC to Apple 2 bus

class A2VideoManager
{
public:

	//////////////////////////////////////////////////////////////////////////
	// Extra structs
	//////////////////////////////////////////////////////////////////////////

		// NOTE:	Anything labled "id" is an internal identifier by the GPU
		//			Anything labled "index" is an actual array or vector index used by the code

	// We'll create 2 BeamRenderVRAMs objects, for double buffering
	struct BeamRenderVRAMs {
		uint32_t id = 0;
		uint64_t frame_idx = 0;
		bool bWasRendered = false;
		A2Mode_e mode = A2Mode_e::NONE;
		uint8_t* vram_legacy = nullptr;
		uint8_t* vram_shr = nullptr;
		uint8_t* vram_pal256 = nullptr;			// special vram for mode SHR4 PAL256. 2 bytes of color per byte of shr
		uint8_t* vram_forced_text1 = nullptr;	// these force specific modes for debugging
		uint8_t* vram_forced_text2 = nullptr;
		uint8_t* vram_forced_hgr1 = nullptr;
		uint8_t* vram_forced_hgr2 = nullptr;
		GLfloat* offset_buffer = nullptr;
		int frameSHR4Modes = 0;					// All SHR4 modes in the frame
		int doubleSHR4Mode = 0;			// DoubleSHR4Mode_e : may use E0 (main) $2000-9FFF for interlace or page flip
	};

	//////////////////////////////////////////////////////////////////////////
	// Attributes
	//////////////////////////////////////////////////////////////////////////

	OpenGLHelper::ImageAsset image_assets[6];
	std::unique_ptr<A2WindowBeam> windowsbeam[A2VIDEOBEAM_TOTAL_COUNT];	// beam racing GPU render
	std::unique_ptr<VidHdWindowBeam> vidhdWindowBeam;					// VidHD Text Modes GPU render

	uint64_t current_frame_idx = 0;
	uint64_t rendered_frame_idx = UINT64_MAX;

    bool bShouldReboot = false;             // When an Appletini reboot packet arrives
	uXY ScreenSize();

	bool bAlwaysRenderBuffer = false;		// If true, forces a rerender even if the VRAM hasn't changed

	// Multi-Mode Prefs
	bool bAlignQuadsToScanline = false;		// Forces all the quads to align to the same scanline (for all modes)
	bool bForceSHRWidth = false;			// Forces the legacy to have the SHR width, only in Merge mode
	bool bNoMergedModeWobble = false;		// Don't pixel shift the sine wobble if both SHR and Legacy are on screen

	bool bDEMOMergedMode = false;			// DEMO to show merged mode

	// Enable manually setting a DHGR mode that mixes 140 width 16-col and 560 width b/w
	// It was available in certain RGB cards like the Apple and Chat Mauve RGB cards
	// It could be set in software using a combination of soft switches but due to potential
	// conflicts we'll just let the users choose this mode whenever they want to
	bool bUseDHGRCOL140Mixed = false;
	// And a HGR mode that forces the centered dot around 11011 to be black
	bool bUseHGRSPEC1 = false;
	// And a HGR mode that forces the centered dot around 00100 to be white
	bool bUseHGRSPEC2 = false;
	
	int eA2MonitorType = A2_MON_COLOR;

	MemoryEditor mem_edit_vram_legacy;
	MemoryEditor mem_edit_vram_shr;
	MemoryEditor mem_edit_offset_buffer;
	// Developer flags for specifically rendering certain legacy modes
	// Those will be shown in ImGUI windows
	bool bRenderTEXT1 = false;
	bool bRenderTEXT2 = false;
	bool bRenderHGR1 = false;
	bool bRenderHGR2 = false;

	//////////////////////////////////////////////////////////////////////////
	// Methods
	//////////////////////////////////////////////////////////////////////////

	bool IsReady();		// true after full initialization
	void DisplayCharRomsImGuiChunk();
	void DisplayImGuiLoadFileWindow(bool* p_open);
	void DisplayImGuiExtraWindows();
	void DisplayImGuiWindow(bool* p_open);
	void ToggleA2Video(bool value);

	// Overlay String drawing
	void DrawOverlayString(const std::string& text, uint8_t colors, uint32_t x, uint32_t y);
	void DrawOverlayString(const char* text, uint8_t len, uint8_t colors, uint32_t x, uint32_t y);
	void DrawOverlayCharacter(const char c, uint8_t colors, uint32_t x, uint32_t y);
	void EraseOverlayRange(uint8_t len, uint32_t x, uint32_t y);
	void EraseOverlayCharacter(uint32_t x, uint32_t y);

	// Methods for the single multipurpose beam racing shader
	void BeamIsAtPosition(uint32_t _x, uint32_t _y);

	void ForceBeamFullScreenRender(const uint64_t numFrames = 1);
	
	bool SelectLegacyShader(const int index);
	bool SelectSHRShader(const int index);

	const uint32_t GetVRAMReadId() { return vrams_read->id; };
	const uint8_t* GetLegacyVRAMReadPtr() { return vrams_read->vram_legacy; };
	const uint8_t* GetSHRVRAMReadPtr() { return vrams_read->vram_shr; };
	const uint8_t* GetSHRVRAMInterlacedReadPtr() { return vrams_read->vram_shr + sizeof(vrams_read->vram_shr) / _INTERLACE_MULTIPLIER; };
	const uint8_t* GetPAL256VRAMReadPtr() { return vrams_read->vram_pal256; };
	const uint8_t* GetTEXT1VRAMReadPtr() { return vrams_read->vram_forced_text1; };
	const uint8_t* GetTEXT2VRAMReadPtr() { return vrams_read->vram_forced_text2; };
	const uint8_t* GetHGR1VRAMReadPtr() { return vrams_read->vram_forced_hgr1; };
	const uint8_t* GetHGR2VRAMReadPtr() { return vrams_read->vram_forced_hgr2; };
	const GLfloat* GetOffsetBufferReadPtr() { return vrams_read->offset_buffer; };
	uint8_t* GetLegacyVRAMWritePtr() { return vrams_write->vram_legacy; };
	uint8_t* GetSHRVRAMWritePtr() { return vrams_write->vram_shr; };
	uint8_t* GetSHRVRAMInterlacedWritePtr() { return vrams_write->vram_shr + sizeof(vrams_write->vram_shr) / _INTERLACE_MULTIPLIER; };
	GLfloat* GetOffsetBufferWritePtr() { return vrams_write->offset_buffer; };
	GLuint GetOutputTextureId();		// merged output
	bool Render(GLuint &texUnit);	// outputs the texture unit used, and returns if it rendered or not

	inline uint32_t GetVramWidthLegacy() { return (40 + (2 * borders_w_cycles)); };	// in 4 bytes!
	inline uint32_t GetVramHeightLegacy() { return  (192 + (2 * borders_h_scanlines)); };
	inline uint32_t GetVramSizeLegacy() { return (GetVramWidthLegacy() * GetVramHeightLegacy() * 4); };	// in bytes

	inline uint32_t GetVramWidthSHR() { return (_COLORBYTESOFFSET + (2 * borders_w_cycles * 4) + 160); };	// in bytes
	inline uint32_t GetVramHeightSHR() { return  (200 + (2 * borders_h_scanlines)); };
	inline uint32_t GetVramSizeSHR() { return (GetVramWidthSHR() * GetVramHeightSHR() * _INTERLACE_MULTIPLIER); };	// in bytes

	inline const VideoRegion_e GetCurrentRegion() { return current_region; };

	// Changing borders reinitializes everything
	// Cycle for width (7 or 8 (SHR) lines per increment)
	// And a height (8 lines per increment)
	// Call CheckSetBordersWithReinit() at the start of the main loop
	void CheckSetBordersWithReinit();
	uint32_t GetBordersWidthCycles() const { return borders_w_cycles; }
	uint32_t GetBordersHeightScanlines() const { return borders_h_scanlines; }
	// The input is x,y,width,height where x,y are top left origin. The output is SDL style inverted Y
	SDL_FRect NormalizePixelQuad(const SDL_FRect& pixelQuad);
	SDL_FRect CenteredQuadInFramebuffer(const SDL_FRect& quad);
	SDL_FRect CenteredQuadInFramebufferWithOffset(const SDL_FRect& quad, const SDL_FPoint& offset);

	nlohmann::json SerializeState();
	void DeserializeState(const nlohmann::json &jsonState);
	
	void Initialize();
	void ResetComputer();
	
	// public singleton code
	static A2VideoManager* GetInstance()
	{
		if (NULL == s_instance)
			s_instance = new A2VideoManager();
		return s_instance;
	}
	~A2VideoManager();

private:
	static A2VideoManager* s_instance;
	A2VideoManager()
	{
		vrams_array = new BeamRenderVRAMs[2]{};
		Initialize();
	}
	void StartNextFrame();
	void SwitchToMergedMode(uint32_t scanline);
	void CreateOrResizeFramebuffer(int fb_width, int fb_height);
	void InitializeFullQuad();
	void PrepareOffsetTexture();
	void ResetGLData();

	//////////////////////////////////////////////////////////////////////////
	// Internal data
	//////////////////////////////////////////////////////////////////////////
	bool bIsReady = false;
	bool bA2VideoEnabled = true;			// Is standard Apple 2 video enabled?
	bool bShouldInitializeRender = true;	// Used to tell the render method to run initialization
	bool bIsRebooting = false;              // Rebooting semaphore
	bool bIsSwitchingToMergedMode = false;	// True when refreshing earlier scanlines for merged mode
	bool bShouldDoubleSHR = false;			// Handles updating E0 (main) for double SHR

	// imgui vars
	bool bImguiWindowIsOpen = false;
	bool bImguiLoadFileWindowIsOpen = false;
	bool bImguiMemLoadAuxBank = false;
	int iImguiMemLoadPosition = 0;
	int overrideSHR4Mode = 0;				// Cached here to keep the value between A2WindowBeam resets
	int overrideDoubleSHR = 0;				// At 0, don't override. Above 0, substract 1 to get the override value
	int overrideVidHDTextMode = VIDHDMODE_NONE;
	int c022TextColorForeNibble = 0;
	int c022TextColorBackNibble = 0;
	int vidHdTextAlphaForeNibble = 0b1111;
	int vidHdTextAlphaBackNibble = 0b1111;
	std::string sImguiLoadPath = ".";
	float bWobblePower = 0.200;

	// beam render state variables
	BeamState_e beamState = BeamState_e::UNKNOWN;
	int scanlineSHR4Modes = 0;			// All SHR4 modes in the scanline

	// Double-buffered vrams
	BeamRenderVRAMs* vrams_array;	// 2 buffers of legacy+shr vrams
	BeamRenderVRAMs* vrams_write;	// the write buffer
	BeamRenderVRAMs* vrams_read;	// the read buffer

	// Font ROMs array
	// The font files must be 256 characters, each 14x16px. 16 rows and 16 columns.
	std::vector<std::string> font_roms_array;
	int font_rom_regular_idx = 0;
	int font_rom_alternate_idx = 1;

	Shader shader_merge = Shader();

	VideoRegion_e current_region = VideoRegion_e::NTSC;
	uint32_t region_scanlines = (current_region == VideoRegion_e::NTSC ? SC_TOTAL_NTSC : SC_TOTAL_PAL);

	GLint last_viewport[4];		// Previous viewport used, so we don't clobber it
	GLuint FBO_A2Video = UINT_MAX;			// the framebuffer object
	GLuint a2video_texture_id = UINT_MAX;	// the generated texture

	// for debugging, displaying textures TEXT1/2, HGR1/2
	GLuint FBO_debug[4] = { UINT_MAX, UINT_MAX, UINT_MAX, UINT_MAX };
	GLuint debug_texture_id[4] = { UINT_MAX, UINT_MAX, UINT_MAX, UINT_MAX };

	GLuint quadVAO = UINT_MAX;	// FOR MERGED MODE TODO: GET RID OF THAT
	GLuint quadVBO = UINT_MAX;

	// OFFSET buffer texture. Holds one signed int for each scanline to tell the shader
	// how much to offset by x a line for the sine wobble of the merge
	// The offset is negative for 14->16MHz and positive for 16->14MHz
	// The offset curve is:
	//			negative_offset = 4 - 1.1205^(d+28)
	//			positive_offset = 4 - 1.1205^(-d+28)
	// where d is the distance in scanlines from the mode switch
	// Essentially the curve moves from +/- 16 pixels when d is 0 back down to 0 when d is 20
	bool offsetTextureExists = false;
	unsigned int OFFSETTEX = UINT_MAX;
	A2Mode_e merge_last_change_mode = A2Mode_e::NONE;
	uint32_t merge_last_change_y = UINT_MAX;

	// Those could be anywhere up to 6 or 7 cycles for horizontal borders
	// and a lot more for vertical borders. We just decided on a size
	// But SHR starts VBLANK just like legacy modes, at scanline 192. Hence
	// it has 8 less bottom border scanlines than legacy.
	uint32_t borders_w_cycles = 3;
	uint32_t borders_h_scanlines = 8 * 2;	// multiple of 8
	// Requested border sizes from the UI. Main thread checks if the border
	// is different than the requested size and sets the borders
	int border_w_slider_val = 0;
	int border_h_slider_val = 0;

	// The merged framebuffer width is going to be shr + border
	GLint fb_width = 0;
	GLint fb_height = 0;

	// user event for new frame
	SDL_Event event_newframe;
	// used to determine how many user events are active
	SDL_Event user_events_active[MAX_USEREVENTS_IN_QUEUE];

	// Overlay strings handling
	uint8_t overlay_text[_OVERLAY_CHAR_WIDTH *24];	// text for each overlay
	uint8_t overlay_colors[_OVERLAY_CHAR_WIDTH *24];
	uint8_t overlay_lines[24];
	bool bWasSHRBeforeOverlay = false;
	void UpdateOverlayLine(uint32_t y);
};
#endif // A2VIDEOMANAGER_H

