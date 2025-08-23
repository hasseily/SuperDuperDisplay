#
# Cross Platform Makefile
# Compatible with MSYS2/MINGW, Ubuntu 14.04.1 and Mac OS X
#
# You will need SDL2 (http://www.libsdl.org) and zlib:
# Linux:
#   apt-get install libsdl2-dev
#   apt-get install zlib (could be already installed using a different package name)
# Mac OS X:
#   brew install sdl2
#   brew install zlib
# MSYS2:
#	pacman -S gcc
#	pacman -S autotools
#	pacman -S make
#   pacman -S mingw-w64-ucrt-x86_64-SDL2
#   pacman -S mingw-w64-ucrt-x86_64-zlib
#
# You will also need, for all platforms, the FTDI USB drivers
# They can be downloaded at: https://ftdichip.com/drivers/d3xx-drivers/
# Read the documentation for installation
#

#CXX = g++
#CXX = clang++

EXE = SuperDuperDisplay
IMGUI_DIR = imgui
SOURCES = main.cpp OpenGLHelper.cpp MosaicMesh.cpp MemoryManager.cpp SDHRNetworking.cpp SDHRManager.cpp SDHRWindow.cpp TimedTextManager.cpp LogTextManager.cpp
SOURCES += A2VideoManager.cpp A2WindowBeam.cpp A2WindowRGB.cpp shader.cpp PostProcessor.cpp CycleCounter.cpp EventRecorder.cpp SoundManager.cpp
SOURCES += Ayumi.cpp MockingboardManager.cpp SSI263.cpp MainMenu.cpp VidHdWindowBeam.cpp BasicQuad.cpp
SOURCES += extras/MemoryLoader.cpp extras/ImGuiFileDialog.cpp miniz.c
SOURCES += glad/glad.cpp
SOURCES += $(IMGUI_DIR)/imgui.cpp $(IMGUI_DIR)/imgui_demo.cpp $(IMGUI_DIR)/imgui_draw.cpp $(IMGUI_DIR)/imgui_tables.cpp $(IMGUI_DIR)/imgui_widgets.cpp
SOURCES += $(IMGUI_DIR)/backends/imgui_impl_sdl2.cpp $(IMGUI_DIR)/backends/imgui_impl_opengl3.cpp
OBJS = $(addsuffix .o, $(basename $(notdir $(SOURCES))))
UNAME_S := $(shell uname -s)
LINUX_GL_LIBS = -lGL -lftd3xx

CXXFLAGS = -std=c++17 -I$(IMGUI_DIR) -I$(IMGUI_DIR)/backends -Iglad
CXXFLAGS += -Wall -Wformat -Wno-unused-function -Wno-unknown-pragmas
CONFIGFLAGS =
LIBS =

##---------------------------------------------------------------------
## OPENGL ES
##---------------------------------------------------------------------

## This assumes a GL ES library available in the system, e.g. libGLESv2.so

ifeq ($(UNAME_S), Linux) #LINUX
	CXXFLAGS += -DIMGUI_IMPL_OPENGL_ES2
	LINUX_GL_LIBS = -lGLESv2 -lftd3xx
endif
## If you're on a Raspberry Pi and want to use the legacy drivers,
## use the following instead:
# LINUX_GL_LIBS = -L/opt/vc/lib -lbrcmGLESv2

##---------------------------------------------------------------------
## BUILD FLAGS PER PLATFORM
##---------------------------------------------------------------------

ifeq ($(UNAME_S), Linux) #LINUX
	ECHO_MESSAGE = "Linux"
	LIBS += $(LINUX_GL_LIBS) -l:libz.a -lpthread -ldl `sdl2-config --libs`

	CXXFLAGS += `sdl2-config --cflags`
	CFLAGS = $(CXXFLAGS)
endif

ifeq ($(UNAME_S), Darwin) #APPLE
	ECHO_MESSAGE = "Mac OS X"
	LIBS += -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo `/opt/homebrew/bin/sdl2-config --libs` -lz
	LIBS += -framework CoreFoundation -lftd3xx-static
	LIBS += -L/usr/local/lib -L/opt/homebrew/lib -Llib/OSX

	CXXFLAGS += `/opt/homebrew/bin/sdl2-config --cflags`
	CXXFLAGS += -I/usr/local/include -I/opt/homebrew/include
	CFLAGS = $(CXXFLAGS)
endif

ifeq ($(OS), Windows_NT)
    ECHO_MESSAGE = "MinGW"
    LIBS += -llibz -lgdi32 -lopengl32 -limm32 -lWs2_32 `pkg-config --static --libs sdl2` -lftd3xx

    CXXFLAGS += `pkg-config --cflags sdl2`
	CXXFLAGS += -I/ucrt64/include/
    CFLAGS = $(CXXFLAGS)
endif

##---------------------------------------------------------------------
## BUILD RULES
##---------------------------------------------------------------------

%.o:glad/%.cpp
	$(CXX) $(CXXFLAGS) $(CONFIGFLAGS) -c -o $@ $<

%.o:extras/%.cpp
	$(CXX) $(CXXFLAGS) $(CONFIGFLAGS) -c -o $@ $<

%.o:%.cpp
	$(CXX) $(CXXFLAGS) $(CONFIGFLAGS) -c -o $@ $<

%.o:$(IMGUI_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) $(CONFIGFLAGS) -c -o $@ $<

%.o:$(IMGUI_DIR)/backends/%.cpp
	$(CXX) $(CXXFLAGS) $(CONFIGFLAGS) -c -o $@ $<

all: $(EXE)
	@echo Build complete for $(ECHO_MESSAGE)

$(EXE): $(OBJS)
	$(CXX) -o $@ $^ $(CXXFLAGS) $(CONFIGFLAGS) $(LIBS)

clean:
	rm -f $(EXE) $(OBJS)

debug: 	CONFIGFLAGS += -g3 -O0 -DDEBUG -D_DEBUGTIMINGS
debug:	$(EXE)

release: 	CONFIGFLAGS += -Os -DNDEBUG
release:	$(EXE)
