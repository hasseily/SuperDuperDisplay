#ifndef SOUNDMANAGER_H
#define SOUNDMANAGER_H

#include <SDL.h>
#include <vector>
#include <mutex>
#include "nlohmann/json.hpp"
#include "common.h"

// This singleton class manages the Apple 2 speaker sound
// All it needs is to be sent EventReceived(bool isC03x=false) on each cycle.
// Any time the passed in param is true, the speaker is switched low<->high
// and kept in that position until the next param change.

// Unfortunately Linux in some cases pukes when using multiple audio devices
// so we have to do the mixing in SoundManager for anything audio


const uint32_t SM_AUDIO_BUFLEN = 256;					// number of SDL_Audio samples in callback
const uint32_t SM_BUFFER_DRIFT_LIMIT = 4096;			// Size to start trying to reduce the drift. At least 4096
const uint32_t SM_BEEPER_BUFFER_SIZE = SM_AUDIO_BUFLEN * 10;	// circular buffer
const uint32_t SM_BEEPER_DCADJ_BUFLEN = 256;
const float SM_BASE_VOLUME_ADJUSTMENT = 0.6f;			// beeper base volume adjustment

class SoundManager {
public:
	~SoundManager();
	void Initialize();
	void Enable() { bIsEnabled = true; };
	void Disable() { bIsEnabled = false; };
	void BeginPlay();
	void StopPlay();
	bool IsPlaying();
	void EventReceived(bool isC03x = false);	// Received any event -- if isC03x then the event is a 0xC03x
	void SetPAL(bool isPal);				// Sets PAL (true) or NTSC (false)

	// DC Adjustment
	float DCAdjustment(float freq);

	// ImGUI and prefs
	void DisplayImGuiChunk();
	nlohmann::json SerializeState();
	void DeserializeState(const nlohmann::json &jsonState);
	
	// public singleton code
	static SoundManager* GetInstance()
	{
		if (NULL == s_instance)
			s_instance = new SoundManager(_AUDIO_SAMPLE_RATE, SM_AUDIO_BUFLEN);
		return s_instance;
	}
private:
	static SoundManager* s_instance;
	SoundManager(uint32_t sampleRate, uint32_t bufferSize);
	static void AudioCallback(void* userdata, uint8_t* stream, int len);

	SDL_AudioSpec audioSpec;
	SDL_AudioDeviceID audioDevice;
	uint32_t cyclesPerSample;					// Around 23.14. Changes between NTSC and PAL
	uint32_t sampleRate;						// SDL_Audio sample rate
	int bufferSize;								// SDL_Audio buffer size
	bool bIsEnabled = true;						// Did user enable speaker through HDMI?
	bool bIsPlaying;							// Is the audio playing?
	bool bIsPAL = false;						// Is the machine PAL?
	
	uint32_t beeper_samples_idx_read = 0;
	uint32_t beeper_samples_idx_write = 0;
	uint32_t beeper_samples_same_ct = 0;		// count of successive same samples (no freq change)
	float beeper_samples[SM_BEEPER_BUFFER_SIZE];
	float audioCallbackBuffer[SM_AUDIO_BUFLEN * 2] = { 0.f };	// Stereo
	uint64_t ticks_per_sample;	// Depends on NTSC/PAL
	uint64_t curr_tick = 0;	// tick value since the beginning of the sample
	float curr_freq = -1.f;	// current frequency for the sample
	float beeper_volume = 1.f;	// main sound volume
	int ticks_drift_adjustment;	// change to ticks_per_sample to compensate for drift
	int sm_imgui_queued_audio_size = 0;	// for ImGui

	// DC adjustment filter
	float dcadj_sum;
	uint32_t dcadj_pos;
	float dcadj_buf[SM_BEEPER_DCADJ_BUFLEN];
};

#endif // SOUNDMANAGER_H
