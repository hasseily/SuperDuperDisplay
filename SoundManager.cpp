#include "SoundManager.h"
#include <iostream>

// below because "The declaration of a static data member in its class definition is not a definition"
SoundManager* SoundManager::s_instance;

// const int TONE_FREQUENCY = 1023; // Apple //e used a ~1 kHz tone

SoundManager::SoundManager(uint32_t sampleRate, uint32_t bufferSize)
: sampleRate(sampleRate), bufferSize(bufferSize), isPlaying(false) {
	if (SDL_Init(SDL_INIT_AUDIO) < 0) {
		std::cerr << "Failed to initialize SDL: " << SDL_GetError() << std::endl;
		throw std::runtime_error("SDL_Init failed");
	}
	audioDevice = 0;
	Initialize();
}

void SoundManager::Initialize()
{
	if (audioDevice == 0)
	{
		SDL_zero(audioSpec);
		audioSpec.freq = sampleRate;
		audioSpec.format = AUDIO_S8;
		audioSpec.channels = 1;
		audioSpec.samples = bufferSize;
		audioSpec.callback = SoundManager::AudioCallback;
		audioSpec.userdata = this;
		
		audioDevice = SDL_OpenAudioDevice(NULL, 0, &audioSpec, NULL, 0);
	}
	
	if (audioDevice == 0) {
		std::cerr << "Failed to open audio device: " << SDL_GetError() << std::endl;
		SDL_Quit();
		throw std::runtime_error("SDL_OpenAudioDevice failed");
	}
}

SoundManager::~SoundManager() {
	SDL_CloseAudioDevice(audioDevice);
	SDL_Quit();
}

void SoundManager::AudioCallback(void* userdata, uint8_t* stream, int len) {
	SoundManager* self = static_cast<SoundManager*>(userdata);
	std::lock_guard<std::mutex> lock(self->pointerMutex);
	
	
	if ((self->sb_index_read % (SM_BUFFER_SIZE * 50)) == 0)
	{
		std::cerr << "R:" << self->sb_index_read
		<< " W-R: " << (int)(self->sb_index_write+SM_RINGBUFFER_SIZE - self->sb_index_read) % SM_RINGBUFFER_SIZE
		<< " EPS:" << (int)self->eventsPerSample << std::endl;
	}
	
	/*
	 // if the read pointer is too close to the write pointer, send some silence
	 if (self->sb_index_read < self->sb_index_write)
	 {
	 if (self->sb_index_read + len > self->sb_index_write)
	 {
	 SDL_memset(stream, 0, len);
	 return;
	 }
	 }
	 // Same thing, but if the write pointer has wrapped around
	 if (self->sb_index_read > self->sb_index_write)
	 {
	 auto test_sb_read = (self->sb_index_read + len) % SM_RINGBUFFER_SIZE;
	 if ((test_sb_read < self->sb_index_read) && (test_sb_read > self->sb_index_write))
	 {
	 SDL_memset(stream, 0, len);
	 return;
	 }
	 }
	 */
	
	// copy the part of the ringbuffer that matters and advance the read pointer
	if ((self->sb_index_read + len) < SM_RINGBUFFER_SIZE)
	{
		SDL_memcpy(stream, self->soundRingbuffer+self->sb_index_read, len);
		self->sb_index_read += len;
	} else {
		auto count_over = len - (SM_RINGBUFFER_SIZE - self->sb_index_read);
		SDL_memcpy(stream, self->soundRingbuffer+self->sb_index_read, len - count_over);
		SDL_memcpy(stream, self->soundRingbuffer, count_over);
		self->sb_index_read = count_over;
	}
}

void SoundManager::BeginPlay() {
	sampleEventCount = 0;
	speakerPhaseEventCount = 0;
	sampleAverage = 0;
	eventsPerSample = SM_DEFAULT_EVENTS_PER_SAMPLE;
	isSoundOn = false;
	SDL_memset(soundRingbuffer, 0, SM_RINGBUFFER_SIZE);
	sb_index_read = 0;
	sb_index_write = sb_index_read + SM_STARTING_DELTA_READWRITE;
	SDL_PauseAudioDevice(audioDevice, 0); // Start audio playback
	isPlaying = true;
}

void SoundManager::StopPlay() {
	SDL_PauseAudioDevice(audioDevice, 1); // Stop audio playback immediately
	isPlaying = false;
}

bool SoundManager::IsPlaying() {
	return isPlaying;
}

inline void SoundManager::ToggleSoundState() {
	// std::lock_guard<std::mutex> lock(bufferMutex);
	isSoundOn = !isSoundOn;
}

void SoundManager::EventReceived(bool isC03x) {
	if (!isPlaying)
		BeginPlay();
	if (isC03x)
		ToggleSoundState();
	float sampleTotal = sampleAverage * sampleEventCount;
	if (isSoundOn)
	{
		if (speakerPhaseEventCount < (SM_EVENTS_PER_SPEAKER_PHASE/2))
			sampleAverage = (sampleTotal + 128) / (sampleEventCount + 1);	// High of the square wave
		else
			sampleAverage = (sampleTotal - 127) / (sampleEventCount + 1);	// Low of the square wave
	} else {
		// add 0 to the array
		sampleAverage = (sampleTotal + 0) / (sampleEventCount + 1);
	}
	speakerPhaseEventCount = (speakerPhaseEventCount + 1) % SM_EVENTS_PER_SPEAKER_PHASE;
	sampleEventCount++;
	
	if (sampleEventCount == eventsPerSample)
	{
		soundRingbuffer[sb_index_write] = round(sampleAverage);
		// std::cerr << "Sample average: " << round(sampleAverage) << std::endl;
		sampleAverage = 0;
		sampleEventCount = 0;
		sb_index_write = (sb_index_write + 1) % SM_RINGBUFFER_SIZE;
		
		// Modify eventsPerSample based on how close the reads are from the writes
		std::lock_guard<std::mutex> lock(pointerMutex);
		float _delta = (sb_index_write+SM_RINGBUFFER_SIZE - sb_index_read) % SM_RINGBUFFER_SIZE;
		
		eventsPerSample = SM_DEFAULT_EVENTS_PER_SAMPLE * (_delta / SM_STARTING_DELTA_READWRITE);
	}
}
