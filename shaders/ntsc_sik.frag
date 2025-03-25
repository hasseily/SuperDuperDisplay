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
uniform int ticks;						// ms since start
uniform int frameIsOdd;					// 0 if even frame, 1 if odd frame

// Incoming texture to turn into NTSC
uniform sampler2D TEXIN;			

// for NTSC blending
uniform COMPAT_PRECISION float NTSC_COMB_STR;
uniform COMPAT_PRECISION float NTSC_GAMMA_CORRECTION;

in vec2 vTexCoords;
out vec4 fragColor;

// For NTSC
float phase[7];		// Colorburst phase (in radians)
float raw_y[7];    // Luma isolated from raw composite signal
float raw_iq[7];   // Chroma isolated from raw composite signal

// Converts from RGB to YIQ
vec3 rgba2yiq(vec4 rgba)
{
	return vec3(
				rgba[0] * 0.3 + rgba[1] * 0.59 + rgba[2] * 0.11,
				rgba[0] * 0.599 + rgba[1] * -0.2773 + rgba[2] * -0.3217,
				rgba[0] * 0.213 + rgba[1] * -0.5251 + rgba[2] * 0.3121
				);
}
// Encodes YIQ into composite
float yiq2raw(vec3 yiq, float phase)
{
	return yiq[0] + yiq[1] * sin(phase) + yiq[2] * cos(phase);
}
// Converts from YIQ to RGB
vec4 yiq2rgba(vec3 yiq)
{
	return vec4(
				yiq[0] + yiq[1] * 0.9469 + yiq[2] * 0.6236,
				yiq[0] - yiq[1] * 0.2748 - yiq[2] * 0.6357,
				yiq[0] - yiq[1] * 1.1 + yiq[2] * 1.7,
				1.0
				);
}


void main()
{
	fragColor = texture(TEXIN, vTexCoords);
	if (fragColor.a < 0.91)	// it's a monochrome pixel or transparent
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
		float raw1 = yiq2raw(rgba2yiq(texture(TEXIN, vec2(x, y))), phase[n]);
		float raw2 = yiq2raw(rgba2yiq(texture(TEXIN, vec2(x, y - 1.0))), phase[n] + 3.1415926);
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
	fragColor = pow(yiq2rgba(vec3(y_mix, i_mix, q_mix)),
					vec4(gamma, gamma, gamma, 1.0));
}
