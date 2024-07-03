#include "SoundManager.h"
#include <iostream>

// below because "The declaration of a static data member in its class definition is not a definition"
SoundManager* SoundManager::s_instance;

const int TONE_FREQUENCY = 1023; // Apple //e used a ~1 kHz tone

SoundManager::SoundManager(int sampleRate, int bufferSize)
: sampleRate(sampleRate), bufferSize(bufferSize), isPlaying(false), phase(0) {
	if (SDL_Init(SDL_INIT_AUDIO) < 0) {
		std::cerr << "Failed to initialize SDL: " << SDL_GetError() << std::endl;
		throw std::runtime_error("SDL_Init failed");
	}
	audioDevice = 0;
	isDevicePaused = true;
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
	if (isDevicePaused)
		SoundOn();
}

SoundManager::~SoundManager() {
	SDL_CloseAudioDevice(audioDevice);
	SDL_Quit();
}

void SoundManager::AudioCallback(void* userdata, Uint8* stream, int len) {
	SoundManager* self = static_cast<SoundManager*>(userdata);
	std::lock_guard<std::mutex> lock(self->bufferMutex);
	
	if (self->isPlaying) {
		self->GenerateTone(len, stream);
	} else {
		self->GenerateSilence(len, stream);
	}
}

void SoundManager::GenerateTone(int len, Uint8* stream) {
	int tonePeriod = sampleRate / TONE_FREQUENCY;
	
	for (int i = 0; i < len; ++i) {
		stream[i] = ((phase / (tonePeriod / 2)) % 2) ? 127 : -128;
		phase = (phase + 1) % tonePeriod;
	}
}

void SoundManager::GenerateSilence(int len, Uint8* stream) {
	SDL_memset(stream, 0, len);
}

void SoundManager::BeginPlay() {
	std::lock_guard<std::mutex> lock(bufferMutex);
	isPlaying = true;
}

void SoundManager::StopPlay() {
	std::lock_guard<std::mutex> lock(bufferMutex);
	isPlaying = false;
}

void SoundManager::SoundOn() {
	if (isDevicePaused)
		SDL_PauseAudioDevice(audioDevice, 0);
	isDevicePaused = false;
}

void SoundManager::SoundOff() {
	if (!isDevicePaused)
		SDL_PauseAudioDevice(audioDevice, 1);
	isDevicePaused = true;
}
