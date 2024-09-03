#pragma once
#ifndef SSI263_H
#define SSI263_H

//
//  SSI263.h
/*
	Partial implementation of SSI263 speech chip for SuperDuperDisplay
	Uses hard-coded phoneme data in SSI263Phonemes.h
	Each SSI263 is self-contained with its own SDL audio device.
	The current phoneme loops infinitely as long as the chip CTL==0 (or amplitude > 0).

	IMPLEMENTED:
		- Phonemes
		- Amplitude (volume)
		- Phoneme Duration (cutoff after 25-50-75-100%)
		- Immediate Inflection
		- Speech Rate (speed)

	NOT IMPLEMENTED:
		- Filter Frequency
		- FRAME_IMMEDIATE Duration mode
		- Articulation
		- Transitioned Inflection

	 A phoneme should loop infinitely unless the phoneme is changed or CTL=1.
	 If amplitude=0 it is still playing
	 IRQ (i.e. "Phoneme is generated") is set at the end of the phoneme
	 If IRQ is reset while the phoneme is playing, it is set again at the end
	 IRQ is only reset in the following cases:
	 - writes to registers 0,1,2
	 - write to register 3 only if CTL=1
	 CTL=1 sets "Power Down"
	 CTL=0 activates power, durationMode is set and phoneme will play
	 On Power-On (from AppleWin):
		- CTL 	= 1
		- D7 	= 0
		- filterFrequency = 0xFF (register 4)
	 
	 durationMode is only set when CTL switches from 1 to 0. The 2 bits of duration
	 hence have 2 uses: normally they set the phoneme duration (default is value 0),
	 but also to set the durationMode when CTL goes low. In general the durationMode is set
	 once at the beginning of the speech, and kept as-is because CTL won't move from 0.
 */

#include <stdio.h>
#include <vector>
#include <SDL.h>

constexpr int SSI263_SAMPLE_RATE = 22050;

enum SSI263DurationModes_e
{
	SSI263DR_DISABLED_AR 			= 0b00,	// DR1=0 DR0=0
	SSI263DR_FRAME_IMMEDIATE		= 0b01,	// DR1=0 DR0=1	NOT IMPLEMENTED
	SSI263DR_PHONEME_IMMEDIATE		= 0b10,	// DR1=1 DR0=0
	SSI263DR_PHONEME_TRANSITIONED 	= 0b11	// DR1=1 DR0=1
};

class SSI263 {
public:
	SSI263();
	~SSI263();
	void Update();
	void ResetRegisters();
	void SetRegisterSelect(int val);	// Set RS2->RS0 (A2->A0)
	void SetData(int data);				// Set D7->D0 data pins
	void SetReadMode(bool pinState);	// R/W mode, true for R
	void SetCS0(bool pinState);			// Linked to A6 (1st SSI) or A5 (2nd SSI)
	void SetCS1(bool pinState);			// !IOSELECT
	bool GetAR();						// Returns value of A/!R pin
	int GetData();						// 0x00 or 0x80, only in read mode (reads D7)
	bool IsPlaying();
	
	// Call WasIRQTriggered() on every update. Guaranteed to only
	// return true once per IRQ (if true, subsequent calls are false until
	// IRQ triggers again)
	bool WasIRQTriggered();
private:
	// CONTROL data
	// All the below have immediate effect
	int speechRate;
	int filterFrequency;
	int articulationRate;
	int durationMode;
	// TARGET data
	// Upon loading "target" data the device will begin to move
	// towards that target at the prescribed transition rates
	int phoneme;
	int phonemeDuration;	// 0 is 100%, 3 is 25%
	int amplitude;
	// Inflection is special and depends on the duration mode
	int inflection;
	bool regCTL;	// control value, in register 3 bit 7
	
	// Relevant pins
	int registerSelect;		// RS2->RS0
	int byteData;			// D7->D0
	bool pinRead;			// R/!W
	bool pinCS0;			// switching cs0 to ON writes the data
	bool pinCS1;			// !IOSELECT
	
	bool irqIsSet = false;
	bool irqShouldProcess = false;

	void LoadRegister();

	SDL_AudioSpec audioSpec;
	SDL_AudioDeviceID audioDevice;
	std::vector<uint8_t> v_samples;
	int m_currentSampleIdx = 0;
	void GeneratePhonemeSamples();
	static void AudioCallback(void* userdata, uint8_t* stream, int len);
};

#endif /* SSI263_H */
