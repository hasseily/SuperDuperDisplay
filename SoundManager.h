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
// It pushes to the audio subsystem the samples when it reaches enough samples to fill SM_BUFFER_SIZE.
// If there aren't enough samples, SDL2 automatically inserts silence.
// If SDL2 isn't capable of processing samples at the speed they're pushed to the audio system,
// we drop samples once its queue is above SM_BUFFER_SIZE*2.

// TODO: 	Dynamically calculate SM_DEFAULT_CYCLES_PER_SAMPLE based on the Apple 2 region's clock cycle.
//			It is 1/(region_cycle_length*SM_SAMPLE_RATE) , i.e. 1/(usec/cycle*samples/usec)

const uint32_t SM_SAMPLE_RATE = 44100; 					// Audio sample rate
const uint32_t SM_BUFFER_SIZE = 1024;					// SDL_Audio buffer size.
const uint32_t SM_BUFFER_DRIFT_LIMIT_SIZE = 8192;		// Size to start trying to reduce the drift. At least 4096
const float SM_CYCLES_PER_SAMPLE = 23.14f;				// Count of cycles per 44.1kHz sample
const uint32_t SM_BEEPER_BUFFER_SIZE = 256;				// number of beeper samples to buffer before sending to SDL_Audio

class SoundManager {
public:
	~SoundManager();
	void Initialize();
	void Enable() { bIsEnabled = true; };
	void Disable() { bIsEnabled = false; };
	void BeginPlay();
	void StopPlay();
	bool IsPlaying();
	void EventReceived(bool isC03x=false);	// Received any event -- if isC03x then the event is a 0xC03x
	
	// ImGUI and prefs
	void DisplayImGuiChunk();
	nlohmann::json SerializeState();
	void DeserializeState(const nlohmann::json &jsonState);
	
	// public singleton code
	static SoundManager* GetInstance()
	{
		if (NULL == s_instance)
			s_instance = new SoundManager(SM_SAMPLE_RATE, SM_BUFFER_SIZE);
		return s_instance;
	}
private:
	static SoundManager* s_instance;
	SoundManager(uint32_t sampleRate, uint32_t bufferSize);
	
	void QueueAudio();
	
	SDL_AudioSpec audioSpec;
	SDL_AudioDeviceID audioDevice;
	uint32_t cyclesPerSample;					// Around 23.14. Changes between NTSC and PAL
	uint32_t sampleRate;						// SDL_Audio sample rate
	int bufferSize;								// SDL_Audio buffer size
	bool bIsEnabled = true;						// Did user enable speaker through HDMI?
	bool bIsPlaying;							// Is the audio playing?
	
	uint32_t beeper_samples_idx = 0;
	uint32_t beeper_samples_zero_ct = 0;		// count of successive 0 samples
	float beeper_samples[SM_BEEPER_BUFFER_SIZE];
};

#endif // SOUNDMANAGER_H
