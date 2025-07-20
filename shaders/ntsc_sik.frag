#ifdef GL_ES
#define COMPAT_PRECISION mediump
precision mediump float;
precision highp usampler2D;
precision highp int;
#else
#define COMPAT_PRECISION
#endif

/*
	NTSC shader by Sik
	It has a special feature in that the TEXTURE_IN alpha channel determines color or mono.
	If it is 0.9, then the pixel is monochrome and will be untouched (alpha will be 1)
	If it is 1, then the pixel is filtered through NTSC color
*/

// Global uniforms
uniform uint ticks;						// ms since start
uniform int frameIsOdd;					// 0 if even frame, 1 if odd frame

// Incoming texture to turn into NTSC
uniform sampler2D TEXIN;			

// for NTSC blending
uniform COMPAT_PRECISION float NTSC_COMB_STR;
uniform COMPAT_PRECISION float NTSC_GAMMA_CORRECTION;
uniform bool bNOFILTERMONO;	// Do not filter monochrome pixels

in vec2 vTexCoords;
out vec4 fragColor;

// For NTSC
float phase[7];		// Colorburst phase (in radians)
float raw_y[7];    // Luma isolated from raw composite signal
float raw_iq[7];   // Chroma isolated from raw composite signal

const mat3 RGB2YIQ = mat3(	0.299,  0.596,  0.211,
							0.587, -0.274, -0.523,
							0.114, -0.322,  0.312
						);

const mat3 YIQ2RGB = mat3(	1.000, 1.000, 1.000,
							0.956,-0.272,-1.106,
							0.621,-0.647, 1.703
						);

// Encodes YIQ into composite
float yiq2raw(vec3 yiq, float phase)
{
	return yiq[0] + yiq[1] * sin(phase) + yiq[2] * cos(phase);
}

void main()
{
	// dummy use to keep unused uniforms
	if (ticks < 0u) {			// Never true
		uint keep = ticks;
	}
	
	fragColor = texture(TEXIN, vTexCoords);
	if (bNOFILTERMONO && fragColor.a < 0.91)	// it's a monochrome pixel or transparent
	{
		// if not transparent, set to full opaque
		if (fragColor.a > 0.001)
			fragColor.a = 1.0;
		return;
	}

	float factorX = 0.5 / 170.667;
	float gamma = NTSC_GAMMA_CORRECTION / 2.2;
	float x = vTexCoords.x;
	float y = vTexCoords.y;
	for (int n = 0; n < 7; n++, x -= factorX * 0.5) {
		phase[n] = x / factorX * 3.1415926;
		float raw1 = 0.0;
		float raw2 = 0.0;
		if (x >= 0.0)
		{
			raw1 = yiq2raw(RGB2YIQ * texture(TEXIN, vec2(x, y)).rgb, phase[n]);
			if (vTexCoords.y > 0.999)
				raw2 = yiq2raw(RGB2YIQ * texture(TEXIN, vec2(x, y - 1.0)).rgb, phase[n] + 3.1415926);
		}
		raw_y[n] = (raw1 + raw2) * 0.5;
		raw_iq[n] = raw1 - (raw1 + raw2) * (NTSC_COMB_STR * 0.5);
	}
	float y_mix = (raw_y[0] + raw_y[1] + raw_y[2] + raw_y[3]) * 0.25;
	float i_mix =
		0.125 * raw_iq[0] * sin(phase[0]) +
		0.25  * raw_iq[1] * sin(phase[1]) +
		0.375 * raw_iq[2] * sin(phase[2]) +
		0.5   * raw_iq[3] * sin(phase[3]) +
		0.375 * raw_iq[4] * sin(phase[4]) +
		0.25  * raw_iq[5] * sin(phase[5]) +
		0.125 * raw_iq[6] * sin(phase[6]);
	float q_mix =
		0.125 * raw_iq[0] * cos(phase[0]) +
		0.25  * raw_iq[1] * cos(phase[1]) +
		0.375 * raw_iq[2] * cos(phase[2]) +
		0.5   * raw_iq[3] * cos(phase[3]) +
		0.375 * raw_iq[4] * cos(phase[4]) +
		0.25  * raw_iq[5] * cos(phase[5]) +
		0.125 * raw_iq[6] * cos(phase[6]);
	// increase the brightness by 80% to align with the other A2 shaders
	fragColor.rgb = pow(YIQ2RGB * vec3(y_mix, i_mix, q_mix) * 1.8,
					vec3(gamma, gamma, gamma));
	fragColor.a = 1.0;
}
