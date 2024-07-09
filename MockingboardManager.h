#ifndef MOCKINGBOARDMANAGER_H
#define MOCKINGBOARDMANAGER_H

/*
	This class emulates one or 2 mockingboards with dual AY-3-8910 chips
	They're in slot 4 and 5
 */
#include <stdio.h>
#include <SDL.h>
#include "Ayumi.h"
#include "nlohmann/json.hpp"

const uint32_t MM_SAMPLE_RATE = 44100; 					// Audio sample rate
const uint32_t MM_BUFFER_SIZE = 1024;

// The first AY-3-8910 starts at 0x00
// The second AY3-8910 starts at 0x80
enum A2MBEvent_e
{
	A2MBE_ORB = 0x00,			// Port B (Code)
	A2MBE_ORA = 0x01,			// Port A (Data)
	A2MBE_ODDRB = 0x02,			// Data Direction Register (B)
	A2MBE_ODDRA = 0x03,			// Data Direction Register (A)
	A2MBE_TOTAL_COUNT
};

enum A2MBCodes_e
{
	A2MBC_RESET = 0,			// Used generally on init only
	A2MBC_INACTIVE = 4,			// Always used after any of the other codes is used
	A2MBC_LATCH = 6,			// Set register number (i.e. "latch" a register)
	A2MBC_WRITE = 7,			// Write data to the latched register
	A2MBC_TOTAL_COUNT
};

enum A2MBAYRegisters_e
{
	A2MBAYR_ATONEFINE = 0,
	A2MBAYR_ATONECOARSE,
	A2MBAYR_BTONEFINE,
	A2MBAYR_BTONECOARSE,
	A2MBAYR_CTONEFINE,
	A2MBAYR_CTONECOARSE,
	A2MBAYR_NOISEPER,
	A2MBAYR_ENABLE,
	A2MBAYR_AVOL,
	A2MBAYR_BVOL,
	A2MBAYR_CVOL,
	A2MBAYR_EFINE,
	A2MBAYR_ECOARSE,
	A2MBAYR_ESHAPE
};

class MockingboardManager {
public:
	~MockingboardManager();
	void Initialize();
	void BeginPlay();
	void StopPlay();
	bool IsPlaying();
	void SetDualMode(bool _isDual) { bIsDual = _isDual; };	// set isDual to have 2 mockingboards
	void Enable() { bIsEnabled = true; };
	void Disable() { bIsEnabled = false; };

	// Received a mockingboard event, we don't care if it's C4XX or C5XX
	// All mockingboard events MUST be write events!
	void EventReceived(uint16_t addr, uint8_t val);
	
	// Set the panning of a channel in an AY
	// Pan is 0.0-1.0, left to right
	// Set isEqp for "equal power" panning
	// A mockingboard does _NOT_ have panning capabilities. One AY is assigned to each speaker
	void SetPan(uint8_t ay_idx, uint8_t channel_idx, double pan, bool isEqp);
	
	// ====================
	// Utility test methods
	
	// Resets the ay, i.e. stops the sound
	void Util_Reset(uint8_t ay_idx);
	
	// Writes to a register in one of the 2 ays
	void Util_WriteToRegister(uint8_t ay_idx, uint8_t reg_idx, uint8_t val);
	
	// Writes all registers at once -- val_array is 16 values
	void Util_WriteAllRegisters(uint8_t ay_idx, uint8_t* val_array);
	
	// ====================
	
	// ImGUI and prefs
	void DisplayImGuiChunk();
	nlohmann::json SerializeState();
	void DeserializeState(const nlohmann::json &jsonState);
	
	// public singleton code
	static MockingboardManager* GetInstance()
	{
		if (NULL == s_instance)
			s_instance = new MockingboardManager(MM_SAMPLE_RATE, MM_BUFFER_SIZE);
		return s_instance;
	}
private:
	static MockingboardManager* s_instance;
	MockingboardManager(uint32_t sampleRate, uint32_t bufferSize);
	
	void Process();
	void UpdateAllPans();
	void SetLatchedRegister(Ayumi* ayp, uint8_t value);
	static void AudioCallback(void* userdata, uint8_t* stream, int len);
	
	bool bIsDual;
	SDL_AudioSpec audioSpec;
	SDL_AudioDeviceID audioDevice;
	uint32_t sampleRate;
	uint32_t bufferSize;
	bool bIsEnabled;
	bool bIsPlaying;
	
	Ayumi ay[4];
	float allpans[4][3] = {
		0.0f, 0.0f, 0.0f,	// AY0 pans left
		1.0f, 1.0f, 1.0f,	// AY1 pans right
		0.0f, 0.0f, 0.0f,	// AY2 pans left
		1.0f, 1.0f, 1.0f,	// AY3 pans right
	};
};

#endif // MOCKINGBOARDMANAGER_H
