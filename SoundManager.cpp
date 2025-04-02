#include "SoundManager.h"
#include "imgui.h"
#include <iostream>
#include "MockingboardManager.h"
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
	if (audioDevice == 0)
	{
		SDL_zero(audioSpec);
		audioSpec.freq = sampleRate;
		audioSpec.format = AUDIO_F32SYS;
		audioSpec.channels = 2;
		audioSpec.samples = bufferSize;
		audioSpec.callback = SoundManager::AudioCallback;
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
	SDL_PauseAudioDevice(audioDevice, 0);
}

SoundManager::~SoundManager() {
	SDL_PauseAudioDevice(audioDevice, 1);
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
	beeper_desc_t bdesc = { bIsPAL ? (float)_A2_CPU_FREQUENCY_PAL : (float)_A2_CPU_FREQUENCY_NTSC, _AUDIO_SAMPLE_RATE, 
		SM_BASE_VOLUME_ADJUSTMENT };
	beeper_init(&beeper, &bdesc);
	if (_isPlaying)
		BeginPlay();
}

void SoundManager::BeginPlay() {
	if (!bIsEnabled)
		return;
	beeper_reset(&beeper);
	curr_tick = 0;
	curr_freq = 0.f;
	beeper_samples_idx_read = SM_BEEPER_BUFFER_SIZE - SM_AUDIO_BUFLEN;
	beeper_samples_idx_write = 0;
	dcadj_pos = 0;
	dcadj_sum = 0;
	memset(beeper_samples, 0, sizeof(beeper_samples));
	memset(dcadj_buf, 0, sizeof(dcadj_buf));
	bIsPlaying = true;
	MockingboardManager::GetInstance()->BeginPlay();
}

void SoundManager::StopPlay() {
	// flush the last sounds
	bool _enabledState = bIsEnabled;
	bIsEnabled = false;	 // disable event handling until everything is flushed
	MockingboardManager::GetInstance()->StopPlay();
	SDL_Delay(100);
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
		beeper_toggle(&beeper);
	if (beeper_tick(&beeper))
	{
		if (((beeper_samples_idx_write + 1) % SM_BEEPER_BUFFER_SIZE) == beeper_samples_idx_read)
		{
			// drop the sample, reading is lagging
		}
		else {
			beeper_samples_idx_write = (beeper_samples_idx_write + 1) % SM_BEEPER_BUFFER_SIZE;
			beeper_samples[beeper_samples_idx_write] = beeper.sample;
		}
	}
}

void SoundManager::AudioCallback(void* userdata, uint8_t* stream, int len)
{
	SoundManager* self = static_cast<SoundManager*>(userdata);

	if (self->master_volume < 0.01f)	// if master volume is zero, turn off the sound
	{
		SDL_memset(stream, 0, len);		// shouldn't be necessary, but better be safe
		return;
	}

	int samples = len / (sizeof(float) * 2); 	// Number of samples to fill

	// Need to mix the speaker and the mockingboard Audio
	auto mmMgr = MockingboardManager::GetInstance();
	float mm_left = 0.f, mm_right = 0.f;	// The left and right values from the Mockingboard mix

	float beeper_sample = 0.f;	// that's the beeper mono sample

	for (int i = 0; i < samples; ++i) {
		if (self->IsPlaying())
		{
			if (((self->beeper_samples_idx_read + 1) % SM_BEEPER_BUFFER_SIZE) == self->beeper_samples_idx_write)
			{
				// write is lagging, use the current sample
				beeper_sample = self->beeper_samples[self->beeper_samples_idx_read];
			}
			else {
				self->beeper_samples_idx_read = (self->beeper_samples_idx_read + 1) % SM_BEEPER_BUFFER_SIZE;
				beeper_sample = self->beeper_samples[self->beeper_samples_idx_read];
			}
		}
		if (mmMgr->IsPlaying())
			mmMgr->GetSamples(mm_left, mm_right);

		// Mix in the mono beeper and stereo Mockingboard streams
		auto _relBeeperVol = self->beeper_volume / (self->beeper_volume + self->mockingboard_volume);
		auto _leftmix = self->master_volume * (_relBeeperVol * beeper_sample + (1.f - _relBeeperVol) * mm_left);
		auto _rightmix = self->master_volume * (_relBeeperVol * beeper_sample + (1.f - _relBeeperVol) * mm_right);
		self->audioCallbackBuffer[2 * i] = _leftmix;
		self->audioCallbackBuffer[2 * i + 1] = _rightmix;
	}

	// Copy the buffer to the stream
	SDL_memcpy(stream, self->audioCallbackBuffer, len);
	SDL_memset(self->audioCallbackBuffer, 0, len);
}

///
///
/// ImGUI Interface
///
///

void SoundManager::DisplayImGuiChunk()
{
	if (ImGui::BeginMenu("Volume Mix")) {
		ImGui::SliderFloat("Master Volume", &master_volume, 0.f, 1.f);
		ImGui::SliderFloat("Beeper Volume", &beeper_volume, 0.f, 1.f);
		ImGui::SliderFloat("Mockingboard Volume", &mockingboard_volume, 0.f, 1.f);
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("HDMI Speaker")) {
		if (ImGui::Checkbox("Enable HDMI Beeper Sound", &bIsEnabled))
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
		ImGui::Separator();
		static int sm_imgui_samples_delay = (SM_BEEPER_BUFFER_SIZE + beeper_samples_idx_write - beeper_samples_idx_read) % SM_BEEPER_BUFFER_SIZE;
		if ((SDL_GetTicks64() & 0xC0) == 0)
			sm_imgui_samples_delay = (SM_BEEPER_BUFFER_SIZE + beeper_samples_idx_write - beeper_samples_idx_read) % SM_BEEPER_BUFFER_SIZE;
		ImGui::Text("Write-Read Samples Delay: %d", sm_imgui_samples_delay);
		ImGui::Text("Current Audio Driver: %s\n", SDL_GetCurrentAudioDriver());
		ImGui::EndMenu();
	}
}

nlohmann::json SoundManager::SerializeState()
{
	nlohmann::json jsonState = {
		{"sound_enabled", bIsEnabled},
		{"sound_volume", beeper_volume},
		{"mockingboard_volume", mockingboard_volume},
		{"master_volume", master_volume}
	};
	return jsonState;
}

void SoundManager::DeserializeState(const nlohmann::json &jsonState)
{
	bIsEnabled = jsonState.value("sound_enabled", bIsEnabled);
	beeper_volume = jsonState.value("sound_volume", beeper_volume);
	mockingboard_volume = jsonState.value("mockingboard_volume", mockingboard_volume);
	master_volume = jsonState.value("master_volume", master_volume);
}
