#include "MockingboardManager.h"
#include <iostream>
#include <vector>

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

void MockingboardManager::Initialize(bool _isDual)
{
	isDual = _isDual;
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
	// The default panning is one AY for each speaker
	uint8_t ay_ct = (isDual ? 4 : 2);
	for(uint8_t ayidx = 0; ayidx < ay_ct; ayidx++)
	{
		ay[ayidx].ResetRegisters();
		double pan = (ayidx % 2 == 0 ? 0.0 : 1.0);
		ay[ayidx].SetPan(0, pan, 0);
		ay[ayidx].SetPan(1, pan, 0);
		ay[ayidx].SetPan(2, pan, 0);
	}
}

MockingboardManager::~MockingboardManager() {
	SDL_CloseAudioDevice(audioDevice);
	SDL_Quit();
}

void MockingboardManager::BeginPlay() {
	SDL_PauseAudioDevice(audioDevice, 0); // Start audio playback
	isPlaying = true;
}

void MockingboardManager::StopPlay() {
	SDL_PauseAudioDevice(audioDevice, 1); // Stop audio playback immediately
	isPlaying = false;
}

bool MockingboardManager::IsPlaying() {
	return isPlaying;
}

void MockingboardManager::AudioCallback(void* userdata, uint8_t* stream, int len) {
	MockingboardManager* self = static_cast<MockingboardManager*>(userdata);
	int samples = len / (sizeof(float) * 2); // Number of samples to fill
	std::vector<float> buffer(samples * 2);
	
	for (int i = 0; i < samples; ++i) {
		uint8_t ay_ct = (self->isDual ? 4 : 2);
		for(uint8_t ayidx = 0; ayidx < ay_ct; ayidx++)
		{
			self->ay[ayidx].Process();
		}
		if (self->isDual)
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
	// Only parse 0xC4xx or 0xC5xx events (slots 4 and 5)
	if (((addr >> 8) != 0xC4) && ((addr >> 8) != 0xC5))
		return;
	
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
	if (ay_idx > (isDual ? 3 : 1))
		return;
	ay[ay_idx].SetPan(channel_idx, pan, isEqp);
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
