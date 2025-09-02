#ifdef GL_ES
#define COMPAT_PRECISION mediump
precision mediump float;
precision highp usampler2D;
precision highp int;
#else
#define COMPAT_PRECISION
#endif

/*
	NTSC shader
	It has a special feature in that the TEXTURE_IN alpha channel determines color or mono.
	If it is 0.95, then the pixel is monochrome and will be untouched (alpha will be 1)
	If it is Between 0.0001 and 0.95, then the pixel is filtered through NTSC color
	Output alpha is always set to 1
*/

// Global uniforms
uniform int frameIsOdd;					// 0 if even frame, 1 if odd frame

// Incoming texture in linear RGB
uniform sampler2D TEXIN;			

// for NTSC blending
uniform COMPAT_PRECISION float NTSC_STR;
uniform COMPAT_PRECISION float NTSC_COMB_STR;
uniform COMPAT_PRECISION float NTSC_GAMMA_CORRECTION;
uniform bool bNOFILTERMONO;	// Do not filter monochrome pixels

// tell the shader about color spaces
bool INPUT_IS_LINEAR = true;       // true if TEXIN is linear
bool OUTPUT_LINEAR = true;         // true if we want linear out to the next stage

in vec2 vTexCoords;
out vec4 fragColor;

// ---- Helpers for gamma encode/decode ----
COMPAT_PRECISION float safePow(float x, float p)	{ return pow(max(x, 0.0), p); }
COMPAT_PRECISION vec3  toVideo(vec3 lin)			{ return vec3(safePow(lin.r, 1.0/2.2), safePow(lin.g, 1.0/2.2), safePow(lin.b, 1.0/2.2)); }
COMPAT_PRECISION vec3  toLinear(vec3 vid)			{ return vec3(safePow(vid.r, 2.2),     safePow(vid.g, 2.2),     safePow(vid.b, 2.2));     }

// For NTSC
float phase[7];		// Colorburst phase (in radians)
float raw_y[7];    // Luma isolated from raw composite signal
float raw_iq[7];   // Chroma isolated from raw composite signal

// NOTE: These matrices assume gamma-encoded "video" RGB.
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
	// Fetch alpha first for mono/transparent logic (no color-space impact)
	vec4 srcSample = texture(TEXIN, vTexCoords);
	if (bNOFILTERMONO && (srcSample.a < 1.0)) {
		if (srcSample.a > 0.95) { // monochrome pixel passthrough
		fragColor = vec4(srcSample.rgb, 1.0);
		return;
		} else if (srcSample.a < 0.001) { // fully transparent
			fragColor = srcSample;
			return;
		}
	}

	// NTSC parameters
	float factorX = NTSC_STR / 170.667;
	float x = vTexCoords.x;
	float y = vTexCoords.y;

	// All NTSC math below expects gamma-encoded RGB.

	for (int n = 0; n < 7; n++, x -= factorX * 0.5) {
		phase[n] = x / factorX * 3.1415926;
		float raw1 = 0.0;
		float raw2 = 0.0;

		if (x >= 0.0) {
			vec3 s1_video = toVideo(texture(TEXIN, vec2(x, y)).rgb);	// from linear
			raw1 = yiq2raw(RGB2YIQ * s1_video, phase[n]);

			if (vTexCoords.y > 0.999) {
				vec3 s2_video = toVideo(texture(TEXIN, vec2(x, y - 1.0)).rgb);	// from linear
				raw2 = yiq2raw(RGB2YIQ * s2_video, phase[n] + 3.1415926);
			}
		}

		raw_y[n]	= (raw1 + raw2) * 0.5;
		raw_iq[n]	= raw1 - (raw1 + raw2) * (NTSC_COMB_STR * 0.5);
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

	// Back to RGB (still in video-gamma space)

	// increase the brightness by 80% to align with the other A2 shaders
	vec3 rgbVideo = YIQ2RGB * vec3(y_mix, i_mix, q_mix) * 1.8;
	rgbVideo = clamp(rgbVideo, 0.0, 1.0);

	// Optional display gamma tweak in video space (1.0 = neutral)
	if (abs(NTSC_GAMMA_CORRECTION - 1.0) > 1e-5) {
		rgbVideo = vec3(safePow(rgbVideo.r, NTSC_GAMMA_CORRECTION),
						safePow(rgbVideo.g, NTSC_GAMMA_CORRECTION),
						safePow(rgbVideo.b, NTSC_GAMMA_CORRECTION));
	}

	// Convert to linear output space
	fragColor.rgb	= toLinear(rgbVideo);
	fragColor.a		= 1.0;
}
