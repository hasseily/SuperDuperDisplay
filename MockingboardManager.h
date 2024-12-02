#ifndef MOCKINGBOARDMANAGER_H
#define MOCKINGBOARDMANAGER_H

/*
	This class emulates one or 2 mockingboards, each with the following chips:
		2x M6522, 2x AY-3-8913, 2x SSI263
	They're in slot 4 and 5. This class is NOT a complete mockingboard emulator,
	it doesn't deal with timers or read requests. Those just cannot be handled on
	the host computer due to the latency between the Appletini and the host.

	* Notes on the Mockingboards:
	* Do not assume any panning for the AY channels beyond the fact that each AY with
	* its 3 channels maps to the same L-R pan. The original MB-A maps AY1 left and AY2
	* right, although AY-2 has some crosstalk in the left channel. The new ReactiveMicro
	* v2.1 maps both AYs to both L+R but slightly more left.
	* 
	* In both cases of MB-A and RM-2.1, AY1 is _significantly_ more powerful than AY2
	* and outputs close to 4x the dB of AY2. If the AY2 pot in MB-A is at max, then to
	* balance out AY1 its pot needs to be at 1/4.
 */

#include <stdio.h>
#include <SDL.h>
#include "Ayumi.h"
#include "SSI263.h"
#include "nlohmann/json.hpp"
#include "common.h"

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

// Codes for the AY-8913 come from pins BC1, BC2 and BDIR (from low to high bit)
// BC2 is tied high to +5V
// The first 2 bits of the value will be BC1 and BDIR. The 3rd bit is !RESET
enum A2MBCodes_e
{
	A2MBC_INACTIVE = 0,		// Always used after any of the other codes is used
	A2MBC_READ,				// Read data from the latched register
	A2MBC_WRITE,			// Write data to the latched register
	A2MBC_LATCH,			// Set register number (i.e. "latch" a register)
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
	bool IsEnabled() { return bIsEnabled; };

	// Received a mockingboard event, we don't care if it's C4XX or C5XX
	void EventReceived(uint16_t addr, uint8_t val, bool rw);
	
	// Audio callback
	void GetSamples(float& left, float& right);

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
	
	// Speak a demo phrase
	void Util_SpeakDemoPhrase();
	// ====================
	
	// ImGUI and prefs
	void DisplayImGuiChunk();
	nlohmann::json SerializeState();
	void DeserializeState(const nlohmann::json &jsonState);
	
	// public singleton code
	static MockingboardManager* GetInstance()
	{
		if (NULL == s_instance)
			s_instance = new MockingboardManager(_AUDIO_SAMPLE_RATE);
		return s_instance;
	}
private:
	static MockingboardManager* s_instance;
	MockingboardManager(uint32_t sampleRate);
	
	void UpdateAllPans();
	void SetLatchedRegister(Ayumi* ayp, uint8_t value);
	
	uint32_t sampleRate;
	uint32_t bufferSize;
	bool bIsEnabled = true;
	bool bIsDual = true;
	bool bIsPlaying;
	int mb_event_count = 0;
	
	// Chips
	Ayumi ay[4];
	SSI263 ssi[4];

	// M6522 pin state
	uint64_t a_pins_in[4] = { 0 };
	uint64_t a_pins_out[4] = { 0 };
	uint64_t a_pins_out_prev[4] = { 0 };

	float allpans[4][3] = {
		0.3f, 0.3f, 0.3f,	// AY0 pans left
		0.7f, 0.7f, 0.7f,	// AY1 pans right
		0.2f, 0.2f, 0.2f,	// AY2 pans left
		0.8f, 0.8f, 0.8f,	// AY3 pans right
	};
};

#endif // MOCKINGBOARDMANAGER_H
