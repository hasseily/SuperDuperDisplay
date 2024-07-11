#ifndef SOUNDMANAGER_H
#define SOUNDMANAGER_H

#include <SDL.h>
#include <vector>
#include <mutex>
#include "nlohmann/json.hpp"

// The RINGBUFFER is a buffer that tracks the cycles of when the speaker clicks (0xC03X)
// Cycles are stored as int64 since reset of the machine

const uint32_t SM_SAMPLE_RATE = 44100; 					// Audio sample rate
const uint32_t SM_BUFFER_SIZE = 1024;					// Default buffer size for SDL Audio callback
const uint32_t SM_RINGBUFFER_SIZE = 1'000'000;			// Audio buffer size - At least 4 seconds of speaker clicks (each 0xC03X is 4 cycles)
const uint32_t SM_STARTING_DELTA_READWRITE = SM_BUFFER_SIZE * 10;	// Starting difference between the 2 pointers
const uint32_t SM_DEFAULT_CYCLES_PER_SAMPLE = 231500;	// Default count of events per 44.1kHz sample
const uint32_t SM_CYCLES_DELAY = 50;					// Delay outgoing sound data by 50 cycles compared to incoming
const uint32_t SM_CYCLE_MULTIPLIER = 10000;				// Sampling multiplier for cycles

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
	
	static void AudioCallback(void* userdata, uint8_t* stream, int len);
	void QueueAudio();
	
	std::mutex pointerMutex;
	SDL_AudioSpec audioSpec;
	SDL_AudioDeviceID audioDevice;
	uint32_t cyclesPerSample;					// Around 23 * SM_CYCLE_MULTIPLIER. Increases or decreases based on how quickly reads catch up
	uint32_t sampleRate;
	int bufferSize;
	bool bIsEnabled = true;						// Did user enable speaker through HDMI?
	bool bIsPlaying;							// Is the audio playing?
	float sampleAverage;
	uint32_t sampleCycleCount;					// Varies around 23 * SM_CYCLE_MULTIPLIER
	uint32_t cyclesHigh;							// speaker cycles that are on for each sample
	bool bIsLastHigh;								// Did the last on cycle go positive or negative?
	float soundRingbuffer[SM_RINGBUFFER_SIZE];// 5 seconds of buffer to feed the 44.1kHz audio
	uint32_t sb_index_read;						// index in soundRingbuffer where read should start
	uint32_t sb_index_write;					// index in soundRingbuffer where write should start
	
	int lastQueuedSampleCount;
};

#endif // SOUNDMANAGER_H
