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
#include <algorithm>

#define _DEBUG_SSI263 0

constexpr int SSI263_FILTER_FREQ_SILENCE = 0xFF;

SSI263::SSI263()
{
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
	filterFrequency = SSI263_FILTER_FREQ_SILENCE; // Register 4
	articulationRate = 0;
	durationMode = SSI263DR_PHONEME_IMMEDIATE;
	previousDurationMode = durationMode;
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

	for (int i = 0; i < SSI263_DCADJ_BUFLEN; ++i) {
		dcadj_buf[i] = 0.0;
	}
	dcadj_sum = 0.0;
	dcadj_pos = 0;

	bIsEnabled = true;
}

// Pins RS2->RS0 are mapped to the bus's A2->A0
void SSI263::SetRegisterSelect(int addr)
{
	registerSelect = addr & 0b111;	// RS3->RS0
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
				if constexpr (_DEBUG_SSI263 > 0)
					std::cerr << "Generating P:" << phoneme << " Dur:" << phonemeDuration << std::endl;
				GeneratePhonemeSamples();
			}
			break;
		case 0b001:
			{
				// Clear and update bits 3-10
				inflection &= 0b1000'0000'0111;
				inflection |= (byteData << 3);
				irqIsSet = false;
				if constexpr (_DEBUG_SSI263 > 0)
					std::cerr << "I1:" << inflection << std::endl;
			}
			break;
		case 0b010:
			{
				// Clear bits 0-2 and 11 of inflection
				inflection &= 0b0111'1111'1000;;
				// Update bits 0-2
				inflection |= (byteData & 0b111);
				// bit 3 of byteData is bit 11 of inflection
				inflection |= ((byteData & 0b1000) << 11);
				
				// top 4 bits are the speech rate
				speechRate = (byteData >> 4);
				irqIsSet = false;
				if constexpr (_DEBUG_SSI263 > 0)
					std::cerr << "I2:" << inflection << " SpeechRate:" << speechRate << std::endl;

			}
			break;
		case 0b011:
			{
				amplitude = byteData & 0b1111;
				articulationRate = (byteData >> 4) & 0b111;
				if constexpr (_DEBUG_SSI263 > 0)
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
						// SSI263DR_DISABLED_AR disables the A!R output but leaves
						// the duration mode unchanged. So we need to remember the
						// previous duration mode as the actual mode in use if we're
						// in SSI263DR_DISABLED_AR
						if (durationMode != SSI263DR_DISABLED_AR)
							previousDurationMode = durationMode;
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
						std::cerr << "CTL:" << regCTL << " DMode: " << durationMode << std::endl;
				}
			}
			break;
		default:	// Anything 0b1xx
			filterFrequency = byteData;
			if constexpr (_DEBUG_SSI263 > 0)
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
	return (sample - (dcadj_sum / SSI263_DCADJ_BUFLEN));
}

void SSI263::GeneratePhonemeSamples()
{
	// Generate the actual phoneme samples based on:
	//	- amplitude tuning
	//	- phoneme length
	//	- speech rate
	// TODO: articulation rate (linear move to the new sample)
	// TODO: Filter Frequency

	if (!bIsEnabled)
		return;
	
	int _basePhonemeLength = g_nPhonemeInfo[phoneme].nLength;
	auto _offset = g_nPhonemeInfo[phoneme].nOffset;
	
	// Apply pitch factor, no transition. Instant pitch change
	float _speedFactor = 1.0f;
	if ((durationMode == SSI263DR_PHONEME_TRANSITIONED)
		|| (durationMode == SSI263DR_DISABLED_AR && (previousDurationMode == SSI263DR_PHONEME_TRANSITIONED))
		)
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

	// NOTE: The below disregards the register values except for amplitude
	// Any application of register values other than amplitude would necessitate
	// complex resampling that is not effective when the original samples are of
	// such low quality.
	std::vector<float> _rawBuffer;
	for (int i=_offset; i<_offset+_basePhonemeLength; ++i) {
		int16_t _sample = static_cast<int16_t>(g_nPhonemeData[i]);
		// Clamp the value to the valid range for int16_t.
		if (_sample < -32768) _sample = -32768;
		if (_sample > 32767) _sample = 32767;
		// Convert to a float sample in the range [-1.0, 1.0].
		float _sampleF = static_cast<float>(_sample) / 32768.0f;
		if (filterFrequency == SSI263_FILTER_FREQ_SILENCE)	// plays silence
			_sampleF = 0.f;
		if (regCTL)											// it's off
			_sampleF = 0.f;
		// Apply the amplitude (0 to 15, typically 10)
		_sampleF = (_sampleF * amplitude) / 16.f;
		_rawBuffer.push_back(DCAdjust(_sampleF));
	}
	// resample to float 44.1kHz
	if (!_rawBuffer.empty())
	{
		// Resize the resampled buffer to (2 * N - 1) samples.
		for (size_t i = 0; i < _rawBuffer.size() - 1; ++i)
		{
			v_samples.push_back(_rawBuffer[i]);
			v_samples.push_back((_rawBuffer[i] + _rawBuffer[i + 1]) * 0.5f);
		}
		v_samples.push_back(_rawBuffer.back());
	}

	if (_DEBUG_SSI263 > 0)
		std::cerr << "Added phoneme " << phoneme << ", sample count: " << v_samples.size() << std::endl;
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

	float sample_to_return = v_samples[m_currentSampleIdx];
	if constexpr (_DEBUG_SSI263 > 3)
		std::cerr << "Getting sample: " << m_currentSampleIdx << " of " << v_samples.size() << " val: " << sample_to_return << std::endl;
	++m_currentSampleIdx;
	if (m_currentSampleIdx == (v_samples.size() - 100))
	{
		// The phoneme is almost finished, so trigger the IRQ if needed
		if constexpr (_DEBUG_SSI263 > 0)
			std::cerr << " --- Almost finished getting samples of phoneme --- " << std::endl;
		if (durationMode != SSI263DR_DISABLED_AR)
		{
			if (!irqIsSet)
			{
				irqIsSet = true;
				irqShouldProcess = true;
				if constexpr (_DEBUG_SSI263 > 0)
					std::cerr << " >> SETTING IRQ" << std::endl;
			}
		}
		else {
			if constexpr (_DEBUG_SSI263 > 0)
				std::cerr << " >> IRQ not set. Duration mode disabled " << std::endl;
		}
	}

	if (m_currentSampleIdx == v_samples.size()) {
		// Here we really finished playing the phoneme, time to replay it
		m_currentSampleIdx = 0;	// reset to replay the phoneme
	}

	return sample_to_return;
}
