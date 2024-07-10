#include "SoundManager.h"
#include "imgui.h"
#include <iostream>

// below because "The declaration of a static data member in its class definition is not a definition"
SoundManager* SoundManager::s_instance;

// const int TONE_FREQUENCY = 1023; // Apple //e used a ~1 kHz tone

SoundManager::SoundManager(uint32_t sampleRate, uint32_t bufferSize)
: sampleRate(sampleRate), bufferSize(bufferSize), bIsPlaying(false) {
	if (SDL_Init(SDL_INIT_AUDIO) < 0) {
		std::cerr << "Failed to initialize SDL: " << SDL_GetError() << std::endl;
		throw std::runtime_error("SDL_Init failed");
	}
	audioDevice = 0;
	Initialize();
}

void SoundManager::Initialize()
{
	if (audioDevice != 0)
	{
		StopPlay();
		SDL_CloseAudioDevice(audioDevice);
		audioDevice = 0;
	}
	SDL_zero(audioSpec);
	audioSpec.freq = sampleRate;
	audioSpec.format = AUDIO_S8;
	audioSpec.channels = 1;
	audioSpec.samples = bufferSize;
	audioSpec.callback = SoundManager::AudioCallback;
	audioSpec.userdata = this;
	
	audioDevice = SDL_OpenAudioDevice(NULL, 0, &audioSpec, NULL, 0);
	
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
	if (!bIsEnabled)
		return;
	sampleEventCount = 0;
	speakerPhaseEventCount = 0;
	sampleAverage = 0;
	eventsPerSample = SM_DEFAULT_EVENTS_PER_SAMPLE;
	bIsSoundOn = false;		// Always start assuming no sound from the speaker
	SDL_memset(soundRingbuffer, 0, SM_RINGBUFFER_SIZE);
	sb_index_read = 0;
	sb_index_write = sb_index_read + SM_STARTING_DELTA_READWRITE;
	SDL_PauseAudioDevice(audioDevice, 0); // Start audio playback
	bIsPlaying = true;
}

void SoundManager::StopPlay() {
	SDL_PauseAudioDevice(audioDevice, 1); // Stop audio playback immediately
	bIsPlaying = false;
}

bool SoundManager::IsPlaying() {
	return bIsPlaying;
}

inline void SoundManager::ToggleSoundState() {
	// std::lock_guard<std::mutex> lock(bufferMutex);
	bIsSoundOn = !bIsSoundOn;
	if (bIsSoundOn)
		speakerPhaseEventCount = 0;		// reset the speaker phase when we turn on
}

void SoundManager::EventReceived(bool isC03x) {
	if (!bIsPlaying)
		BeginPlay();
	if (isC03x)
		ToggleSoundState();
	float sampleTotal = sampleAverage * sampleEventCount;
	if (bIsSoundOn)
	{
		//if (speakerPhaseEventCount < (SM_EVENTS_PER_SPEAKER_PHASE/2))
			sampleAverage = (sampleTotal + 128) / (sampleEventCount + 1);	// High of the square wave
		//else
		//	sampleAverage = (sampleTotal - 127) / (sampleEventCount + 1);	// Low of the square wave
		speakerPhaseEventCount = (speakerPhaseEventCount + 1) % SM_EVENTS_PER_SPEAKER_PHASE;
	} else {
		// add 0 to the array
		sampleAverage = (sampleTotal + 0) / (sampleEventCount + 1);
	}
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

///
///
/// ImGUI Interface
///
///

void SoundManager::DisplayImGuiChunk()
{
	if (ImGui::CollapsingHeader("[ SOUND ]"))
	{
		if (ImGui::Checkbox("Enable HDMI Sound", &bIsEnabled))
		{
			if (bIsEnabled)
				BeginPlay();
			else
				StopPlay();
		}
		ImGui::SameLine();
		ImGui::TextDisabled("(!)");
		if (ImGui::BeginItemTooltip())
		{
			ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
			ImGui::TextUnformatted(
								   "WARNING:\n"
								   "Do not enable HDMI sound when the Apple 2 is emitting sounds.\n"
								   "There is a chance that the sound will be reversed, i.e:\n"
								   "Sound will be on when it should be off, and vice versa.\n"
								   "So make sure you check this box only when you're certain the Apple 2 is silent.\n");
			ImGui::PopTextWrapPos();
			ImGui::EndTooltip();
		}
		if (ImGui::SliderInt("Buffer Size", &bufferSize, 128, 2048))
		{
			bufferSize -= (bufferSize % 128);
			if (audioSpec.samples != bufferSize)
				Initialize();
			if (bIsEnabled)
				BeginPlay();
		}
		ImGui::SetItemTooltip("A bigger buffer can add latency but is more CPU efficient. Ideally a power of 2.");
	}
}

nlohmann::json SoundManager::SerializeState()
{
	nlohmann::json jsonState = {
		{"sound_enabled", bIsEnabled},
		{"sound_buffer_size", bufferSize},
	};
	return jsonState;
}

void SoundManager::DeserializeState(const nlohmann::json &jsonState)
{
	bIsEnabled = jsonState.value("sound_enabled", bIsEnabled);
	bufferSize = jsonState.value("sound_buffer_size", bufferSize);
	if (audioSpec.samples != bufferSize)
		Initialize();
}
