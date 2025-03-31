/*
By Rikkles.

This shader is a frankenstein and uses parts from:
crt-Cyclon by DariusG
crt-Geom (alternate scanlines)
Mattias CRT
Quillez (main filter)
Dogway's inverse Gamma

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2 of the License, or (at your option)
any later version.

*/

#define pi 3.1415926535897932384626433
#define blck (1.0)/(1.0-BLACK);

#ifdef GL_ES
	#define COMPAT_PRECISION mediump
	precision mediump float;
#else
	#define COMPAT_PRECISION
#endif

///////////////////////////////////////// VERTEX SHADER /////////////////////////////////////////
#if defined(VERTEX)

in vec2 aPos;
in vec2 TexCoord;
//in vec4 COLOR;
out vec2 TexCoords; // Pass texture coordinates to fragment shader
out vec2 scale;
out vec2 ps;

uniform COMPAT_PRECISION int iFrameCount;
uniform COMPAT_PRECISION vec2 OutputSize;
uniform COMPAT_PRECISION vec2 TextureSize;
uniform COMPAT_PRECISION vec2 InputSize;

uniform mat4 uTransform;

void main()
{
	gl_Position = uTransform * vec4(aPos, 0.0, 1.0);
	TexCoords = TexCoord;
	scale = OutputSize.xy/TextureSize.xy;
	ps = 1.0/TextureSize.xy;
}

///////////////////////////////////////// FRAGMENT SHADER /////////////////////////////////////////
#elif defined(FRAGMENT)

uniform COMPAT_PRECISION int iFrameCount;
uniform COMPAT_PRECISION vec2 OutputSize;
uniform COMPAT_PRECISION vec2 TextureSize;
uniform COMPAT_PRECISION vec2 InputSize;
uniform COMPAT_PRECISION vec2 ViewportSize;
uniform COMPAT_PRECISION uint ScanlineCount;
uniform sampler2D A2TextureCurrent;
uniform sampler2D PreviousFrame;
uniform bool bHalveFrameRate;
in vec2 TexCoords;
in vec2 scale;
in vec2 ps;
out vec4 FragColor;

uniform COMPAT_PRECISION int POSTPROCESSING_LEVEL;

uniform COMPAT_PRECISION float GhostingPercent;
uniform COMPAT_PRECISION float BlurSize;

uniform bool bCORNER_SMOOTH;
uniform bool bEXT_GAMMA;
uniform bool bPOTATO; 
uniform bool bSLOT;
uniform bool bVIGNETTE;
uniform COMPAT_PRECISION float BARRELDISTORTION;
uniform COMPAT_PRECISION float BGR;
uniform COMPAT_PRECISION float BLACK; 
uniform COMPAT_PRECISION float BR_DEP; 
uniform COMPAT_PRECISION float BRIGHTNESS;
uniform COMPAT_PRECISION float C_STR;
uniform COMPAT_PRECISION float CONV_B;
uniform COMPAT_PRECISION float CONV_G;
uniform COMPAT_PRECISION float CONV_R;
uniform COMPAT_PRECISION float CORNER;
uniform COMPAT_PRECISION float GB;
uniform COMPAT_PRECISION float MASKH;
uniform COMPAT_PRECISION float MASKL;
uniform COMPAT_PRECISION float MSIZE;
uniform COMPAT_PRECISION float P_GLOW;
uniform COMPAT_PRECISION float RB;
uniform COMPAT_PRECISION float RG;
uniform COMPAT_PRECISION float SATURATION;
uniform COMPAT_PRECISION float SCANLINE_WEIGHT;
uniform COMPAT_PRECISION float SCAN_SPEED;
uniform COMPAT_PRECISION float FILM_GRAIN;
uniform COMPAT_PRECISION float INTERLACE_WEIGHT;
uniform COMPAT_PRECISION float SLOTW;
uniform COMPAT_PRECISION float VIGNETTE_WEIGHT;
uniform COMPAT_PRECISION int iCOLOR_SPACE;
uniform COMPAT_PRECISION int iM_TYPE;
uniform COMPAT_PRECISION int iSCANLINE_TYPE;
uniform COMPAT_PRECISION vec2 vWARP;
uniform COMPAT_PRECISION vec2 vCENTER;
uniform COMPAT_PRECISION vec2 vZOOM;


#define fTime (float(iFrameCount) / 60.0)
#define iResolution OutputSize.xy
#define fragCoord gl_FragCoord.xy

#define GAMMA 2.2

//Decode gamma to linear color
vec4 gamma_decode(vec4 srgb)
{
	if (bEXT_GAMMA)
		return srgb;
	return pow(srgb, vec4(GAMMA,GAMMA,GAMMA,1.0));
}

//Encode linear color to gamma
vec4 gamma_encode(vec4 lrgb)
{
	return pow(lrgb, 1.0/vec4(GAMMA,GAMMA,GAMMA,1.0));
}

// This function merges 2 generated frames: if it's an even frame nothing
// happens, but an odd frame mixes in the previous even frame at 50%
// In main.cpp if we halve the frame rate, every even frame is skipped
// The background buffer isn't flipped and we overwrite it. The end result
// is that the frame rate is halved, and we only display even+odd during
// odd frames
// NOTE: currentColor must be in linear space already
vec4 HalveFrameRate(vec2 coords, vec4 currentColor)
{
	if ((iFrameCount & 1) == 1) {
		vec3 previousColor = gamma_decode(texture(PreviousFrame, coords)).rgb;

		// Perform the mix in linear space
		vec3 linearMix = (currentColor.rgb + previousColor) * 0.5;

		return vec4(linearMix, 1.0);
	}
	return currentColor;
}

// Creates a ghosting effect by mixing in the previous frame
// NOTE: currentColor must be in linear space already
vec4 GenerateGhosting(vec2 coords, vec4 currentColor)
{
	vec4 previousColor = gamma_decode(texture(PreviousFrame, coords));
	// Calculate the intensity levels of both frames
	float currentIntensity = dot(currentColor.rgb, vec3(0.299, 0.587, 0.114));
	float previousIntensity = dot(previousColor.rgb, vec3(0.299, 0.587, 0.114));
	vec4 blended = vec4(0.0,0.0,0.0,0.0);
	if (currentIntensity > previousIntensity)	// move at a fast fixed speed towards higher intensity
		blended = mix(currentColor, previousColor, 0.01);
	else {
		if ((previousIntensity - currentIntensity) < (GhostingPercent*0.0025))
			// As we get closer to the color (the higher the ghosting, the higher the cutoff),
			// at some point we need to accelerate the move. Otherwise at higher ghosting values
			// the color will never be reached (especially visible when fading to black)
			blended = mix(currentColor, previousColor, 0.4);
		else
			blended = mix(currentColor, previousColor, GhostingPercent/100.0);
	}
	return blended;
}

// Use the LODs for the phosphor blur, which is much faster than sampling neighbor points
// NOTE: returns color in linear space
vec4 PhosphorBlur(sampler2D tex, vec2 uv, vec2 resolution, float blurAmount) {
    vec4 color = gamma_decode(textureLod(tex, uv, blurAmount));
    // Increase brightness linearly based on blurAmount.
    float brightnessFactor = 1.0 + blurAmount;
    return clamp(color * brightnessFactor,0.0,1.0);
}

vec3 Mask(vec2 pos, float CGWG) {
	if (iM_TYPE == 0)
		return vec3(1.0);
	vec3 mask = vec3(CGWG);
	if (iM_TYPE == 1){
		if (bPOTATO) {
			return vec3( (1.0-CGWG)*sin(pos.x*pi)+CGWG);
		} else {
			float m = fract(pos.x*0.5);
			if (m < 0.5)
				mask.rb = vec2(1.0);
			else
				mask.g = 1.0;
			return mask;
		}
	}

	if (iM_TYPE == 2) {
		if (bPOTATO) {
			return vec3( (1.0-CGWG)*sin(pos.x*pi*0.6667)+CGWG) ;
		} else {
			float m = fract(pos.x*0.3333);
			if (m<0.3333)
				mask.rgb = (BGR == 0.0) ? vec3(mask.r, mask.g, 1.0) : vec3(1.0, mask.g, mask.b);
			else if (m<0.6666)
				mask.g = 1.0;
			else
				mask.rgb = (BGR == 0.0) ? vec3(1.0, mask.g, mask.b) : vec3(mask.r, mask.g, 1.0);
			return mask;
		}
	}
	return vec3(1.0);
}

float roundCorners(vec2 p, vec2 b, float r)
{
    return length(max(abs(p)-b+r,0.0))-r;
}

float scanlineWeights(float distance, vec3 color, float x) {
	// "wid" controls the width of the scanline beam, for each RGB
	// channel The "weights" lines basically specify the formula
	// that gives you the profile of the beam, i.e. the intensity as
	// a function of distance from the vertical center of the
	// scanline. In this case, it is gaussian if width=2, and
	// becomes nongaussian for larger widths. Ideally this should
	// be normalized so that the integral across the beam is
	// independent of its width. That is, for a narrower beam
	// "weights" should have a higher peak at the center of the
	// scanline than for a wider beam.
	float wid = SCANLINE_WEIGHT + 0.15 * dot(color, vec3(0.25-VIGNETTE_WEIGHT*x));
	float weights = distance / wid;
	return 0.1 * exp(-weights * weights * weights) / wid;
}

// Returns gamma corrected output, compensated for scanline+mask embedded gamma
vec3 inv_gamma(vec3 col, vec3 power) {
	vec3 cir = col-1.0;
	cir *= cir;
	col = mix(sqrt(col),sqrt(1.0-cir),power);
	return col;
}

// standard 6500k
mat3 PAL = mat3 (					
	1.0740 , -0.0574 , -0.0119 ,
	0.0384 , 0.9699 , -0.0059 ,
	-0.0079 , 0.0204 , 0.9884
	);

// standard 6500k
mat3 NTSC = mat3 (				 
	0.9318 , 0.0412 , 0.0217 ,
	0.0135 , 0.9711 , 0.0148 ,
	0.0055 , -0.0143 , 1.0085
	);

// standard 8500k
mat3 NTSC_J = mat3 (					
	0.9501 , -0.0431 , 0.0857 ,
	0.0265 , 0.9278 , 0.0432 ,
	0.0011 , -0.0206 , 1.3153
	);

vec3 slot(vec2 pos) {
	float h = fract(pos.x/SLOTW);
	float v = fract(pos.y);
	
	float odd;
	if (v<0.5)
		odd = 0.0;
	else
		odd = 1.0;

	if (odd == 0.0) {
		if (h<0.5)
			return vec3(0.5);
		else
			return vec3(1.5);
	} else if (odd == 1.0) {
		if (h<0.5)
			return vec3(1.5);
		else
			return vec3(0.5);
	}
}

vec2 Warp(vec2 pos) {
	pos = pos*2.0-1.0;
	pos *= vec2(1.0+pos.y*pos.y*vWARP.x, 1.0+pos.x*pos.x*vWARP.y);
	pos = pos*0.5+0.5;
	return pos;
}

vec2 BarrelDistortion(vec2 uv) {
	vec2 delta = uv - 0.5;
	float delta2 = dot(delta.xy, delta.xy);
	float delta4 = delta2 * delta2;
	float delta_offset = delta4 * BARRELDISTORTION;
	
	vec2 warped = uv + delta * delta_offset;
	return (warped - 0.5) / mix(1.0,1.2,BARRELDISTORTION/5.0) + 0.5;
}

//Canonical noise function; replaced to prevent precision errors
//float rand(vec2 co){
//    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
//}

float rand(vec2 co)
{
    float a = 12.9898;
    float b = 78.233;
    float c = 43758.5453;
    float dt= dot(co.xy ,vec2(a,b));
    float sn= mod(dt,3.14);
    return fract(sin(sn) * c);
}

void main() {

	if (POSTPROCESSING_LEVEL == 0) {
		FragColor = texture(A2TextureCurrent, TexCoords);
		if (bHalveFrameRate || ((GhostingPercent > 0.0001))) {
			FragColor = gamma_decode(FragColor);
			if (bHalveFrameRate)
				FragColor = HalveFrameRate(TexCoords, FragColor);
			if (GhostingPercent > 0.0001)
				FragColor = GenerateGhosting(TexCoords, FragColor);
			FragColor = gamma_encode(FragColor);
		}
		return;
	}

	vec2 q = (TexCoords.xy * TextureSize.xy / InputSize.xy);
    vec2 uv = q;
	float o =2.0*mod(fragCoord.y,2.0)/iResolution.x;

	if (uv.x < 0.0 || uv.x > 1.0)
		discard;
	if (uv.y < 0.0 || uv.y > 1.0)
		discard;

	
// Apply simple horizontal scanline if required and exit
	if (POSTPROCESSING_LEVEL == 1) {
		FragColor = gamma_decode(texture(A2TextureCurrent, TexCoords));
		FragColor.rgb = FragColor.rgb * (1.0 - mod(floor(TexCoords.y * TextureSize.y), 2.0));
		if (bHalveFrameRate)
			FragColor = HalveFrameRate(TexCoords, FragColor);
		if (GhostingPercent > 0.0001)
			FragColor = GenerateGhosting(TexCoords, FragColor);
		FragColor = gamma_encode(FragColor);
		return;
	}

	if (iSCANLINE_TYPE == 1) {
		if (mod(floor(TexCoords.y * TextureSize.y), 2.0) > 0.9)
			discard;
	}

	// Hue matrix inside main() to avoid GLES error
	mat3 hue = mat3 (
		1.0, RG, RB,
		-RG, 1.0, GB,
		-RB, -GB, 1.0
		);
	
// Curvature on both axes
	vec2 pos = Warp(TexCoords);

// If people prefer the BarrelDistortion algo
	pos = BarrelDistortion(pos);

	vec2 bpos = pos;
	vec2 dx = vec2(ps.x,0.0);
	
// Quillez
	vec2 ogl2 = pos*TextureSize.xy;
	vec2 i = floor(pos*TextureSize.xy) + 0.5;
	float f = ogl2.y - i.y;
	pos.y = (i.y + 4.0*f*f*f)*ps.y; // smooth
	pos.x = mix(pos.x, i.x*ps.x, 0.2);

// Rounded corners
	float corn = 1.0;
	if (CORNER > 0.000001) {
		vec2 halfRes = 0.5 * OutputSize.xy;
		float b = 1.0 - roundCorners(pos.xy * OutputSize.xy - halfRes, halfRes, abs(CORNER * OutputSize.x * 30.0));
		if (bCORNER_SMOOTH)
			corn = b/10.0;	// if we want it smooth
		else
			if (b < CORNER)
				discard;
	}

	vec4 res0;
	vec3 res;

	if (BlurSize > 0.001)
		res0 = PhosphorBlur(A2TextureCurrent, pos, TextureSize, BlurSize);
	else
		res0 = gamma_decode(texture(A2TextureCurrent,pos));

	res = res0.rgb;
	// Convergence
	if (C_STR > 0.0001) {
		if ((CONV_R+CONV_G+CONV_B) > 0.001) {
			float resr = gamma_decode(texture(A2TextureCurrent,pos + dx*CONV_R)).r;
			float resg = gamma_decode(texture(A2TextureCurrent,pos + dx*CONV_G)).g;
			float resb = gamma_decode(texture(A2TextureCurrent,pos + dx*CONV_B)).b;

			res = vec3(res0.r*(1.0-C_STR) + resr*C_STR,
					   res0.g*(1.0-C_STR) + resg*C_STR,
					   res0.b*(1.0-C_STR) + resb*C_STR
					   );
		}
	}

	res = clamp(res,0.0,1.0);
	float l = dot(vec3(BR_DEP),res);

 // Color Spaces
	if (iCOLOR_SPACE != 0) {
		if (iCOLOR_SPACE == 1)
			res *= PAL;
		if (iCOLOR_SPACE == 2)
			res *= NTSC;
		if (iCOLOR_SPACE == 3)
			res *= NTSC_J;
		// Apply CRT-like luminances
		res /= vec3(0.24,0.69,0.07);
		res *= vec3(0.29,0.6,0.11); 
		res = clamp(res,0.0,1.0);
	}

	// Mask CGWG definition, used for scanlines
	float CGWG = 0.3;
	if (bSLOT)
		CGWG = mix(MASKL, MASKH, l);

	if (iSCANLINE_TYPE == 2) {
		// New style scanlines, derived from Mattias' shader
		// vignette
		if (VIGNETTE_WEIGHT > 0.00001) {
			float vig = (0.0 + 1.0*16.0*pos.x*pos.y*(1.0-pos.x)*(1.0-pos.y));
			vig = pow(vig,VIGNETTE_WEIGHT);
			res *= vec3(vig);
		}

		if (SCANLINE_WEIGHT > 0.00001) {
			float scans = clamp( 0.35+0.15*sin(2.0*(-fTime * SCAN_SPEED)+pos.y*float(ScanlineCount)*8.0/2.55), 0.0, 1.0);
			float s = pow(scans,SCANLINE_WEIGHT);
			s = pow(s,SCANLINE_WEIGHT);
			res = res*vec3(s);
		}

		// flicker
		res *= 1.0+INTERLACE_WEIGHT*sin(300.0*fTime);

		// film grain
		res *= vec3(1.0) - FILM_GRAIN * vec3(
					rand(pos + 0.0001 * fTime),
					rand(pos + 0.0001 * fTime + 0.3),
					rand(pos + 0.0001 * fTime + 0.5)
				);
	}

	/*
	// For CRT-Geom scanlines
	if (iSCANLINE_TYPE == 3) {
		vec3 v_pwr;
		v_pwr = vec3(1.0/((-1.0*SCANLINE_WEIGHT+1.0)*(-0.8*CGWG+1.0))-1.2);
		// handle interlacing
		float s = fract(pos.y*TextureSize.y/2.001+0.5);
		if (INTERLACE_WEIGHT > 0.001)
			s = mod(float(iFrameCount),2.0) < 1.0 ? s: fract(s+0.5);

		// Vignette
		float x = 0.0;
		if (VIGNETTE_WEIGHT > 0.001) {
			x = TexCoords.x-0.5;
			x = x*x;
		}
		// Calculate CRT-Geom scanlines weight and apply
		float weight = scanlineWeights(s, res, x);
		float weight2 = scanlineWeights(1.0-s, res, x);
		res *= weight + weight2;
	}
	*/

// Masks
	vec2 xy = TexCoords*OutputSize.xy/MSIZE;	
	res *= Mask(xy, CGWG);

// Apply slot mask on top of Trinitron-like mask
	if (bSLOT)
		res *= mix(slot(xy/2.0),vec3(1.0),CGWG);

// Saturation
	float lum = dot(vec3(0.29,0.60,0.11),res);
	res = mix(vec3(lum),res,SATURATION);

// Brightness, Hue and Black Level
	res *= BRIGHTNESS;
	res *= hue;
	res -= vec3(BLACK);
	res *= blck;

	res = clamp(res,0.0,1.0);

	FragColor = vec4(res, corn);

	if (bHalveFrameRate)
		FragColor = HalveFrameRate(TexCoords, FragColor);
	
	if (GhostingPercent > 0.0001) {
		FragColor = GenerateGhosting(TexCoords, FragColor);
	}
	FragColor = gamma_encode(FragColor);
}

#endif
