#include "MockingboardManager.h"
#include <iostream>
#include <vector>
#include "common.h"
#include "imgui.h"

// below because "The declaration of a static data member in its class definition is not a definition"
MockingboardManager* MockingboardManager::s_instance;

MockingboardManager::MockingboardManager(uint32_t sampleRate, uint32_t bufferSize)
: sampleRate(sampleRate), bufferSize(bufferSize),
	ay{ Ayumi(false, _A2_CPU_FREQUENCY_NTSC, sampleRate),
		Ayumi(false, _A2_CPU_FREQUENCY_NTSC, sampleRate),
		Ayumi(false, _A2_CPU_FREQUENCY_NTSC, sampleRate),
		Ayumi(false, _A2_CPU_FREQUENCY_NTSC, sampleRate) } {
	if (SDL_Init(SDL_INIT_AUDIO) < 0) {
		std::cerr << "Failed to initialize SDL: " << SDL_GetError() << std::endl;
		throw std::runtime_error("SDL_Init failed");
	}
	audioDevice = 0;
	Initialize();
}

void MockingboardManager::Initialize()
{
	// NOTE: SSI263 objects have their own audioDevice
	// This here is for mixing the AYs
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
	
	// Reset registers
	for(uint8_t ayidx = 0; ayidx < 4; ayidx++)
	{
		ay[ayidx].ResetRegisters();
	}
	for(uint8_t ssiidx = 0; ssiidx < 4; ssiidx++)
	{
		ssi[ssiidx].ResetRegisters();
	}
	
	bIsPlaying = false;
}

MockingboardManager::~MockingboardManager() {
	SDL_CloseAudioDevice(audioDevice);
	SDL_Quit();
}

void MockingboardManager::BeginPlay() {
	if (!bIsEnabled)
		return;
	// Reset registers and set panning
	for(uint8_t ayidx = 0; ayidx < 4; ayidx++)
	{
		ay[ayidx].ResetRegisters();
	}
	UpdateAllPans();
	SDL_PauseAudioDevice(audioDevice, 0); // Start audio playback
	bIsPlaying = true;
	mb_event_count = 0;
}

void MockingboardManager::StopPlay() {
	// set amplitude to 0
	for(uint8_t ayidx = 0; ayidx < 4; ayidx++)
	{
		ay[ayidx].SetVolume(0, 0);
		ay[ayidx].SetVolume(1, 0);
		ay[ayidx].SetVolume(2, 0);
	}
	SDL_PauseAudioDevice(audioDevice, 1);
	SDL_ClearQueuedAudio(audioDevice);
	bIsPlaying = false;
}

bool MockingboardManager::IsPlaying() {
	return bIsPlaying;
}

void MockingboardManager::AudioCallback(void* userdata, uint8_t* stream, int len) {
	MockingboardManager* self = static_cast<MockingboardManager*>(userdata);
	int samples = len / (sizeof(float) * 2); 	// Number of samples to fill
	std::vector<float> buffer(samples * 2);		// TODO: preallocate large buffer
	
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

void MockingboardManager::EventReceived(uint16_t addr, uint8_t val, bool rw)
{
	if (!bIsEnabled)
		return;

	// Reset CS0 for all SSI chips
	for (uint8_t ssiidx = 0; ssiidx < 4; ssiidx++)
	{
		ssi[ssiidx].SetCS0(0);
	}

	// Only parse 0xC4xx or 0xC5xx events (slots 4 and 5)
	if (((addr >> 8) != 0xC4) && ((addr >> 8) != 0xC5))
		return;
	
	if (!bIsPlaying)
		BeginPlay();

	++mb_event_count;

	// get which ay chip to use
	Ayumi *ayp;
	SSI263* ssip;
	if ((addr >> 8) == 0xC4)
	{
		ayp = ((addr & 0x80) == 0 ? &ay[0] : &ay[1]);		// 0x00, 0x80
		ssip = ((addr & 0x20) == 0 ? &ssi[0] : &ssi[1]);	// 0x40, 0x20
	}
	else {
		ayp = ((addr & 0x80) == 0 ? &ay[2] : &ay[3]);		// 0x00, 0x80
		ssip = ((addr & 0x20) == 0 ? &ssi[2] : &ssi[3]);	// 0x40, 0x20
	}

	uint8_t low_nibble = addr & 0x0F;

	switch (low_nibble) {
		case A2MBE_ORA:
			ayp->value_ora = val;	// data channel now has val
			break;
		case A2MBE_ORB:
			// Check !RESET pin
			if ((val & 0b100) == 0)
			{
				if (bNotResetPinState == 1)
				{
					// !RESET pulled low
					// Reset all registers to 0
					ayp->ResetRegisters();
					bNotResetPinState = 0;
				}
			}
			else {
				bNotResetPinState = 1;
			}
			switch (val & 0b11) {
			case A2MBC_INACTIVE:
				// In some Mockingboards, it's the setting to inactive that triggers the
				// latching or writing. In others, it's not the case. Let's just keep it simple
				// and not use the inactive code.
				break;
			case A2MBC_READ:
				// Never handled here. By the time we returned the data to the
				// Apple 2 it would be horribly late
				break;
			case A2MBC_WRITE:
				SetLatchedRegister(ayp, ayp->value_ora);
				// std::cerr << "Setting Register value: " << (int)ayp->value_ora << std::endl;
				break;
			case A2MBC_LATCH:
				// http://www.worldofspectrum.org/forums/showthread.php?t=23327
				// Selecting an unused register number above 0x0f puts the AY into a state where
				// any values written to the data/address bus are ignored, but can be read back
				// within a few tens of thousands of cycles before they decay to zero.
				if (ayp->value_ora <= 0xFF)
				{
					ayp->latched_register = ayp->value_ora;
					// std::cerr << "Latching register: " << (int)ayp->value_ora << std::endl;
				}
				break;
			default:
				break;
			}
			ayp->value_orb = (val & 0b11);
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

	// Update the valid SSI chip
	ssip->SetData(val);
	ssip->SetRegisterSelect(addr & 0b11);
	ssip->SetReadMode(rw);
	ssip->SetCS0(1);
	ssip->Update();
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
// Every time we send an event, make sure the !RESET bit (bit 2) is high

void MockingboardManager::Util_Reset(uint8_t ay_idx)
{
	if (ay_idx > 3)
		return;
	uint16_t slot = (ay_idx < 2 ? 0xC400 : 0xC500);
	uint16_t offset = slot + (ay_idx * 0x80);
	
	EventReceived(offset+A2MBE_ORB, 0, 0);	// Reset (bit 2 is 0)
	EventReceived(offset+A2MBE_ORB, 0b100 | A2MBC_INACTIVE, 0);
}

void MockingboardManager::Util_WriteToRegister(uint8_t ay_idx, uint8_t reg_idx, uint8_t val)
{
	if (ay_idx > 3)
		return;
	uint16_t slot = (ay_idx < 2 ? 0xC400 : 0xC500);
	uint16_t offset = slot + ((ay_idx % 2) * 0x80);
	
	// Latch the register
	EventReceived(offset+A2MBE_ORA, reg_idx, 0);
	EventReceived(offset+A2MBE_ORB, 0b100 | A2MBC_LATCH, 0);
	EventReceived(offset+A2MBE_ORB, 0b100 | A2MBC_INACTIVE, 0);
	
	// Write
	EventReceived(offset+A2MBE_ORA, val, 0);
	EventReceived(offset+A2MBE_ORB, 0b100 | A2MBC_WRITE, 0);
	EventReceived(offset+A2MBE_ORB, 0b100 | A2MBC_INACTIVE, 0);
}

void MockingboardManager::Util_WriteAllRegisters(uint8_t ay_idx, uint8_t* val_array)
{
	for (uint8_t i=0; i<16; i++) {
		Util_WriteToRegister(ay_idx, i, val_array[i]);
	}
}

void MockingboardManager::Util_SpeakDemoPhrase()
{
	// Data is formatted like the sample phrases in the MB disk1
	// Sets of 5 bytes, starting with the highest register (FF) and
	// ending with the Duration/Phoneme register
	const uint8_t phrase[220] = {
		0x50, 0xD0, 0x00, 0xCF, 0x00,	// PAUSE
		0xE8, 0x70, 0xA8, 0x5F, 0x00,	// PAUSE
		0xE8, 0x70, 0xA8, 0x5F, 0x00,	// PAUSE
		0xE8, 0x70, 0xA8, 0x5F, 0x00,	// PAUSE
		0xE8, 0x7B, 0xA8, 0x5F, 0x63,	// W
		0xE7, 0x6B, 0xA8, 0x59, 0x47,	// I
		0xE8, 0x6A, 0xA8, 0x61, 0x36,	// TH
		0xE8, 0x79, 0xA8, 0x65, 0x37,	// M
		0xE7, 0x79, 0xA8, 0x69, 0x0E,	// AH
		0xE7, 0x6A, 0xA8, 0x61, 0x29,	// K
		0xE8, 0x6B, 0xA8, 0x59, 0xAD,	// HFC
		0xE8, 0x6B, 0xA8, 0x51, 0xC0,	// PAUSE
		0xE8, 0x6A, 0xA8, 0x51, 0x07,	// I
		0xE8, 0x6A, 0xA8, 0x4B, 0x39,	// NG
		0xE8, 0x6A, 0xA8, 0x49, 0x64,	// B
		0xE7, 0x6A, 0x88, 0x49, 0x11,	// O
		0xE7, 0x5A, 0xB8, 0x51, 0x63,	// W
		0xE7, 0x5A, 0xB8, 0x59, 0x1D,	// R
		0xE8, 0x7A, 0xA8, 0x61, 0x65,	// D	(75% speed - at 100% is good: byte 5 would be 0x25)
		0xE8, 0x79, 0xA8, 0x61, 0xC0,	// PAUSE
		0xE8, 0x70, 0x78, 0x51, 0x00,	// PAUSE
		0xE8, 0x70, 0x78, 0x51, 0x00,	// PAUSE
		0xE8, 0x70, 0x78, 0x59, 0x00,	// PAUSE
		0xE7, 0x79, 0xA8, 0x65, 0x04,	// Y
		0xE7, 0x6A, 0x98, 0x61, 0x16,	// U
		0xE8, 0x6B, 0xA8, 0x61, 0x60,	// L
		0xE8, 0x7C, 0xA8, 0x59, 0x78,	// N
		0xE7, 0x7C, 0xA8, 0x55, 0x0A,	// EH
		0xE8, 0x6B, 0xA8, 0x62, 0x33,	// V
		0xE8, 0x6A, 0xA8, 0x59, 0x1C,	// ER
		0xE8, 0x7A, 0xA8, 0x51, 0x64,	// B
		0xE7, 0x7B, 0x88, 0x51, 0x01,	// E
		0xE8, 0x7C, 0xA8, 0x59, 0x30,	// S	Always crackles at 100%
		0xE8, 0x7D, 0xA8, 0x59, 0x27,	// P
		0xE7, 0x7D, 0x78, 0x61, 0x01,	// E
		0xE8, 0x6C, 0xA8, 0x61, 0x28,	// T
		0xE8, 0x6B, 0xA8, 0x59, 0x32,	// SCH
		0xE8, 0x6A, 0xA8, 0x4D, 0x60,	// L
		0xE7, 0x29, 0xA8, 0x41, 0x0A,	// EH
		0xE8, 0x78, 0xA8, 0x41, 0x30,	// S
		0xE8, 0x70, 0xA8, 0x39, 0xFF,	// LB
		0xE8, 0x70, 0xA8, 0x39, 0x00,	// PAUSE
		0xE8, 0xFF, 0xA8, 0x39, 0x00,	// PAUSE
		0xE8, 0x7B, 0xA8, 0x47, 0xFF,	// LB
	};
	ssi[0].ResetRegisters();
	// Raise CTL, set TRANSITIONED_INFLECTION, and lower CTL
	EventReceived(0xC443, 0x80, 0);	// Reg 3, raise CTL
	EventReceived(0xC440, 0xC0, 0);	// Set TRANSITIONED_INFLECTION
	EventReceived(0xC443, 0x70, 0);	// Reg 3, lower CTL
	
	for (auto i=0; i < sizeof(phrase); i+=5) {
		for (auto j=0; j < 5; ++j)
		{
			EventReceived(0xC440 + (4-j), phrase[i+j], 0);
		}
		
		if (i < (sizeof(phrase) - 1))
		{
			while (!ssi[0].WasIRQTriggered() && ssi[0].IsPlaying())
			{
				// haven't finished yet, wait for irq to send next phoneme
				SDL_Delay(1);
			}
		}
	}
	EventReceived(0xC443, 0x80, 0);
}
///
///
/// ImGUI Interface
///
///

void MockingboardManager::DisplayImGuiChunk()
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
