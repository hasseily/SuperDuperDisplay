#ifndef AYUMI_H
#define AYUMI_H

/** @file   ayumi.h
 @brief  AY-3-8910 and YM2149 emulator
 @author Peter Sovietov
 @author Vladislav Nikonov (docs)
 @author Henri Asseily (C++)
 */

#include <stdint.h>

#define AYUMI_TABLE_FREQ (44100)
#define AYUMI_PAN_WIDTH (8.0f)

constexpr int AYUMI_TONE_CHANNELS = 3;
constexpr int AYUMI_DECIMATE_FACTOR = 8;
constexpr int AYUMI_FIR_SIZE = 192;
constexpr int AYUMI_DC_FILTER_SIZE = 1024;

class Ayumi {
public:
	/** @brief Creates an ayumi structure
	 @param isYM 1 if chip is YM2149, 0 if chip is AY-3-8910
	 @param clockRate clock rate of the chip.
	 @param sampleRate output sample rate
	 */
	Ayumi(bool isYM = false, double clockRate = 1750000, int sampleRate = 44100);

	/** @brief Sets the panning value for the specified sound channel
	 @param index index of sound channel
	 @param pan stereo panning value [0...1]
	 @param isEqp 1 if "equal power" panning is used
	 */
	void SetPan(int index, double pan, bool isEqp);
	/** @brief Sets the tone period value for the specified sound channel
	 @param index index of the sound channel
	 @param period tone period value [0...4095]
	 */
	void SetTone(int index, int period);
	/** @brief Sets the noise
	 @param period noise period value [0-31]
	 */
	void SetNoise(int period);
	/** @brief Sets the mixer value for the specified sound channel
	 @param index index of the sound channel
	 @param t_off 1 if the tone is off
	 @param n_off 1 if the noise is off
	 @param e_on 1 if the envelope is on
	 */
	void SetMixer(int index, bool t_off, bool n_off, bool e_on);
	/** @brief Sets the volume for the specified sound channel
	 @param index index of the sound channel
	 @param volume volume of channel [0...15]
	 */
	void SetVolume(int index, int volume);
	/** @brief Sets the envelope period value
	 @param period envelope period value [0-65535]
	 */
	void SetEnvelope(int period);
	/** @brief Sets the envelope shape value
	 @param shape envelope shape index [0-15]
	 */
	void SetEnvelopeShape(int shape);
	/** @brief Removes the DC offset from the current sample
	 */
	void RemoveDC();
	/** @brief Resets all registers to 0
	 */
	void ResetRegisters();
	/** @brief Renders the next stereo sample in **ay->left** and **ay->right**
	 */
	void Process();
	
	uint8_t value_ora = 0;			// data channel
	uint8_t value_orb = 0;			// command channel
	uint8_t value_oddra = 0;		// data direction (a) -- should always be 0xFF after init
	uint8_t value_oddrb = 0;		// data direction (b) -- should always be 0xFF after init
	uint8_t latched_register = 0;	// currently latched register
	
	struct tone_channel {
		int tone_period = 0;
		int tone_counter = 0;
		int tone = 0;
		bool t_off = 0;
		bool n_off = 0;
		bool e_on = 0;
		int volume = 0;
		double pan_left = 0;
		double pan_right = 0;
	};
	struct interpolator {
		double c[4] = {0};
		double y[4] = {0};
	};
	struct dc_filter {
		double sum = 0;
		double delay[AYUMI_DC_FILTER_SIZE] = {0};
	};
	
	struct tone_channel channels[AYUMI_TONE_CHANNELS];
	int noise_period = 0;
	int noise_counter = 0;
	int noise = 0;
	int envelope_counter = 0;
	int envelope_period = 0;
	int envelope_shape = 0;
	int envelope_segment = 0;
	int envelope = 0;
	const double* dac_table;
	double step = 0;
	double x = 0;
	struct interpolator interpolator_left;
	struct interpolator interpolator_right;
	double fir_left[AYUMI_FIR_SIZE * 2];
	double fir_right[AYUMI_FIR_SIZE * 2];
	int fir_index = 0;
	struct dc_filter dc_left;
	struct dc_filter dc_right;
	int dc_index = 0;
	/// left output sample
	double left = 0;
	/// right output sample
	double right = 0;

	// this is public because of the Envelopes dispatch table
	void ResetSegment();
	// dummy variable to force MSVC to not merge hold_top() and hold_bottom()
	volatile bool bHoldTop;
private:
	int UpdateTone(int index);
	int UpdateNoise();
	int UpdateEnvelope();
	void UpdateMixer();
	static double Decimate(double* x);
	static double DCFilter(struct dc_filter* dc, int index, double x);
	
};

#endif	// AYUMI_H
