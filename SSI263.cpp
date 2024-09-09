//
//  SSI263.cpp
//  SuperDuperDisplay
//
//  Created by Henri Asseily on 30/08/2024.
//

#include "SSI263.h"
#include "SSI263Phonemes.h"
#include <iostream>

#define _DEBUG_SSI263 1

SSI263::SSI263()
{
	if (SDL_Init(SDL_INIT_AUDIO) < 0) {
		std::cerr << "Failed to initialize SDL Audio: " << SDL_GetError() << std::endl;
		throw std::runtime_error("SDL_Init Audio failed");
	}
	audioDevice = 0;
	v_samples.reserve(2*SSI263_SAMPLE_RATE);
	ResetRegisters();
}

SSI263::~SSI263()
{
	
}

void SSI263::ResetRegisters()
{
	// Reset all registers to their default values on power-on
	speechRate = 0;
	filterFrequency = 0xFF; // Register 4
	articulationRate = 0;
	durationMode = SSI263DR_DISABLED_AR;
	phoneme = 0;
	amplitude = 0;
	inflection = 0;
	
	registerSelect = 0;
	byteData = 0;
	pinRead = false;
	pinCS0 = false;
	pinCS1 = false;
	irqIsSet = false;
	irqShouldProcess = false;
	regCTL = true; // Power down
	
	v_samples.clear();
	
	if (audioDevice == 0)
	{
		SDL_zero(audioSpec);
		audioSpec.freq = SSI263_SAMPLE_RATE;
		audioSpec.format = AUDIO_S16LSB;
		audioSpec.channels = 1;
		audioSpec.samples = 512;
		audioSpec.callback = SSI263::AudioCallback;
		audioSpec.userdata = this;
		
		audioDevice = SDL_OpenAudioDevice(NULL, 0, &audioSpec, NULL, 0);
	} else {
		SDL_ClearQueuedAudio(audioDevice);
	}
	
	if (audioDevice == 0) {
		std::cerr << "Failed to open SSI263 audio device: " << SDL_GetError() << std::endl;
		SDL_Quit();
		throw std::runtime_error("SDL_OpenAudioDevice SSI263 failed");
	}
}

// Pins RS2->RS0 are mapped to the bus's A2->A0
void SSI263::SetRegisterSelect(int val)
{
	registerSelect = val & 0b111;	// RS3->RS0
}

void SSI263::SetData(int data)
{
	// This just sets the pin values
	// The chip reads the pin values when any of R/!W, CS0 or !CS1 changes
	// and the final result is 0,1,0
	byteData = data & 0xFF;
}

void SSI263::LoadRegister()
{
	switch (registerSelect) {
		case 0b000:
			{
				phoneme 		= byteData & 0b0011'1111;
				phonemeDuration = ((byteData & 0b1100'0000) >> 6);
				irqIsSet = false;
				if (_DEBUG_SSI263 > 0)
					std::cerr << "Generating P:" << phoneme << " Dur:" << phonemeDuration << std::endl;
				GeneratePhonemeSamples();
			}
			break;
		case 0b001:
			{
				// Clear and update bits 3-10
				inflection &= ~(0x00FF << 3);
				inflection |= (byteData << 3);
				irqIsSet = false;
				if (_DEBUG_SSI263 > 0)
					std::cerr << "I1:" << inflection << std::endl;
			}
			break;
		case 0b010:
			{
				// Clear bits 0-2 and 11 of inflection
				inflection &= 0x7f8;
				// Update bits 0-2
				inflection |= (byteData & 0b111);
				// bit 3 of byteData is bit 11 of inflection
				inflection |= ((byteData & 0b1000) << 11);
				
				// top 4 bits are the speech rate
				speechRate = (byteData >> 4);
				irqIsSet = false;
				if (_DEBUG_SSI263 > 0)
					std::cerr << "I2:" << inflection << " SpeechRate:" << speechRate << std::endl;

			}
			break;
		case 0b011:
			{
				amplitude = byteData & 0b1111;
				articulationRate = (byteData >> 4) & 0b111;
				if (_DEBUG_SSI263 > 0)
					std::cerr << "Amp:" << amplitude << " Artic:" << articulationRate << std::endl;

				// if CTL goes from high to low:
				// - set the duration mode to the current phoneme duration
				// - bring the device to "power up"
				// else:
				// - reset the IRQ
				// - bring the device to "power down"
				if (regCTL != (bool)(byteData >> 7))	// change in CTL
				{
					regCTL = !regCTL;
					if (regCTL == false)
					{
						durationMode = phonemeDuration;
						// SSI263DR_FRAME_IMMEDIATE not implemented
						// Also, transitioned mode is not fully implemented,
						// it is effectively immediate
						if (durationMode == SSI263DR_FRAME_IMMEDIATE)
							durationMode = SSI263DR_PHONEME_IMMEDIATE;
					}
					else {
						irqIsSet = false;
					}

					SDL_PauseAudioDevice(audioDevice, regCTL);
					if (_DEBUG_SSI263 > 0)
						std::cerr << "CTL:" << regCTL << " PAUSE is 1!" << std::endl;
				}
			}
			break;
		default:	// Anything 0b1xx
			filterFrequency = byteData;
			if (_DEBUG_SSI263 > 0)
				std::cerr << "FF:" << filterFrequency << std::endl;
			break;
	}
}

// SetReadMode(), SetCS0() and SetCS1() trigger loading of register
// only if after the change, R/!W, CS0 and !CS1 are respectively 0,1 and 0
void SSI263::SetReadMode(bool pinState)
{
	// Set R/W mode, true for R (read)
	bool isChanged = (pinRead != pinState);
	pinRead = pinState;
	if (isChanged && !pinRead && pinCS0 && !pinCS1)
		LoadRegister();
}

void SSI263::SetCS0(bool pinState)
{
	// Coming from A6 or A5 depending on the SSI chip position on the card
	bool isChanged = (pinCS0 != pinState);
	pinCS0 = pinState;
	if (isChanged && !pinRead && pinCS0 && !pinCS1)
		LoadRegister();
}

void SSI263::SetCS1(bool pinState)
{
	// Set CS1 pin state (IOSELECT)
	bool isChanged = (pinCS1 != pinState);
	pinCS1 = pinState;
	if (isChanged && !pinRead && pinCS0 && !pinCS1)
		LoadRegister();
}

bool SSI263::WasIRQTriggered()
{
	if (irqShouldProcess)
	{
		irqShouldProcess = false;
		return true;
	}
	return false;
}

bool SSI263::IsPlaying()
{
	return (SDL_GetAudioDeviceStatus(audioDevice) == SDL_AUDIO_PLAYING);
}

void SSI263::GeneratePhonemeSamples()
{
	// Generate the actual phoneme samples based on:
	//	- amplitude tuning
	//	- phoneme length
	//	- speech rate
	// TODO: articulation rate (linear move to the new sample)
	
	//v_samples.clear();
	
	int _basePhonemeLength = g_nPhonemeInfo[phoneme].nLength;
	auto _offset = g_nPhonemeInfo[phoneme].nOffset;
	
	// Apply pitch factor, no transition. Instant pitch change
	float _speedFactor = 1.0f;
	if (durationMode == SSI263DR_PHONEME_TRANSITIONED)
	{
		// Pitch is 5 bits, and apply as (32 - _inflPitch)
		int _inflPitch = (inflection >> 6) & 0b11111;
		_speedFactor = (32.f - SSI263_INFLECTION_PITCH)/(32.f - _inflPitch);
	}
	else {
		// Pitch is all 12 bits
		_speedFactor = (4096.f - 2048)/(4096.f - inflection);
	}
	// Apply speech rate
	// Should be (16.f - speechRate) but that's too big a change so we attenuate
	_speedFactor *= (1024.f - speechRate)/(1024.f - SSI263_PHONEME_SPEECH_RATE);

	int _phonemeLength = (int)(_basePhonemeLength / _speedFactor);

	// Apply phoneme duration
	// Phoneme duration is 0->3, which maps to 100%,75%,50%,25% of default duration
	_phonemeLength /= (1 + phonemeDuration);
	
	SDL_LockAudioDevice(audioDevice);
	v_samples.clear();
	m_currentSampleIdx = 0;
	for (int i=0; i<_phonemeLength; ++i) {
		// Do the resampling due to the new speed
		float _fIndex = i * _speedFactor;
		int _iIndex = (int)_fIndex;
		float _frac = _fIndex - _iIndex;
		
		int32_t _newSample;	// Resampled sample!
		if (_iIndex + 1 < (_basePhonemeLength / (1 + phonemeDuration))) {
			int16_t sample1 = g_nPhonemeData[_offset + _iIndex];
			int16_t sample2 = g_nPhonemeData[_offset + _iIndex + 1];
			_newSample = static_cast<uint32_t>(sample1 + _frac * (sample2 - sample1));
		} else {
			_newSample = g_nPhonemeData[_offset + _iIndex];
		}
		
		// Apply the amplitude (0 to 16, typically 10)
		_newSample = (_newSample * amplitude) / 16;
		
		// Clamp the result to stay within 16-bit signed integer range
		// and more.
		// TODO:  Clamping at 32768 creates artifacts
		if (_newSample > 32768) _newSample = 32768;
		if (_newSample < -32768) _newSample = -32768;
		
		v_samples.push_back(_newSample & 0xFF);
		v_samples.push_back((_newSample >> 8) & 0xFF);
	}
	SDL_UnlockAudioDevice(audioDevice);
	if (_DEBUG_SSI263 > 0)
		std::cerr << "Added samples: " << _basePhonemeLength << std::endl;
}

// The audio callback sets the IRQ when the phoneme is done playing.
// Contrary to the real SSI263, we do not replay the phoneme
void SSI263::AudioCallback(void* userdata, uint8_t* stream, int len) {
	SSI263* self = static_cast<SSI263*>(userdata);
	auto& _vecS = self->v_samples;
	auto& _currIdx = self->m_currentSampleIdx;
	int samples_to_copy = len;
	samples_to_copy = std::min(samples_to_copy, static_cast<int>(_vecS.size()) - _currIdx);

	if (_DEBUG_SSI263 > 1)
		std::cerr << "samples: " << samples_to_copy << " / " << _currIdx << " / " << _vecS.size() << std::endl;

	SDL_memcpy(stream, _vecS.data() + _currIdx, samples_to_copy);
	_currIdx += samples_to_copy;
	
	// rollover
	if (samples_to_copy < len) {
		// TODO: CHECK WHICH IS HAPPENING IN THE REAL MOCKINGBOARD
		// HARDCODED LAST SAMPLES
		_currIdx = static_cast<int>(_vecS.size()) - self->audioSpec.samples*2;
		SDL_memcpy(stream + samples_to_copy, _vecS.data() + _currIdx, (len - samples_to_copy));
		// LAST X SAMPLES
		// _currIdx = static_cast<int>(_vecS.size()) - (len - samples_to_copy);
		// SDL_memcpy(stream + samples_to_copy, _vecS.data() + _currIdx, (len - samples_to_copy));
		// ROLL OVER
		// SDL_memcpy(stream + samples_to_copy, _vecS.data(), (len - samples_to_copy));
		// _currIdx = (len - samples_to_copy);
		
		// Determine if we need to trigger the IRQ
		if (self->durationMode != SSI263DR_DISABLED_AR)
		{
			if (!self->irqIsSet)
			{
				self->irqIsSet = true;
				self->irqShouldProcess = true;
			}
		}
	}
	
}
