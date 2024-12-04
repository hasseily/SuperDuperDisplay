//
//  SSI263.cpp
//  SuperDuperDisplay
//
//  Created by Henri Asseily on 30/08/2024.
//

#include "SSI263.h"
#include "SSI263Phonemes.h"
#include "common.h"
#include <iostream>

#define _DEBUG_SSI263 0

SSI263::SSI263()
{
	if (SDL_Init(SDL_INIT_AUDIO) < 0) {
		std::cerr << "Failed to initialize SDL Audio: " << SDL_GetError() << std::endl;
		throw std::runtime_error("SDL_Init Audio failed");
	}
	bIsEnabled = false;
	v_samples.reserve(2*SSI263_SAMPLE_RATE);	// To accommodate for slower speech rate
	ResetRegisters();
}

SSI263::~SSI263()
{
	
}

void SSI263::Update()
{
	if (!bIsEnabled)
		return;
	if (!pinCS0)
		return;
	bool isChanged = 
		((pinRead != pinRead_prev)
		|| (pinCS0 != pinCS0_prev)
		|| (pinCS1 != pinCS1_prev));
	if (isChanged && !pinRead && pinCS0 && !pinCS1)
		LoadRegister();
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
	pinRead = pinRead_prev = false;
	pinCS0 = pinCS0_prev = false;
	pinCS1 = pinCS1_prev = false;
	irqIsSet = false;
	irqShouldProcess = false;
	regCTL = true; // Power down
	
	v_samples.clear();
	bIsEnabled = true;
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
	if (_DEBUG_SSI263 > 1)
		std::cerr << "Set Data:" << std::hex << byteData << std::dec << std::endl;
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
	pinRead_prev = pinRead;
	pinRead = pinState;
}

void SSI263::SetCS0(bool pinState)
{
	// Coming from A6 or A5 depending on the SSI chip position on the card
	pinCS0_prev = pinCS0;
	pinCS0 = pinState;
}

void SSI263::SetCS1(bool pinState)
{
	// Set CS1 pin state (IOSELECT)
	pinCS1_prev = pinCS1;
	pinCS1 = pinState;
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

float SSI263::DCAdjust(float sample)
{
	dcadj_sum -= dcadj_buf[dcadj_pos];
	dcadj_sum += sample;
	dcadj_buf[dcadj_pos] = sample;
	dcadj_pos = (dcadj_pos + 1) & (SSI263_DCADJ_BUFLEN-1);
	return (dcadj_sum / SSI263_DCADJ_BUFLEN);
}

void SSI263::GeneratePhonemeSamples()
{
	// Generate the actual phoneme samples based on:
	//	- amplitude tuning
	//	- phoneme length
	//	- speech rate
	// TODO: articulation rate (linear move to the new sample)
	
	if (!bIsEnabled)
		return;
	
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
	if (SSI263_PHONEME_SPEECH_RATE > 15.5f)
		_speedFactor *= 16.f - speechRate;
	else
		_speedFactor *= (16.f - speechRate)/(16.f - SSI263_PHONEME_SPEECH_RATE);

	int _phonemeLength = (int)(_basePhonemeLength / _speedFactor);

	// Apply phoneme duration
	// Phoneme duration is 0->3, which maps to 100%,75%,50%,25% of default duration
	_phonemeLength /= (1 + phonemeDuration);

	std::unique_lock<std::mutex> lock(this->d_mutex_accessing_phoneme);
	v_samples.clear();
	m_currentSampleIdx = 0;
	for (int i=0; i<_phonemeLength; ++i) {
		// Do the resampling due to the new speed
		float _fIndex = i * _speedFactor;
		int _iIndex = (int)_fIndex;
		float _frac = _fIndex - _iIndex;
		
		float _newSample;	// Resampled sample!
		if (_iIndex + 1 < (_basePhonemeLength / (1 + phonemeDuration))) {
			float sample1 = static_cast<float>(g_nPhonemeData[_offset + _iIndex]) / UINT16_MAX;
			int32_t sample2 = static_cast<float>(g_nPhonemeData[_offset + _iIndex + 1]) / UINT16_MAX;
			_newSample = sample1 + _frac * (sample2 - sample1);
		} else {
			_newSample = static_cast<float>(g_nPhonemeData[_offset + _iIndex]) / UINT16_MAX;
		}
		_newSample = (2.f * _newSample) - 1.f;
		// Apply the amplitude (0 to 16, typically 10)
		_newSample = (_newSample * amplitude) / 16.f;
		if (_newSample > 1.f) _newSample = 1.f;
		if (_newSample < -1.f) _newSample = -1.f;

		v_samples.push_back(DCAdjust(_newSample));
	}
	if (_DEBUG_SSI263 > 0)
		std::cerr << "Added samples: " << _basePhonemeLength << std::endl;
}

// Call GetSample on every audio callback from the main audio stream
// It sets the IRQ when the phoneme is done playing.
// Like the real SSI263, the phoneme is replayed when it reaches the end (after the IRQ is triggered)
float SSI263::GetSample() {
	if ((!bIsEnabled) || regCTL)
		return 0.f;
	std::unique_lock<std::mutex> lock(this->d_mutex_accessing_phoneme);
	if (v_samples.size() == 0) {
		return 0.f;
	}

	// Right now the samples are at 22,050 Hz, which is half the sample size of the main audio stream (44,100)
	// So each sample will be duplicated.
	const int freq_mult = _AUDIO_SAMPLE_RATE / SSI263_SAMPLE_RATE;
	float sample_to_return = v_samples[m_currentSampleIdx/freq_mult];
	if (_DEBUG_SSI263 > 3)
		std::cerr << "Getting sample: " << m_currentSampleIdx / freq_mult << " of " << v_samples.size() << " val: " << sample_to_return << std::endl;
	++m_currentSampleIdx;
	if (m_currentSampleIdx == (v_samples.size() * freq_mult))
	{
		// this was the second call to the last sample of the phoneme, so trigger the IRQ if needed
		if (durationMode != SSI263DR_DISABLED_AR)
		{
			if (!irqIsSet)
			{
				irqIsSet = true;
				irqShouldProcess = true;
			}
		}
		m_currentSampleIdx = 0;	// reset to replay the phoneme
	}
	return sample_to_return;
}
