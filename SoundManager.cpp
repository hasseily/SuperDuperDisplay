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
	audioSpec.format = AUDIO_F32;
	audioSpec.channels = 1;
	audioSpec.samples = bufferSize;
	audioSpec.callback = nullptr;
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

void SoundManager::BeginPlay() {
	if (!bIsEnabled)
		return;
	sampleCycleCount = 0;
	sampleAverage = 0;
	lastQueuedSampleCount = -1;	// not yet known
	bIsLastHigh = false;
	cyclesPerSample = SM_DEFAULT_CYCLES_PER_SAMPLE;
	SDL_memset(soundRingbuffer, 0, SM_RINGBUFFER_SIZE);
	sb_index_read = 0;
	sb_index_write = sb_index_read + SM_STARTING_DELTA_READWRITE;
	SDL_PauseAudioDevice(audioDevice, 0); // Start audio playback
	bIsPlaying = true;
}

void SoundManager::StopPlay() {
	SDL_PauseAudioDevice(audioDevice, 1); // Stop audio playback immediately
	SDL_ClearQueuedAudio(audioDevice);
	bIsPlaying = false;
}

bool SoundManager::IsPlaying() {
	return bIsPlaying;
}

void SoundManager::EventReceived(bool isC03x) {
	if (!bIsEnabled)
		return;
	if (!bIsPlaying)
		BeginPlay();
	if (isC03x)
		bIsLastHigh = !bIsLastHigh;

	if (bIsLastHigh)
		cyclesHigh+=SM_CYCLE_MULTIPLIER;

	sampleCycleCount+=SM_CYCLE_MULTIPLIER;

	if (sampleCycleCount >= cyclesPerSample)
	{
		soundRingbuffer[sb_index_write] = static_cast<float>(cyclesHigh)/static_cast<float>(cyclesPerSample);
		sb_index_write = (sb_index_write + 1) % SM_RINGBUFFER_SIZE;
		auto _delta = (sb_index_write+SM_RINGBUFFER_SIZE - sb_index_read) % SM_RINGBUFFER_SIZE;
		auto len = SM_BUFFER_SIZE;
		if (_delta >= len)
		{
			// Skip queuing if we're bigger than SM_BUFFER_SIZE*2
			// Cheap way to avoid getting too far behind and getting a sound delay
			auto _queuedSamples = SDL_GetQueuedAudioSize(audioDevice) / sizeof(float);
			// if (_queuedSamples >= (SM_BUFFER_SIZE*2))
			//	std::cerr << "Dropping sample!" << std::endl;
			if ((sb_index_read + len) < SM_RINGBUFFER_SIZE)
			{
				if (_queuedSamples < (SM_BUFFER_SIZE*2))
					SDL_QueueAudio(audioDevice, (const void*)(soundRingbuffer+sb_index_read), len*sizeof(float));
				sb_index_read += len;
			} else {
				auto count_over = len - (SM_RINGBUFFER_SIZE - sb_index_read);
				if (_queuedSamples < (SM_BUFFER_SIZE*2))
				{
					SDL_QueueAudio(audioDevice, (const void*)(soundRingbuffer+sb_index_read), (len - count_over)*sizeof(float));
					SDL_QueueAudio(audioDevice, (const void*)(soundRingbuffer), count_over*sizeof(float));
				}
				sb_index_read = count_over;
			}
		}
	
		// Keep the remainder cycles for next sample
		// whatever is left should be kept at the state of the speaker
		sampleCycleCount = sampleCycleCount - cyclesPerSample;
		cyclesHigh = sampleCycleCount * bIsLastHigh;
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
		ImGui::Text("Cycles per Sample: %.4f", (double)cyclesPerSample / SM_CYCLE_MULTIPLIER);
		ImGui::Text("Queued Samples: %d", (int)(SDL_GetQueuedAudioSize(audioDevice) / sizeof(float)));
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
		if (ImGui::SliderInt("Buffer Size", &bufferSize, 128, 4096))
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
