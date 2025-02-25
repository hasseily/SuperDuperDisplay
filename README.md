# SuperDuperDisplay
Display engine for the Apple 2 network bus card "Appletini"

# Platform-specific Compilation
## Mac OS X (XCode)
Download the DMG from https://github.com/libsdl-org/SDL/releases/tag/release-2.30.5 and copy it into /Library/Frameworks.
## Windows (Visual Studio)
TODO...
## MSYS2/MINGW, Ubuntu 14.04.1 and Mac OS X (Makefile)
Use the included Makefile. Check the comments at the top.

# DONE
- Switch to the new USB protocol using the Appletini FPGA
- Implement support for the Mockingboard 6522s' interrupts

# TODO
- Fix the SSI263 Speech implementation and get new phoneme samples
- COL160 DHGR Video-7 mode?
- For non-border pixels, use 4 border color bits in VRAM to store Video-7 & EVE registers
- VidHD Text modes

