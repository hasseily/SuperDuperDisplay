//
//  TimedTextManager.h
//  SuperDuperDisplay
//
//  Created by Henri Asseily on 17/07/2025.
//
//  This class allows you to draw colored text on screen using the default Apple 2 font
//  or a custom font. The text will last a certain number of milliseconds. To use:
//
//  Instance an object via the default constructor, such as:
//  	TimedTextManager timedTextManager;
//  After OpenGL is set up:
//		timedTextManager.Initialize();	// for default Apple 2 font
//	or
//		timedTextManager.Initialize("./assets/ProggyTiny.ttf", 24);
//
//	Note that the default Apple 2 font has a limited set of glyphs. But it is already
//  loaded in the GPU and comes for free.
//
//	Then add any text. Text is drawn in REVERSE ORDER of creation. So if you want to add
//  a shadow, add it after the text itself:
//		timedTextManager.AddText("Hello, world!", 50, 50, 1000);
//		timedTextManager.AddText("Hello, world!", 52, 52, 1000, .1,.1,.1,.9);
//  Finally, call UpdateAndRender() from within the render code while the framebuffer is active:
//		timedTextManager.UpdateAndRender(fb_width, fb_height);
//
//
//  NOTES:
//  - Set use80ColDefaultFont to true to use the 80 col version of the default Apple II font

#pragma once
#include "common.h"
#include "shader.h"
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcomma"
#include "stb_truetype.h"
#pragma clang diagnostic pop

struct TimedText {
	size_t id;			// starts at 1, there is no 0
	std::string text;
	int x, y;
	uint64_t ticksFinish;
	float r, g, b, a;
};

class TimedTextManager {
public:
	// Call Initialize after OpenGL is set up
	// Initialize with default Apple 2 font and size (_TEXUNIT_IMAGE_FONT_ROM_DEFAULT)
	void Initialize();
	// Initialize with custom font
	void Initialize(const std::string& ttfPath, float pixelHeight);
	// Colored text, white by default. Returns the id of the text added.
	const size_t AddText(const std::string& text, int x, int y, uint64_t durationTicks,
				 float r = 1.f, float g = 1.f, float b = 1.f, float a = 1.f);
	// Deletes the text before its scheduled removal. Returns true if the text was deleted
	bool DeleteText(const size_t id);
	// Call UpdateAndRender from within the render code, before the framebuffer is unbound
	// Use shouldFlipY to align based on OGL or SDL
	void UpdateAndRender(bool shouldFlipY = false);
	// use80ColDefaultFont attribute to use the 80 col default font, otherwise 40 col
	bool use80ColDefaultFont = false;
	~TimedTextManager();
private:
	stbtt_bakedchar bakedChars[96];
	stbtt_fontinfo fontInfo;

	std::vector<unsigned char> fontBuffer;
	int ascent = 0;

	std::vector<TimedText> texts;
	bool useDefaultFont = true;
	size_t idCounter = 0;
	
	GLuint vao = 0, vbo = 0, atlasTex = 0;

	Shader shader = Shader();

	void LoadFont(const std::string& path, float pixelHeight);
	void CreateGLObjects();
};
