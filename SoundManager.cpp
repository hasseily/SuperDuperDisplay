#include "SoundManager.h"
#include "imgui.h"
#include <iostream>
#include "common.h"

// below because "The declaration of a static data member in its class definition is not a definition"
SoundManager* SoundManager::s_instance;

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
	if (audioDevice == 0)
	{
		SDL_zero(audioSpec);
		audioSpec.freq = sampleRate;
		audioSpec.format = AUDIO_F32SYS;
		audioSpec.channels = 1;
		audioSpec.samples = bufferSize;
		audioSpec.callback = nullptr;
		audioSpec.userdata = this;

		audioDevice = SDL_OpenAudioDevice(NULL, 0, &audioSpec, NULL, 0);
	}
	else {
		// std::cerr << "Stopping and clearing Speaker Audio" << std::endl;
		SDL_PauseAudioDevice(audioDevice, 1);
		SDL_ClearQueuedAudio(audioDevice);
	}
	
	if (audioDevice == 0) {
		std::cerr << "Failed to open audio device: " << SDL_GetError() << std::endl;
		SDL_Quit();
		throw std::runtime_error("SDL_OpenAudioDevice failed");
	}

	bIsPlaying = false;
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
	ticks_per_sample = ((uint64_t)SM_TICKS_PER_CYCLE * (bIsPAL ? _A2_CPU_FREQUENCY_PAL : _A2_CPU_FREQUENCY_NTSC)) / sampleRate;
	// ticks_per_sample += 10000;	// Create just slightly less samples than the sample rate
	if (_isPlaying)
		BeginPlay();
}

void SoundManager::BeginPlay() {
	if (!bIsEnabled)
		return;
	curr_tick = 0;
	beeper_state = 0;	// We can't know the state of the beeper, assume it's LOW
	prev_beeper_state = beeper_state;
	curr_freq = 0.f;
	beeper_samples_idx = 0;
	beeper_samples_same_ct = 0;
	dcadj_pos = 0;
	dcadj_sum = 0;
	memset(dcadj_buf, 0, sizeof(dcadj_buf));
	SDL_PauseAudioDevice(audioDevice, 0); // Start audio playback
	bIsPlaying = true;
}

void SoundManager::StopPlay() {
	// flush the last sounds
	bool _enabledState = bIsEnabled;
	bIsEnabled = false;	 // disable event handling until everything is flushed
	SDL_PauseAudioDevice(audioDevice, 1);
	SDL_ClearQueuedAudio(audioDevice);
	bIsPlaying = false;
	bIsEnabled = _enabledState;
}

bool SoundManager::IsPlaying() {
	return bIsPlaying;
}

float SoundManager::DCAdjustment(float freq)
{
	dcadj_sum -= dcadj_buf[dcadj_pos];
	dcadj_sum += freq;
	dcadj_buf[dcadj_pos] = freq;
	dcadj_pos = (dcadj_pos + 1) & (SM_BEEPER_DCADJ_BUFLEN - 1);
	return (dcadj_sum / SM_BEEPER_DCADJ_BUFLEN);
}

void SoundManager::EventReceived(bool isC03x) {
	if (!bIsEnabled)
		return;
	if (!bIsPlaying)
		BeginPlay();
	if (isC03x)
		beeper_state = !beeper_state;
	curr_tick += SM_TICKS_PER_CYCLE;	// move the tick ptr
	if (curr_tick < ticks_per_sample)
	{
		// the 44.1kHz sample isn't full yet, just add the frequency for this cycle
		curr_freq += ((2.f * beeper_state) - 1.f) *
			((float)(SM_TICKS_PER_CYCLE) / ticks_per_sample);
		beeper_state == prev_beeper_state ? ++beeper_samples_same_ct : beeper_samples_same_ct = 0;
		prev_beeper_state = beeper_state;
	}
	else {
		// We passed the sample threshold. Add the frequency for the fraction of
		// the cycle in this sample, then add to the sample buffer. If the buffer is
		// full, send it to SDL's sound system.
		// Keep the rest of the cycle as the frequency
		uint32_t ticks_used = ticks_per_sample - (curr_tick - SM_TICKS_PER_CYCLE);
		curr_freq += (2.f * beeper_state - 1.f) *
			((float)(ticks_used) / ticks_per_sample);
		// add to the sound buffer, dampening by the volume
		// TODO: Use a DC adjustment filter
		// TODO: Always reduce the range to 70% to avoid pops and cracks
		beeper_samples[beeper_samples_idx] = DCAdjustment(curr_freq * beeper_volume * SM_BASE_VOLUME_ADJUSTMENT);
		if (isC03x)
			std::cerr << curr_freq << " " << beeper_volume << std::endl;
		++beeper_samples_idx;
		// Now keep the remainder of the cycle for the next sample
		curr_tick = SM_TICKS_PER_CYCLE - ticks_used;
		curr_freq = (2.f * beeper_state - 1.f) *
			((float)(curr_tick) / ticks_per_sample);
		// Check if we've got enough samples to send to SDL audio
		if (beeper_samples_idx == SM_BEEPER_BUFLEN)
		{
			// Drift removal code
			// Here we check if we've got too long of an audio queue, which means
			// we're going to soon have a noticeable lag. We check if the queue is full of the same frequency
			// and we clear it completely. When the frequency is the same no sound is output.
			// As long as SM_BUFFER_DRIFT_LIMIT >= 4096, which gets to 11Hz or lower,
			// it is certain that we are not "within a sound" (because no human can hear < 12Hz). So this
			// is really a silent time, which we can use to remove the drift.
			auto sdl_queue_size = SDL_GetQueuedAudioSize(audioDevice) / sizeof(float);
			if ((sdl_queue_size >= SM_BUFFER_DRIFT_LIMIT) && (sdl_queue_size < beeper_samples_same_ct/23))
			{
				// SDL_ClearQueuedAudio(audioDevice);
				beeper_samples_same_ct -= SM_BEEPER_BUFLEN;
			}
			else {
				SDL_QueueAudio(audioDevice, (const void*)(beeper_samples), SM_BEEPER_BUFLEN * sizeof(float));
			}
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
	ImGui::SliderFloat("Volume", &beeper_volume, 0.f, 1.f);
	ImGui::Separator();
	static int sm_imgui_queued_audio_size = 0;
	if ((SDL_GetTicks64() & 0xFF) == 0)
		sm_imgui_queued_audio_size = (int)(SDL_GetQueuedAudioSize(audioDevice) / sizeof(float));
	ImGui::Text("Queued Samples: %d", sm_imgui_queued_audio_size);
}

nlohmann::json SoundManager::SerializeState()
{
	nlohmann::json jsonState = {
		{"sound_enabled", bIsEnabled},
		{"sound_volume", beeper_volume}
	};
	return jsonState;
}

void SoundManager::DeserializeState(const nlohmann::json &jsonState)
{
	bIsEnabled = jsonState.value("sound_enabled", bIsEnabled);
	beeper_volume = jsonState.value("sound_volume", beeper_volume);
}
