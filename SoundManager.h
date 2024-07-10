#ifndef SOUNDMANAGER_H
#define SOUNDMANAGER_H

#include <SDL.h>
#include <vector>
#include <mutex>
#include "nlohmann/json.hpp"

const uint32_t SM_SAMPLE_RATE = 44100; 					// Audio sample rate
const uint32_t SM_BUFFER_SIZE = 256;					// Default buffer size for SDL Audio callback
const uint32_t SM_RINGBUFFER_SIZE = SM_SAMPLE_RATE * 5;	// Audio buffer size (5 seconds)
const uint32_t SM_EVENTS_PER_SPEAKER_PHASE = 1'023;		// Apple at 1MHz. Speaker at 1KHz. NTSC is 1'022'727 and PAL is 1'017'857
const uint32_t SM_STARTING_DELTA_READWRITE = SM_BUFFER_SIZE * 10;	// Starting difference between the 2 pointers
const uint32_t SM_DEFAULT_EVENTS_PER_SAMPLE = 23;		// Default count of events per 44.1kHz sample

class SoundManager {
public:
	~SoundManager();
	void Initialize();
	void Enable() { bIsEnabled = true; };
	void Disable() { bIsEnabled = false; };
	void BeginPlay();
	void StopPlay();
	bool IsPlaying();
	void ToggleSoundState();		// Toggles between silence and 1kHz
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
	uint8_t eventsPerSample;					// Around 23. Increases or decreases based on how quickly reads catch up
	uint32_t sampleRate;
	int bufferSize;
	bool bIsEnabled = true;						// Did user enable speaker through HDMI?
	bool bIsPlaying;							// Is the audio playing?
	bool bIsSoundOn;							// Sound state of the 0xC03x speaker (silence or 1kHz
	float sampleAverage;
	uint8_t sampleEventCount;					// up to SM_EVENTS_PER_SAMPLE
	uint32_t speakerPhaseEventCount;			// up to SM_EVENTS_PER_SPEAKER_PHASE
	char soundRingbuffer[SM_RINGBUFFER_SIZE];	// 5 seconds of buffer to feed the 44.1kHz audio
	uint32_t sb_index_read;						// index in soundRingbuffer where read should start
	uint32_t sb_index_write;					// index in soundRingbuffer where write should start
};

#endif // SOUNDMANAGER_H
