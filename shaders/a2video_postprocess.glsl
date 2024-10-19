
/*

Based on 'crt-Cyclon' by DariusG

This shader uses parts from:
crt-Geom (scanlines)
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
out vec2 v_pos;

uniform COMPAT_PRECISION int FrameCount;
uniform COMPAT_PRECISION vec2 OutputSize;
uniform COMPAT_PRECISION vec2 TextureSize;
uniform COMPAT_PRECISION vec2 InputSize;

void main()
{
	gl_Position = vec4(aPos, 0.0, 1.0);
	TexCoords = TexCoord;
	scale = OutputSize.xy/TextureSize.xy;
	ps = 1.0/TextureSize.xy;
	v_pos = gl_Position.xy;
}

///////////////////////////////////////// FRAGMENT SHADER /////////////////////////////////////////
#elif defined(FRAGMENT)

uniform COMPAT_PRECISION int FrameCount;
uniform COMPAT_PRECISION vec2 OutputSize;
uniform COMPAT_PRECISION vec2 TextureSize;
uniform COMPAT_PRECISION vec2 InputSize;
uniform COMPAT_PRECISION vec2 ViewportSize;
uniform COMPAT_PRECISION vec4 VideoRect;
uniform sampler2D A2Texture;
in vec2 TexCoords;
in vec2 scale;
in vec2 ps;
in vec2 v_pos;
out vec4 FragColor;

uniform COMPAT_PRECISION int POSTPROCESSING_LEVEL;

uniform bool bCORNER_SMOOTH;
uniform bool bEXT_GAMMA;
uniform bool bINTERLACE;
uniform bool bPOTATO; 
uniform bool bSLOT;
uniform bool bVIGNETTE; 
uniform COMPAT_PRECISION float BARRELDISTORTION;
uniform COMPAT_PRECISION float BGR;
uniform COMPAT_PRECISION float BLACK; 
uniform COMPAT_PRECISION float BR_DEP; 
uniform COMPAT_PRECISION float BRIGHTNESs;
uniform COMPAT_PRECISION float C_STR;
uniform COMPAT_PRECISION float CENTERX;
uniform COMPAT_PRECISION float CENTERY;
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
uniform COMPAT_PRECISION float SLOTW;
uniform COMPAT_PRECISION float VIGNETTE_WEIGHT;
uniform COMPAT_PRECISION float WARPX;
uniform COMPAT_PRECISION float WARPY;
uniform COMPAT_PRECISION float ZOOMX;
uniform COMPAT_PRECISION float ZOOMY;
uniform COMPAT_PRECISION int iCOLOR_SPACE;
uniform COMPAT_PRECISION int iM_TYPE;
uniform COMPAT_PRECISION int iSCANLINE_TYPE;

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
	pos *= vec2(1.0+pos.y*pos.y*WARPX, 1.0+pos.x*pos.x*WARPY);
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

void main() {
	
	if (POSTPROCESSING_LEVEL == 0) {
		FragColor = texture(A2Texture,TexCoords);
		return;
	}
	
// Apply simple horizontal scanline if required and exit
	if (POSTPROCESSING_LEVEL == 1) {
		FragColor = texture(A2Texture,TexCoords);
		FragColor.rgb = FragColor.rgb * (1.0 - mod(floor(TexCoords.y * TextureSize.y), 2.0));
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
	
// zoom in and center screen for bezel
	vec2 pos = Warp(TexCoords*vec2(1.0-ZOOMX,1.0-ZOOMY)-vec2(CENTERX,CENTERY)/100.0);
	
	
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


// Convergence
	vec3 res0 = texture(A2Texture,pos).rgb;
	float resr = texture(A2Texture,pos + dx*CONV_R).r;
	float resg = texture(A2Texture,pos + dx*CONV_G).g;
	float resb = texture(A2Texture,pos + dx*CONV_B).b;

	vec3 res = vec3(res0.r*(1.0-C_STR) + resr*C_STR,
					res0.g*(1.0-C_STR) + resg*C_STR,
					res0.b*(1.0-C_STR) + resb*C_STR
					);

	float corn;
	if (CORNER > 0.000001) {
		corn = pos.x * pos.y * (1.-pos.x) * (1.-pos.y);
		if (bCORNER_SMOOTH)
			res = res * smoothstep(0.0, CORNER, corn);	// if we want it smooth
		else
			if (corn < CORNER)						// if we want it cut
				discard;
	}
	
	float l = dot(vec3(BR_DEP),res);
 
 // Color Spaces 
	if(!bEXT_GAMMA)
		res *= res;
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

	// For CRT-Geom scanlines
	vec3 v_pwr;
	if (iSCANLINE_TYPE == 2) {
		v_pwr = vec3(1.0/((-1.0*SCANLINE_WEIGHT+1.0)*(-0.8*CGWG+1.0))-1.2);
		// handle interlacing
		float s = fract(pos.y*TextureSize.y/2.001+0.5);
		if (bINTERLACE)
			s = mod(float(FrameCount),2.0) < 1.0 ? s: fract(s+0.5);
	
		// Vignette
		float x = 0.0;
		if (bVIGNETTE) {
			x = TexCoords.x-0.5;
			x = x*x;
		}
		// Calculate CRT-Geom scanlines weight and apply
		float weight = scanlineWeights(s, res, x);
		float weight2 = scanlineWeights(1.0-s, res, x);
		res *= weight + weight2;
	} else {
		v_pwr = vec3(1.0/((-1.0*0.3+1.0)*(-0.8*CGWG+1.0))-1.2);
	}

// Masks
	vec2 xy = TexCoords*OutputSize.xy*scale/MSIZE;	
	res *= Mask(xy, CGWG);

// Apply slot mask on top of Trinitron-like mask
	if (bSLOT)
		res *= mix(slot(xy/2.0),vec3(1.0),CGWG);
	if (bPOTATO) {
		res = sqrt(res);
		res *= mix(1.3,1.1,l);
	} else {
		res = inv_gamma(res,v_pwr);
	}

// Saturation
	float lum = dot(vec3(0.29,0.60,0.11),res);
	res = mix(vec3(lum),res,SATURATION);

// Brightness, Hue and Black Level
	res *= BRIGHTNESs;
	res *= hue;
	res -= vec3(BLACK);
	res *= blck;

	FragColor = vec4(res, 1.0);
}

#endif
