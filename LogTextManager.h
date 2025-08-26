//
//  LogTextManager.h
//  SuperDuperDisplay
//
//  Created by Henri Asseily on 28/07/2025.
//

/*
 Singleton postprocessing class that inherits from TimedTextManager
 and provides a simple running log on the screen. It can roll up or down.
 It efficiently uses a single draw call.

 WARNING: Get its instance only after OpenGL has been initialized!

 There's also a convenience class called LogStream that operates like std::cerr:
 LogStream() << "Error: " << myErr << std::endl;
 */

#include "TimedTextManager.h"

#define LOGTEXT_FONT "./assets/Monaco.ttf"
constexpr uint32_t LOGTEXT_DURATION_MS = 10000;
constexpr float LOGTEXT_FONTSIZE = 18.f;
constexpr glm::vec2 LOGTEXT_PADDING = glm::vec2(15.f, 15.f); // how far from the screen edge
constexpr glm::vec4 LOGTEXT_COLOR = glm::vec4(1.f,1.f,1.f,1.f);

enum class TTLogPosition_e
{
	BOTTOM_LEFT = 0,			// older entries scroll up
	TOP_LEFT,					// older entries scroll down
	TTLOGPOSITION_TOTAL_COUNT
};

class LogTextManager : TimedTextManager {
public:
	// public singleton code
	static LogTextManager* GetInstance()
	{
		if (NULL == s_instance)
			s_instance = new LogTextManager();
		return s_instance;
	}

	// Add an entry to the log. Entry will wrap as necessary
	void AddLog(const std::string& text, glm::vec4 textColor = LOGTEXT_COLOR);
	// Call UpdateAndRender from within the render code, before the framebuffer is unbound
	// Use shouldFlipY to align based on OGL or SDL
	void UpdateAndRender(bool shouldFlipY = false);

	nlohmann::json SerializeState();
	void DeserializeState(const nlohmann::json& jsonState);

	// For log, set the starting position. Default is bottom left, scrolling upwards
	TTLogPosition_e logPosition = TTLogPosition_e::BOTTOM_LEFT;
	// How long a log line lasts on screen. Default is LOGTEXT_DURATION_MS
	uint32_t logDurationMS = LOGTEXT_DURATION_MS;

private:
	// Singleton pattern
	static LogTextManager* s_instance;
	LogTextManager() {
		Initialize(std::string(LOGTEXT_FONT), LOGTEXT_FONTSIZE);
		verts.reserve(120 * 240 * 6 * 8);	// 120 lines of 240 characters per line
	}

};

/*
	LogStream is a convenience class that operates like std::cerr and allows for
	manipulators like std::endl:
	LogStream() << "This is an error: " << myErr << std::endl;

	And LogStreamErr() automatically draws in red
*/

#include <sstream>
#include <string>

class LogStream {
public:
	explicit LogStream(const glm::vec4& color = kDefaultColor) : color_(color) {}
	virtual ~LogStream() {
		if (!oss_.str().empty()) {
			LogTextManager::GetInstance()->AddLog(oss_.str(), color_);
		}
	}

	template <typename T>
	LogStream& operator<<(const T& value) {
		oss_ << value;
		return *this;
	}

	// manipulators like std::endl
	LogStream& operator<<(std::ostream& (*manip)(std::ostream&)) {
		manip(oss_);
		return *this;
	}

protected:
	std::ostringstream oss_;
	glm::vec4 color_;

private:
	static constexpr glm::vec4 kDefaultColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
};

// Derived class for error (red)
class LogStreamErr : public LogStream {
public:
	LogStreamErr() : LogStream(kErrorColor) {}
private:
	static constexpr glm::vec4 kErrorColor = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
};