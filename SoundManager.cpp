#include "SoundManager.h"
#include "imgui.h"
#include <iostream>
#define CHIPS_IMPL
#include "beeper.h"

// below because "The declaration of a static data member in its class definition is not a definition"
SoundManager* SoundManager::s_instance;

beeper_t beeper;

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
	bIsPlaying = false;
	beeper_samples_idx = 0;
	beeper_samples_zero_ct = 0;
	SetPAL(bIsPAL);
}

SoundManager::~SoundManager() {
	SDL_CloseAudioDevice(audioDevice);
	SDL_Quit();
}

void SoundManager::SetPAL(bool isPal) {
	bIsPAL = isPal;
	if (!bIsEnabled)
		return;
	bool _isPlaying = bIsPlaying;
	if (_isPlaying)
		SDL_PauseAudioDevice(audioDevice, 1);
	beeper_desc_t bdesc = { bIsPAL ? 1'015'625 : 1'020'484, SM_SAMPLE_RATE, 1.0f };
	beeper_init(&beeper, &bdesc);
	if (_isPlaying)
		BeginPlay();
}

void SoundManager::BeginPlay() {
	if (!bIsEnabled)
		return;
	beeper_reset(&beeper);
	beeper_samples_idx = 0;
	beeper_samples_zero_ct = 0;
	SDL_PauseAudioDevice(audioDevice, 0); // Start audio playback
	bIsPlaying = true;
}

void SoundManager::StopPlay() {
	// flush the last sounds
	std::memset(beeper_samples + beeper_samples_idx, 0, SM_BEEPER_BUFFER_SIZE - beeper_samples_idx);
	bool is_queue_empty = false;
	while (!is_queue_empty) {
		if (SDL_GetQueuedAudioSize(audioDevice) == 0) {
			is_queue_empty = true;
		}
		SDL_Delay(5);
	}
	SDL_PauseAudioDevice(audioDevice, 1);
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
	{
		beeper_toggle(&beeper);
	}
	if (beeper_tick(&beeper))
	{
		beeper_samples[beeper_samples_idx] = beeper.sample;
		++beeper_samples_idx;
		beeper.sample == 0 ? ++beeper_samples_zero_ct : beeper_samples_zero_ct = 0;
		if (beeper_samples_idx == SM_BEEPER_BUFFER_SIZE)
		{
			// Drift removal code
			// Here we check if we've got more than the buffer size in the queue, which means
			// we're going to soon have a noticeable lag. We check if the queue is full of zeros
			// and we clear it completely. As long as SM_BUFFER_DRIFT_LIMIT_SIZE >= 4096, which is gets to about 11Hz,
			// it is certain that we are not "within a sound" (because no human can hear < 12Hz). So this
			// is really a silent time, which we can use to remove the drift.
			auto sdl_queue_size = SDL_GetQueuedAudioSize(audioDevice);
			if ((sdl_queue_size >= SM_BUFFER_DRIFT_LIMIT_SIZE) && (sdl_queue_size < beeper_samples_zero_ct))
			{
				SDL_ClearQueuedAudio(audioDevice);
				beeper_samples_zero_ct = 0;
			}
			
			// Just queue the SM_BEEPER_BUFFER_SIZE array
			SDL_QueueAudio(audioDevice, (const void*)(beeper_samples), SM_BEEPER_BUFFER_SIZE*sizeof(float));
			beeper_samples_idx = 0;
		}
	}
}

///
///
/// ImGUI Interface
///
///

void SoundManager::DisplayImGuiChunk()
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
	ImGui::SliderFloat("Volume", &beeper.volume, 0.f, 1.f);
	ImGui::Separator();
	ImGui::Text("Queued Samples: %d", (int)(SDL_GetQueuedAudioSize(audioDevice) / sizeof(float)));
}

nlohmann::json SoundManager::SerializeState()
{
	nlohmann::json jsonState = {
		{"sound_enabled", bIsEnabled},
		{"sound_volume", beeper.volume}
	};
	return jsonState;
}

void SoundManager::DeserializeState(const nlohmann::json &jsonState)
{
	bIsEnabled = jsonState.value("sound_enabled", bIsEnabled);
	beeper.volume = jsonState.value("sound_volume", beeper.volume);
}
