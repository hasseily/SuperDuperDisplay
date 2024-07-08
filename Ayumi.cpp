#include "Ayumi.h"

#include <iostream>
#include <string.h>
#include <math.h>

static const double AY_dac_table[] = {
	0.0, 0.0,
	0.00999465934234, 0.00999465934234,
	0.0144502937362, 0.0144502937362,
	0.0210574502174, 0.0210574502174,
	0.0307011520562, 0.0307011520562,
	0.0455481803616, 0.0455481803616,
	0.0644998855573, 0.0644998855573,
	0.107362478065, 0.107362478065,
	0.126588845655, 0.126588845655,
	0.20498970016, 0.20498970016,
	0.292210269322, 0.292210269322,
	0.372838941024, 0.372838941024,
	0.492530708782, 0.492530708782,
	0.635324635691, 0.635324635691,
	0.805584802014, 0.805584802014,
	1.0, 1.0
};

static const double YM_dac_table[] = {
	0.0, 0.0,
	0.00465400167849, 0.00772106507973,
	0.0109559777218, 0.0139620050355,
	0.0169985503929, 0.0200198367285,
	0.024368657969, 0.029694056611,
	0.0350652323186, 0.0403906309606,
	0.0485389486534, 0.0583352407111,
	0.0680552376593, 0.0777752346075,
	0.0925154497597, 0.111085679408,
	0.129747463188, 0.148485542077,
	0.17666895552, 0.211551079576,
	0.246387426566, 0.281101701381,
	0.333730067903, 0.400427252613,
	0.467383840696, 0.53443198291,
	0.635172045472, 0.75800717174,
	0.879926756695, 1.0
};

static void slide_up(Ayumi* ay) {
	ay->envelope += 1;
	if (ay->envelope > 31) {
		ay->envelope_segment ^= 1;
		ay->ResetSegment();
	}
}

static void slide_down(Ayumi* ay) {
	ay->envelope -= 1;
	if (ay->envelope < 0) {
		ay->envelope_segment ^= 1;
		ay->ResetSegment();
	}
}

static void hold_top(Ayumi* ay) {
	(void) ay;
}

static void hold_bottom(Ayumi* ay) {
	(void) ay;
}

static void (* const Envelopes[][2])(Ayumi*) = {
	{slide_down, hold_bottom},
	{slide_down, hold_bottom},
	{slide_down, hold_bottom},
	{slide_down, hold_bottom},
	{slide_up, hold_bottom},
	{slide_up, hold_bottom},
	{slide_up, hold_bottom},
	{slide_up, hold_bottom},
	{slide_down, slide_down},
	{slide_down, hold_bottom},
	{slide_down, slide_up},
	{slide_down, hold_top},
	{slide_up, slide_up},
	{slide_up, hold_top},
	{slide_up, slide_down},
	{slide_up, hold_bottom}
};

// Constructor
Ayumi::Ayumi(bool isYM, double clockRate, int sampleRate) {
	int i;
	step = clockRate / (sampleRate * 8 * AYUMI_DECIMATE_FACTOR);
	dac_table = isYM ? YM_dac_table : AY_dac_table;
	noise = 1;
	SetEnvelope(1);
	for (i = 0; i < AYUMI_TONE_CHANNELS; i += 1) {
		SetTone(i, 1);
		SetVolume(i, 0);
	}
}

// Public methods
void Ayumi::SetPan(int index, double pan, bool isEqp) {
	if (isEqp) {
		channels[index].pan_left = sqrt(1 - pan);
		channels[index].pan_right = sqrt(pan);
	} else {
		channels[index].pan_left = 1 - pan;
		channels[index].pan_right = pan;
	}
}

void Ayumi::SetTone(int index, int period) {
	period &= 0xfff;
	channels[index].tone_period = (period == 0) | period;
}

void Ayumi::SetNoise(int period) {
	period &= 0x1f;
	noise_period = (period == 0) | period;
}

void Ayumi::SetMixer(int index, bool t_off, bool n_off, bool e_on) {
	channels[index].t_off = t_off & 1;
	channels[index].n_off = n_off & 1;
	channels[index].e_on = e_on;
}

void Ayumi::SetVolume(int index, int volume) {
	channels[index].volume = volume & 0xf;
}

void Ayumi::SetEnvelope(int period) {
	period &= 0xffff;
	envelope_period = (period == 0) | period;
}

void Ayumi::SetEnvelopeShape(int shape) {
	envelope_shape = shape & 0xf;
	envelope_counter = 0;
	envelope_segment = 0;
	ResetSegment();
}

void Ayumi::RemoveDC() {
	left = DCFilter(&dc_left, dc_index, left);
	right = DCFilter(&dc_right, dc_index, right);
	dc_index = (dc_index + 1) & (AYUMI_DC_FILTER_SIZE - 1);
}

void Ayumi::ResetRegisters() {
	SetTone(0, 0);			// reg 0 & 1
	SetTone(1, 0);			// reg 2 & 3
	SetTone(2, 0);			// reg 4 & 5
	SetNoise(0);			// reg 6
	channels[0].t_off = 1;	// reg 5
	channels[1].t_off = 1;
	channels[2].t_off = 1;
	channels[0].n_off = 1;
	channels[1].n_off = 1;
	channels[2].n_off = 1;
	SetVolume(0, 0);		// reg 8
	channels[0].e_on = 0;
	SetVolume(1, 0);		// reg 9
	channels[1].e_on = 0;
	SetVolume(2, 0);		// reg 10
	channels[2].e_on = 0;
	SetEnvelope(0);			// reg 11 & 12
	SetEnvelopeShape(0);	// reg 13
}

void Ayumi::Process() {
	int i;
	double y1;
	double* c_left = interpolator_left.c;
	double* y_left = interpolator_left.y;
	double* c_right = interpolator_right.c;
	double* y_right = interpolator_right.y;
	double* _fir_left = &fir_left[AYUMI_FIR_SIZE - fir_index * AYUMI_DECIMATE_FACTOR];
	double* _fir_right = &fir_right[AYUMI_FIR_SIZE - fir_index * AYUMI_DECIMATE_FACTOR];
	fir_index = (fir_index + 1) % (AYUMI_FIR_SIZE / AYUMI_DECIMATE_FACTOR - 1);
	for (i = AYUMI_DECIMATE_FACTOR - 1; i >= 0; i -= 1) {
		x += step;
		if (x >= 1) {
			x -= 1;
			y_left[0] = y_left[1];
			y_left[1] = y_left[2];
			y_left[2] = y_left[3];
			y_right[0] = y_right[1];
			y_right[1] = y_right[2];
			y_right[2] = y_right[3];
			UpdateMixer();
			y_left[3] = left;
			y_right[3] = right;
			y1 = y_left[2] - y_left[0];
			c_left[0] = 0.5 * y_left[1] + 0.25 * (y_left[0] + y_left[2]);
			c_left[1] = 0.5 * y1;
			c_left[2] = 0.25 * (y_left[3] - y_left[1] - y1);
			y1 = y_right[2] - y_right[0];
			c_right[0] = 0.5 * y_right[1] + 0.25 * (y_right[0] + y_right[2]);
			c_right[1] = 0.5 * y1;
			c_right[2] = 0.25 * (y_right[3] - y_right[1] - y1);
		}
		_fir_left[i] = (c_left[2] * x + c_left[1]) * x + c_left[0];
		_fir_right[i] = (c_right[2] * x + c_right[1]) * x + c_right[0];
	}
	left = Decimate(_fir_left);
	right = Decimate(_fir_right);
}

// Private methods
void Ayumi::ResetSegment() {
	if (Envelopes[envelope_shape][envelope_segment] == slide_down
		|| Envelopes[envelope_shape][envelope_segment] == hold_top) {
		envelope = 31;
		return;
	}
	envelope = 0;
}

int Ayumi::UpdateTone(int index) {
	struct tone_channel* ch = &channels[index];
	ch->tone_counter += 1;
	if (ch->tone_counter >= ch->tone_period) {
		ch->tone_counter = 0;
		ch->tone ^= 1;
	}
	return ch->tone;
}

int Ayumi::UpdateNoise() {
	int bit0x3;
	noise_counter += 1;
	if (noise_counter >= (noise_period << 1)) {
		noise_counter = 0;
		bit0x3 = ((noise ^ (noise >> 3)) & 1);
		noise = (noise >> 1) | (bit0x3 << 16);
	}
	return noise & 1;
}

int Ayumi::UpdateEnvelope() {
	envelope_counter += 1;
	if (envelope_counter >= envelope_period) {
		envelope_counter = 0;
		Envelopes[envelope_shape][envelope_segment](this);
	}
	return envelope;
}

void Ayumi::UpdateMixer() {
	int i;
	int out;
	int noise = UpdateNoise();
	int envelope = UpdateEnvelope();
	left = 0;
	right = 0;
	for (i = 0; i < AYUMI_TONE_CHANNELS; i += 1) {
		out = (UpdateTone(i) | channels[i].t_off) & (noise | channels[i].n_off);
		out *= channels[i].e_on ? envelope : channels[i].volume * 2 + 1;
		left += dac_table[out] * channels[i].pan_left;
		right += dac_table[out] * channels[i].pan_right;
	}
}

double Ayumi::Decimate(double* x) {
	double y = -0.0000046183113992051936 * (x[1] + x[191]) +
	-0.00001117761640887225 * (x[2] + x[190]) +
	-0.000018610264502005432 * (x[3] + x[189]) +
	-0.000025134586135631012 * (x[4] + x[188]) +
	-0.000028494281690666197 * (x[5] + x[187]) +
	-0.000026396828793275159 * (x[6] + x[186]) +
	-0.000017094212558802156 * (x[7] + x[185]) +
	0.000023798193576966866 * (x[9] + x[183]) +
	0.000051281160242202183 * (x[10] + x[182]) +
	0.00007762197826243427 * (x[11] + x[181]) +
	0.000096759426664120416 * (x[12] + x[180]) +
	0.00010240229300393402 * (x[13] + x[179]) +
	0.000089344614218077106 * (x[14] + x[178]) +
	0.000054875700118949183 * (x[15] + x[177]) +
	-0.000069839082210680165 * (x[17] + x[175]) +
	-0.0001447966132360757 * (x[18] + x[174]) +
	-0.00021158452917708308 * (x[19] + x[173]) +
	-0.00025535069106550544 * (x[20] + x[172]) +
	-0.00026228714374322104 * (x[21] + x[171]) +
	-0.00022258805927027799 * (x[22] + x[170]) +
	-0.00013323230495695704 * (x[23] + x[169]) +
	0.00016182578767055206 * (x[25] + x[167]) +
	0.00032846175385096581 * (x[26] + x[166]) +
	0.00047045611576184863 * (x[27] + x[165]) +
	0.00055713851457530944 * (x[28] + x[164]) +
	0.00056212565121518726 * (x[29] + x[163]) +
	0.00046901918553962478 * (x[30] + x[162]) +
	0.00027624866838952986 * (x[31] + x[161]) +
	-0.00032564179486838622 * (x[33] + x[159]) +
	-0.00065182310286710388 * (x[34] + x[158]) +
	-0.00092127787309319298 * (x[35] + x[157]) +
	-0.0010772534348943575 * (x[36] + x[156]) +
	-0.0010737727700273478 * (x[37] + x[155]) +
	-0.00088556645390392634 * (x[38] + x[154]) +
	-0.00051581896090765534 * (x[39] + x[153]) +
	0.00059548767193795277 * (x[41] + x[151]) +
	0.0011803558710661009 * (x[42] + x[150]) +
	0.0016527320270369871 * (x[43] + x[149]) +
	0.0019152679330965555 * (x[44] + x[148]) +
	0.0018927324805381538 * (x[45] + x[147]) +
	0.0015481870327877937 * (x[46] + x[146]) +
	0.00089470695834941306 * (x[47] + x[145]) +
	-0.0010178225878206125 * (x[49] + x[143]) +
	-0.0020037400552054292 * (x[50] + x[142]) +
	-0.0027874356824117317 * (x[51] + x[141]) +
	-0.003210329988021943 * (x[52] + x[140]) +
	-0.0031540624117984395 * (x[53] + x[139]) +
	-0.0025657163651900345 * (x[54] + x[138]) +
	-0.0014750752642111449 * (x[55] + x[137]) +
	0.0016624165446378462 * (x[57] + x[135]) +
	0.0032591192839069179 * (x[58] + x[134]) +
	0.0045165685815867747 * (x[59] + x[133]) +
	0.0051838984346123896 * (x[60] + x[132]) +
	0.0050774264697459933 * (x[61] + x[131]) +
	0.0041192521414141585 * (x[62] + x[130]) +
	0.0023628575417966491 * (x[63] + x[129]) +
	-0.0026543507866759182 * (x[65] + x[127]) +
	-0.0051990251084333425 * (x[66] + x[126]) +
	-0.0072020238234656924 * (x[67] + x[125]) +
	-0.0082672928192007358 * (x[68] + x[124]) +
	-0.0081033739572956287 * (x[69] + x[123]) +
	-0.006583111539570221 * (x[70] + x[122]) +
	-0.0037839040415292386 * (x[71] + x[121]) +
	0.0042781252851152507 * (x[73] + x[119]) +
	0.0084176358598320178 * (x[74] + x[118]) +
	0.01172566057463055 * (x[75] + x[117]) +
	0.013550476647788672 * (x[76] + x[116]) +
	0.013388189369997496 * (x[77] + x[115]) +
	0.010979501242341259 * (x[78] + x[114]) +
	0.006381274941685413 * (x[79] + x[113]) +
	-0.007421229604153888 * (x[81] + x[111]) +
	-0.01486456304340213 * (x[82] + x[110]) +
	-0.021143584622178104 * (x[83] + x[109]) +
	-0.02504275058758609 * (x[84] + x[108]) +
	-0.025473530942547201 * (x[85] + x[107]) +
	-0.021627310017882196 * (x[86] + x[106]) +
	-0.013104323383225543 * (x[87] + x[105]) +
	0.017065133989980476 * (x[89] + x[103]) +
	0.036978919264451952 * (x[90] + x[102]) +
	0.05823318062093958 * (x[91] + x[101]) +
	0.079072012081405949 * (x[92] + x[100]) +
	0.097675998716952317 * (x[93] + x[99]) +
	0.11236045936950932 * (x[94] + x[98]) +
	0.12176343577287731 * (x[95] + x[97]) +
	0.125 * x[96];
	memcpy(&x[AYUMI_FIR_SIZE - AYUMI_DECIMATE_FACTOR], x, AYUMI_DECIMATE_FACTOR * sizeof(double));
	return y;
}

double Ayumi::DCFilter(struct dc_filter* dc, int index, double x) {
	dc->sum += -dc->delay[index] + x;
	dc->delay[index] = x;
	return x - dc->sum / AYUMI_DC_FILTER_SIZE;
}
