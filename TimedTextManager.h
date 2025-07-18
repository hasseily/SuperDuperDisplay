//
//  TimedTextManager.h
//  SuperDuperDisplay
//
//  Created by Henri Asseily on 17/07/2025.
//
//  This class allows you to draw colored text on screen using the default Apple 2 font
//  or a custom font. The text will last a certain number of frames. To use:
//
//  Instance an object via the default constructor, such as:
//  	TimedTextManager timedTextManager;
//  After OpenGL is set up:
//		timedTextManager.Initialize();	// for default Apple 2 font
//	or
//		timedTextManager.Initialize("./assets/ProggyTiny.ttf", 24);
//	Then add any text. Text is drawn in REVERSE ORDER of creation. So if you want to add
//  a shadow, add it after the text itself:
//		timedTextManager.AddText("Hello, world!", 50, 50, 120);
//		timedTextManager.AddText("Hello, world!", 52, 52, 120, .1,.1,.1,.9);
//  Finally, call UpdateAndRender() from within the render code while the framebuffer is active:
//		timedTextManager.UpdateAndRender(fb_width, fb_height);
//
//
//  NOTES:
//  - Set use80ColDefaultFont to true to use the 80 col version of the default Apple II font

#pragma once
#include "common.h"
#include "stb_truetype.h"

struct TimedText {
	std::string text;
	int x, y;
	int framesLeft;
	float r, g, b, a;
};

class TimedTextManager {
public:
	// Call Initialize after OpenGL is set up
	// Initialize with default Apple 2 font and size (_TEXUNIT_IMAGE_FONT_ROM_DEFAULT)
	void Initialize();
	// Initialize with custom font
	void Initialize(const std::string& ttfPath, float pixelHeight);
	// White text
	void AddText(const std::string& text, int x, int y, int durationFrames);
	// Colored text
	void AddText(const std::string& text, int x, int y, int durationFrames,
				 float r, float g, float b, float a);
	// Call UpdateAndRender from within the render code, before the framebuffer is unbound
	void UpdateAndRender(int windowW, int windowH);

	~TimedTextManager();
	bool use80ColDefaultFont = false;
private:
	int winW, winH;
	GLuint vao = 0, vbo = 0, shader = 0, atlasTex = 0;
	GLint  locProj = -1, locTex = -1, locColor = -1;

	stbtt_bakedchar bakedChars[96];
	stbtt_fontinfo fontInfo;

	std::vector<unsigned char> fontBuffer;
	int ascent = 0;

	std::vector<TimedText> texts;

	bool useDefaultFont = true;

	void LoadFont(const std::string& path, float pixelHeight);
	void CreateGLObjects();
	void CreateShader();
	void CheckShader(GLuint shader, const std::string& name);
	void CheckLink(GLuint prog);
};
