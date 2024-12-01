#ifndef SOUNDMANAGER_H
#define SOUNDMANAGER_H

#include <SDL.h>
#include <vector>
#include <mutex>
#include "nlohmann/json.hpp"

// This singleton class manages the Apple 2 speaker sound
// All it needs is to be sent EventReceived(bool isC03x=false) on each cycle.
// Any time the passed in param is true, the speaker is switched low<->high
// and kept in that position until the next param change.

// Internally it uses a ringbuffer to store samples to send to the audio subsystem.
// It pushes to the audio subsystem the samples when it reaches enough samples to fill SM_BUFLEN.
// If there aren't enough samples, SDL2 automatically inserts silence.
// If SDL2 isn't capable of processing samples at the speed they're pushed to the audio system,
// we drop samples once its queue is above SM_BUFLEN*2.


const uint32_t SM_SAMPLE_RATE = 44100; 					// Audio sample rate
const uint32_t SM_BUFLEN = 256;							// SDL_Audio buffer size.
const uint32_t SM_BEEPER_BUFLEN = 256;					// number of beeper samples to buffer before sending to SDL_Audio
const uint32_t SM_BUFFER_DRIFT_LIMIT = 4096;			// Size to start trying to reduce the drift. At least 4096
const uint32_t SM_TICKS_PER_CYCLE = 1'000'000;			// All calculations are made in integers using this multiplier
const uint32_t SM_BEEPER_DCADJ_BUFLEN = 256;
const float SM_BASE_VOLUME_ADJUSTMENT = 0.5f;			// Global volume adjustment

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
			s_instance = new SoundManager(SM_SAMPLE_RATE, SM_BUFLEN);
		return s_instance;
	}
private:
	static SoundManager* s_instance;
	SoundManager(uint32_t sampleRate, uint32_t bufferSize);
	
	SDL_AudioSpec audioSpec;
	SDL_AudioDeviceID audioDevice;
	uint32_t cyclesPerSample;					// Around 23.14. Changes between NTSC and PAL
	uint32_t sampleRate;						// SDL_Audio sample rate
	int bufferSize;								// SDL_Audio buffer size
	bool bIsEnabled = true;						// Did user enable speaker through HDMI?
	bool bIsPlaying;							// Is the audio playing?
	bool bIsPAL = false;						// Is the machine PAL?
	
	uint32_t beeper_samples_idx = 0;
	uint32_t beeper_samples_same_ct = 0;		// count of successive same samples (no freq change)
	float beeper_samples[SM_BEEPER_BUFLEN];
	uint64_t ticks_per_sample;	// Depends on NTSC/PAL
	uint64_t curr_tick = 0;	// tick value since the beginning of the sample
	float curr_freq = -1.f;	// current frequency for the sample
	float beeper_volume = 0.f;	// main sound volume
	bool beeper_state = 0;	// beeper high (1) or low (0)
	bool prev_beeper_state = 0;	// used to check same samples
	int ticks_drift_adjustment;	// change to ticks_per_sample to compensate for drift
	int sm_imgui_queued_audio_size = 0;	// for ImGui

	// DC adjustment filter
	float dcadj_sum;
	uint32_t dcadj_pos;
	float dcadj_buf[SM_BEEPER_DCADJ_BUFLEN];
};

#endif // SOUNDMANAGER_H
