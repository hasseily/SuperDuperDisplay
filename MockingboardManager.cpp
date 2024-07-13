#include "MockingboardManager.h"
#include <iostream>
#include <vector>
#include "imgui.h"

// below because "The declaration of a static data member in its class definition is not a definition"
MockingboardManager* MockingboardManager::s_instance;

MockingboardManager::MockingboardManager(uint32_t sampleRate, uint32_t bufferSize)
: sampleRate(sampleRate), bufferSize(bufferSize),
	ay{ Ayumi(false, 1750000, sampleRate),
		Ayumi(false, 1750000, sampleRate),
		Ayumi(false, 1750000, sampleRate),
		Ayumi(false, 1750000, sampleRate) } {
	if (SDL_Init(SDL_INIT_AUDIO) < 0) {
		std::cerr << "Failed to initialize SDL: " << SDL_GetError() << std::endl;
		throw std::runtime_error("SDL_Init failed");
	}
	audioDevice = 0;
	Initialize();
}

void MockingboardManager::Initialize()
{
	if (audioDevice == 0)
	{
		SDL_zero(audioSpec);
		audioSpec.freq = sampleRate;
		audioSpec.format = AUDIO_F32SYS;
		audioSpec.channels = 2;
		audioSpec.samples = bufferSize;
		audioSpec.callback = MockingboardManager::AudioCallback;
		audioSpec.userdata = this;
		
		audioDevice = SDL_OpenAudioDevice(NULL, 0, &audioSpec, NULL, 0);
	}
	
	if (audioDevice == 0) {
		std::cerr << "Failed to open audio device: " << SDL_GetError() << std::endl;
		SDL_Quit();
		throw std::runtime_error("SDL_OpenAudioDevice failed");
	}
	
	// Reset registers and set panning
	for(uint8_t ayidx = 0; ayidx < 4; ayidx++)
	{
		ay[ayidx].ResetRegisters();
	}
	// The default panning is one AY for each speaker
	UpdateAllPans();
}

MockingboardManager::~MockingboardManager() {
	SDL_CloseAudioDevice(audioDevice);
	SDL_Quit();
}

void MockingboardManager::BeginPlay() {
	if (!bIsEnabled)
		return;
	SDL_PauseAudioDevice(audioDevice, 0); // Start audio playback
	bIsPlaying = true;
	mb_event_count = 0;
}

void MockingboardManager::StopPlay() {
	SDL_PauseAudioDevice(audioDevice, 1); // Stop audio playback immediately
	bIsPlaying = false;
}

bool MockingboardManager::IsPlaying() {
	return bIsPlaying;
}

void MockingboardManager::AudioCallback(void* userdata, uint8_t* stream, int len) {
	MockingboardManager* self = static_cast<MockingboardManager*>(userdata);
	int samples = len / (sizeof(float) * 2); // Number of samples to fill
	std::vector<float> buffer(samples * 2);
	
	for (int i = 0; i < samples; ++i) {
		uint8_t ay_ct = (self->bIsDual ? 4 : 2);
		for(uint8_t ayidx = 0; ayidx < ay_ct; ayidx++)
		{
			self->ay[ayidx].Process();
		}
		if (self->bIsDual)
		{
			buffer[2 * i] = static_cast<float>(self->ay[0].left + self->ay[1].left + self->ay[2].left + self->ay[3].left);
			buffer[2 * i + 1] = static_cast<float>(self->ay[0].right + self->ay[1].right + self->ay[2].right + self->ay[3].right);
		} else {
			buffer[2 * i] = static_cast<float>(self->ay[0].left + self->ay[1].left);
			buffer[2 * i + 1] = static_cast<float>(self->ay[0].right + self->ay[1].right);
		}
	}
	
	// Copy the buffer to the stream
	SDL_memcpy(stream, buffer.data(), len);
}

void MockingboardManager::EventReceived(uint16_t addr, uint8_t val)
{
	if (!bIsEnabled)
		return;
	// Only parse 0xC4xx or 0xC5xx events (slots 4 and 5)
	if (((addr >> 8) != 0xC4) && ((addr >> 8) != 0xC5))
		return;
	
	if (!bIsPlaying)
		BeginPlay();

	++mb_event_count;

	// get which ay chip to use
	Ayumi *ayp;
	if ((addr >> 8) == 0xC4)
		ayp = ((addr & 0x80) == 0 ? &ay[0] : &ay[1]);
	else
		ayp = ((addr & 0x80) == 0 ? &ay[2] : &ay[3]);
	uint8_t low_nibble = addr & 0xF;

	switch (low_nibble) {
		case A2MBE_ORA:
			ayp->value_ora = val;	// data channel now has val
			break;
		case A2MBE_ORB:
			ayp->value_orb = val;
			switch (val) {
				case A2MBC_RESET:
					// Reset all registers to 0
					ayp->ResetRegisters();
					break;
				case A2MBC_INACTIVE:
					// In some Mockingboards, it's the setting to inactive that triggers the
					// latching or writing. In others, it's not the case. Let's just keep it simple
					// and not use the inactive code.
					break;
				case A2MBC_LATCH:
					ayp->latched_register = ayp->value_ora;
					// std::cerr << "Latching register: " << (int)ayp->value_ora << std::endl;
					break;
				case A2MBC_WRITE:
					SetLatchedRegister(ayp, ayp->value_ora);
					// std::cerr << "Setting Register value: " << (int)ayp->value_ora << std::endl;
					break;
				default:
					break;
			}
			break;
		case A2MBE_ODDRA:
			ayp->value_oddra = val;
			break;
		case A2MBE_ODDRB:
			ayp->value_oddrb = val;
			break;
		default:
			break;
	}
}

void MockingboardManager::SetPan(uint8_t ay_idx, uint8_t channel_idx, double pan, bool isEqp)
{
	if (ay_idx > 3)
		return;
	ay[ay_idx].SetPan(channel_idx, pan, isEqp);
	allpans[ay_idx][channel_idx] = (float)pan;
}

void MockingboardManager::UpdateAllPans()
{
	for(uint8_t ayidx = 0; ayidx < 4; ayidx++)
	{
		ay[ayidx].SetPan(0, (double)allpans[ayidx][0], 0);
		ay[ayidx].SetPan(1, (double)allpans[ayidx][1], 0);
		ay[ayidx].SetPan(2, (double)allpans[ayidx][2], 0);
	}
}

void MockingboardManager::SetLatchedRegister(Ayumi* ayp, uint8_t value)
{
	switch (ayp->latched_register) {
		case A2MBAYR_ATONEFINE:
			ayp->SetTone(0, (ayp->channels[0].tone_period & 0xFF00) + value);
			break;
		case A2MBAYR_ATONECOARSE:
			ayp->SetTone(0, (ayp->channels[0].tone_period & 0xFF) + (value << 8));
			break;
		case A2MBAYR_BTONEFINE:
			ayp->SetTone(1, (ayp->channels[1].tone_period & 0xFF00) + value);
			break;
		case A2MBAYR_BTONECOARSE:
			ayp->SetTone(1, (ayp->channels[1].tone_period & 0xFF) + (value << 8));
			break;
		case A2MBAYR_CTONEFINE:
			ayp->SetTone(2, (ayp->channels[2].tone_period & 0xFF00) + value);
			break;
		case A2MBAYR_CTONECOARSE:
			ayp->SetTone(2, (ayp->channels[2].tone_period & 0xFF) + (value << 8));
			break;
		case A2MBAYR_NOISEPER:
			ayp->SetNoise(value);
			break;
		case A2MBAYR_ENABLE:
			ayp->channels[0].t_off = (value & 0b1);
			ayp->channels[1].t_off = ((value & 0b10) >> 1);
			ayp->channels[2].t_off = ((value & 0b100) >> 2);
			ayp->channels[0].n_off = ((value & 0b1000) >> 3);
			ayp->channels[1].n_off = ((value & 0b10000) >> 4);
			ayp->channels[2].n_off = ((value & 0b100000) >> 5);
			break;
		case A2MBAYR_AVOL:
			// if value == 16, then it is variable and uses the envelope
			ayp->channels[0].e_on = (value == 16 ? 1 : 0);
			if (value != 16)
				ayp->SetVolume(0, value);
			break;
		case A2MBAYR_BVOL:
			// if value == 16, then it is variable and uses the envelope
			ayp->channels[1].e_on = (value == 16 ? 1 : 0);
			if (value != 16)
				ayp->SetVolume(1, value);
			break;
		case A2MBAYR_CVOL:
			// if value == 16, then it is variable and uses the envelope
			ayp->channels[2].e_on = (value == 16 ? 1 : 0);
			if (value != 16)
				ayp->SetVolume(2, value);
			break;
		case A2MBAYR_EFINE:
			ayp->SetEnvelope((ayp->envelope & 0xFF00) + value);
			break;
		case A2MBAYR_ECOARSE:
			ayp->SetEnvelope((ayp->envelope & 0xFF) + (value << 8));
			break;
		case A2MBAYR_ESHAPE:
			ayp->SetEnvelopeShape(value);
			break;
		default:
			break;
	}
}

// UTILITY METHODS
// These methods are unnecessary for regular operation on SDD where only events are received

void MockingboardManager::Util_Reset(uint8_t ay_idx)
{
	if (ay_idx > 3)
		return;
	uint16_t slot = (ay_idx < 2 ? 0xC400 : 0xC500);
	uint16_t offset = slot + (ay_idx * 0x80);
	
	EventReceived(offset+A2MBE_ORB, A2MBC_RESET);
	EventReceived(offset+A2MBE_ORB, A2MBC_INACTIVE);
}

void MockingboardManager::Util_WriteToRegister(uint8_t ay_idx, uint8_t reg_idx, uint8_t val)
{
	if (ay_idx > 3)
		return;
	uint16_t slot = (ay_idx < 2 ? 0xC400 : 0xC500);
	uint16_t offset = slot + ((ay_idx % 2) * 0x80);
	
	// Latch the register
	EventReceived(offset+A2MBE_ORA, reg_idx);
	EventReceived(offset+A2MBE_ORB, A2MBC_LATCH);
	EventReceived(offset+A2MBE_ORB, A2MBC_INACTIVE);
	
	// Write
	EventReceived(offset+A2MBE_ORA, val);
	EventReceived(offset+A2MBE_ORB, A2MBC_WRITE);
	EventReceived(offset+A2MBE_ORB, A2MBC_INACTIVE);
}

void MockingboardManager::Util_WriteAllRegisters(uint8_t ay_idx, uint8_t* val_array)
{
	for (uint8_t i=0; i<16; i++) {
		Util_WriteToRegister(ay_idx, i, val_array[i]);
	}
}

///
///
/// ImGUI Interface
///
///

void MockingboardManager::DisplayImGuiChunk()
{
	if (ImGui::CollapsingHeader("[ MOCKINGBOARD ]"))
	{
		ImGui::Checkbox("Enable Mockingboard (Slot 4)", &bIsEnabled);
		if (ImGui::Checkbox("Dual Mockingboards (Slots 4 and 5)", &bIsDual))
			bIsEnabled = true;
		
		if (bIsEnabled)
			ImGui::Text("Mockingboard Events: %d", mb_event_count);

		ImGui::SeparatorText("[ CHANNEL PANNING ]");
		if (ImGui::SliderFloat("AY Chip 0 Channel 0", &allpans[0][0], 0, 1, "%.3f", 1))
			SetPan(0, 0, allpans[0][0], false);
		if (ImGui::SliderFloat("AY Chip 0 Channel 1", &allpans[0][1], 0, 1, "%.3f", 1))
			SetPan(0, 1, allpans[0][1], false);
		if (ImGui::SliderFloat("AY Chip 0 Channel 2", &allpans[0][2], 0, 1, "%.3f", 1))
			SetPan(0, 2, allpans[0][2], false);
		if (ImGui::SliderFloat("AY Chip 1 Channel 0", &allpans[1][0], 0, 1, "%.3f", 1))
			SetPan(1, 0, allpans[1][0], false);
		if (ImGui::SliderFloat("AY Chip 1 Channel 1", &allpans[1][1], 0, 1, "%.3f", 1))
			SetPan(1, 1, allpans[1][1], false);
		if (ImGui::SliderFloat("AY Chip 1 Channel 2", &allpans[1][2], 0, 1, "%.3f", 1))
			SetPan(1, 2, allpans[1][2], false);
		if (bIsDual)
		{
			if (ImGui::SliderFloat("AY Chip 2 Channel 0", &allpans[2][0], 0, 1, "%.3f", 1))
				SetPan(2, 0, allpans[2][0], false);
			if (ImGui::SliderFloat("AY Chip 2 Channel 1", &allpans[2][1], 0, 1, "%.3f", 1))
				SetPan(2, 1, allpans[2][1], false);
			if (ImGui::SliderFloat("AY Chip 2 Channel 2", &allpans[2][2], 0, 1, "%.3f", 1))
				SetPan(2, 2, allpans[2][2], false);
			if (ImGui::SliderFloat("AY Chip 3 Channel 0", &allpans[3][0], 0, 1, "%.3f", 1))
				SetPan(3, 0, allpans[3][0], false);
			if (ImGui::SliderFloat("AY Chip 3 Channel 1", &allpans[3][1], 0, 1, "%.3f", 1))
				SetPan(3, 1, allpans[3][1], false);
			if (ImGui::SliderFloat("AY Chip 3 Channel 2", &allpans[3][2], 0, 1, "%.3f", 1))
				SetPan(3, 2, allpans[3][2], false);
		}
		if (ImGui::Button("Reset Pan to Default"))
		{
			allpans[0][0] = 0.3f;
			allpans[0][1] = 0.3f;
			allpans[0][2] = 0.3f;
			allpans[1][0] = 0.7f;
			allpans[1][1] = 0.7f;
			allpans[1][2] = 0.7f;
			allpans[2][0] = 0.2f;
			allpans[2][1] = 0.2f;
			allpans[2][2] = 0.2f;
			allpans[3][0] = 0.8f;
			allpans[3][1] = 0.8f;
			allpans[3][2] = 0.8f;
			UpdateAllPans();
		}
	}
}

nlohmann::json MockingboardManager::SerializeState()
{
	nlohmann::json jsonState = {
		{"mockingboard_enabled", bIsEnabled},
		{"mockingboard_dual", bIsDual},
		{"pan_ay_0_0", allpans[0][0]},
		{"pan_ay_0_1", allpans[0][1]},
		{"pan_ay_0_2", allpans[0][2]},
		{"pan_ay_1_0", allpans[1][0]},
		{"pan_ay_1_1", allpans[1][1]},
		{"pan_ay_1_2", allpans[1][2]},
		{"pan_ay_2_0", allpans[2][0]},
		{"pan_ay_2_1", allpans[2][1]},
		{"pan_ay_2_2", allpans[2][2]},
		{"pan_ay_3_0", allpans[3][0]},
		{"pan_ay_3_1", allpans[3][1]},
		{"pan_ay_3_2", allpans[3][2]},
	};
	return jsonState;
}

void MockingboardManager::DeserializeState(const nlohmann::json &jsonState)
{
	bIsEnabled = jsonState.value("mockingboard_enabled", bIsEnabled);
	bIsDual = jsonState.value("mockingboard_dual", bIsDual);
	allpans[0][0] = jsonState.value("pan_ay_0_0", allpans[0][0]);
	allpans[0][1] = jsonState.value("pan_ay_0_1", allpans[0][1]);
	allpans[0][2] = jsonState.value("pan_ay_0_2", allpans[0][2]);
	allpans[1][0] = jsonState.value("pan_ay_1_0", allpans[1][0]);
	allpans[1][1] = jsonState.value("pan_ay_1_1", allpans[1][1]);
	allpans[1][2] = jsonState.value("pan_ay_1_2", allpans[1][2]);
	allpans[2][0] = jsonState.value("pan_ay_2_0", allpans[2][0]);
	allpans[2][1] = jsonState.value("pan_ay_2_1", allpans[2][1]);
	allpans[2][2] = jsonState.value("pan_ay_2_2", allpans[2][2]);
	allpans[3][0] = jsonState.value("pan_ay_3_0", allpans[3][0]);
	allpans[3][1] = jsonState.value("pan_ay_3_1", allpans[3][1]);
	allpans[3][2] = jsonState.value("pan_ay_3_2", allpans[3][2]);
	UpdateAllPans();
}
