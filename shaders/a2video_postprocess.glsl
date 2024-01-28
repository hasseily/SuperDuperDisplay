
/*

Based on 'crt-Cyclon' by DariusG

This shader uses parts from:
crt-Geom (scanlines)
Quillez (main filter)
Dogway's inverse Gamma
NewPixie Bezel Code only

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

in vec2 VertexCoord;
in vec2 TexCoord;
//in vec4 COLOR;
out vec2 vTexCoord; // Pass texture coordinates to fragment shader
out vec2 scale;
out vec2 ps;

uniform COMPAT_PRECISION int FrameCount;
uniform COMPAT_PRECISION vec2 OutputSize;
uniform COMPAT_PRECISION vec2 TextureSize;
uniform COMPAT_PRECISION vec2 InputSize;

#ifdef PARAMETER_UNIFORM
	uniform COMPAT_PRECISION float one;
#else
	#define one 0.0
#endif

void main()
{
	gl_Position = vec4(VertexCoord, 0.0, 1.0);
	vTexCoord = TexCoord;
	scale = TextureSize.xy/InputSize.xy;
	ps = 1.0/TextureSize.xy; 
}

///////////////////////////////////////// FRAGMENT SHADER /////////////////////////////////////////
#elif defined(FRAGMENT)

uniform COMPAT_PRECISION int FrameCount;
uniform COMPAT_PRECISION vec2 OutputSize;
uniform COMPAT_PRECISION vec2 TextureSize;
uniform COMPAT_PRECISION vec2 InputSize;
uniform sampler2D Texture;
uniform sampler2D BezelTexture;
uniform sampler2D HorizScanlineTexture;
in vec2 vTexCoord;
in vec2 scale;
in vec2 ps;
out vec4 FragColor;

#define SourceSize vec4(TextureSize, 1.0 / TextureSize) //either TextureSize or InputSize

uniform COMPAT_PRECISION float M_TYPE;
uniform COMPAT_PRECISION float BGR;
uniform COMPAT_PRECISION float Maskl;
uniform COMPAT_PRECISION float Maskh;
uniform COMPAT_PRECISION float MSIZE;
uniform COMPAT_PRECISION float P_GLOW;
uniform COMPAT_PRECISION float C_STR;
uniform COMPAT_PRECISION float CONV_R;
uniform COMPAT_PRECISION float CONV_G;
uniform COMPAT_PRECISION float CONV_B;
uniform COMPAT_PRECISION float SCANLINEWEIGHT;
uniform COMPAT_PRECISION float INTERLACE;
uniform COMPAT_PRECISION float WARPX;
uniform COMPAT_PRECISION float WARPY;
uniform COMPAT_PRECISION float SLOT;
uniform COMPAT_PRECISION float SLOTW;
uniform COMPAT_PRECISION float c_space;
uniform COMPAT_PRECISION float SATURATION;
uniform COMPAT_PRECISION float BRIGHTNESs;
uniform COMPAT_PRECISION float RG;
uniform COMPAT_PRECISION float RB;
uniform COMPAT_PRECISION float GB;
uniform COMPAT_PRECISION float BLACK; 
uniform COMPAT_PRECISION float BR_DEP; 
uniform COMPAT_PRECISION float POTATO; 
uniform COMPAT_PRECISION float EXT_GAMMA; 
uniform COMPAT_PRECISION float vig; 
uniform COMPAT_PRECISION float zoomx;
uniform COMPAT_PRECISION float zoomy;
uniform COMPAT_PRECISION float centerx;
uniform COMPAT_PRECISION float centery;
uniform COMPAT_PRECISION float bzl;
uniform COMPAT_PRECISION float corner;

vec3 Mask(vec2 pos, float CGWG) {
	if (M_TYPE == 0.0)
		return vec3(1.0);
	vec3 mask = vec3(CGWG);
	if (M_TYPE == 1.0){
		if (POTATO == 1.0) {
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

	if (M_TYPE == 2.0) {
		if (POTATO == 1.0) {
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
	float wid = SCANLINEWEIGHT + 0.15 * dot(color, vec3(0.25-0.8*x)); //0.8 vignette strength
	float weights = distance / wid;
	return 0.4 * exp(-weights * weights * weights) / wid;
}

#define pwr vec3(1.0/((-1.0*SCANLINEWEIGHT+1.0)*(-0.8*CGWG+1.0))-1.2)
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

void main() {
// Hue matrix inside main() to avoid GLES error
	mat3 hue = mat3 (
		1.0, -RG, -RB,
		RG, 1.0, -GB,
		RB, GB, 1.0
		);
// zoom in and center screen for bezel
	vec2 pos = Warp((vTexCoord*vec2(1.0-zoomx,1.0-zoomy)-vec2(centerx,centery)/100.0)*scale);
	vec2 corn;
	
	if (corner == 1.0){
		corn = min(pos, 1.0-pos);	// This is used to mask the rounded
		corn.x = 0.00015/corn.x;	 // corners later on
	}	 
	pos /= scale;

	vec4 bez = texture(BezelTexture,vTexCoord*SourceSize.xy/InputSize.xy*0.95+vec2(0.022,0.022));	
	bez.rgb = mix(bez.rgb, vec3(0.25),0.4);
	vec2 bpos = pos;
	vec2 dx = vec2(ps.x,0.0);
	
// Quillez
	vec2 ogl2 = pos*SourceSize.xy;
	vec2 i = floor(pos*SourceSize.xy) + 0.5;
	float f = ogl2.y - i.y;
	pos.y = (i.y + 4.0*f*f*f)*ps.y; // smooth
	pos.x = mix(pos.x, i.x*ps.x, 0.2);

// Convergence
	vec3 res0 = texture(Texture,pos).rgb;
	float resr = texture(Texture,pos + dx*CONV_R).r;
	float resg = texture(Texture,pos + dx*CONV_G).g;
	float resb = texture(Texture,pos + dx*CONV_B).b;

	vec3 res = vec3(res0.r*(1.0-C_STR) + resr*C_STR,
					res0.g*(1.0-C_STR) + resg*C_STR,
					res0.b*(1.0-C_STR) + resb*C_STR
					);
// Vignette
	float x = 0.0;
	if (vig == 1.0) {
		x = vTexCoord.x*scale.x-0.5;
		x = x*x;
	}

	float l = dot(vec3(BR_DEP),res);
 
 // Color Spaces 
	if(EXT_GAMMA != 1.0)
		res *= res;
	if (c_space != 0.0) {
		if (c_space == 1.0)
			res *= PAL;
		if (c_space == 2.0)
			res *= NTSC;
		if (c_space == 3.0)
			res *= NTSC_J;
		// Apply CRT-like luminances
		res /= vec3(0.24,0.69,0.07);
		res *= vec3(0.29,0.6,0.11); 
		res = clamp(res,0.0,1.0);
	}
	
// handle interlacing
	float s = fract(bpos.y*SourceSize.y/2.001+0.5);
	if (INTERLACE == 1.0)
		s = mod(float(FrameCount),2.0) < 1.0 ? s: s+0.5;
	
// Calculate CRT-Geom scanlines weight and apply
	float weight = scanlineWeights(s, res, x);
	float weight2 = scanlineWeights(1.0-s, res, x);
	res *= weight + weight2;

// Masks
	vec2 xy = vTexCoord*OutputSize.xy*scale/MSIZE;	
	float CGWG = mix(Maskl, Maskh, l);
	res *= Mask(xy, CGWG);

// Apply slot mask on top of Trinitron-like mask
	if (SLOT == 1.0)
		res *= mix(slot(xy/2.0),vec3(1.0),CGWG);
	if (POTATO == 0.0) {
		res = inv_gamma(res,pwr);
	} else {
		res = sqrt(res);
		res *= mix(1.3,1.1,l);
	}

// Saturation
	float lum = dot(vec3(0.29,0.60,0.11),res);
	res = mix(vec3(lum),res,SATURATION);

// Brightness, Hue and Black Level
	res *= BRIGHTNESs;
	res *= hue;
	res -= vec3(BLACK);
	res *= blck;

// Apply bezel code, adapted from New-Pixie
	if (bzl >0.0)
		res.rgb = mix(	
						res.rgb,
						mix(
							max(res.rgb, 0.0),
							pow( abs(bez.rgb), vec3( 1.4 ) ),
							bez.w * bez.w
							),
						vec3( 1.0 )
						);
	if (corner == 1.0) 
		if (corn.y <= corn.x || corn.x < 0.0001 )
			res = vec3(0.0);

// Apply final horizontal scanline
	
	FragColor = vec4(res, 1.0);
	// FragColor = vec4(res * mod(floor(gl_FragCoord.y), 2.0), 1.0);

}

#endif