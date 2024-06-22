#define PI 3.1415926535897932384626433

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
uniform sampler2D A2Texture;
in vec2 TexCoords;
in vec2 scale;
in vec2 ps;
in vec2 v_pos;
out vec4 FragColor;

uniform COMPAT_PRECISION float time;					// s since start
uniform COMPAT_PRECISION float POSTPROCESSING_LEVEL;

uniform COMPAT_PRECISION float scan_line_amount;		// 0 - 1
uniform COMPAT_PRECISION float scan_line_strength;		// -12 - -1
uniform COMPAT_PRECISION float pixel_strength;			// -4 - 0
uniform COMPAT_PRECISION float warp_amount;				// -0.20 - 5
uniform COMPAT_PRECISION float noise_amount;			// 0 - 0.3
uniform COMPAT_PRECISION float interference_amount;		// 0 - 1
uniform COMPAT_PRECISION float grille_amount;			// 0 - 1
uniform COMPAT_PRECISION float grille_size;				// 1 - 5
uniform COMPAT_PRECISION float vignette_amount;			// 0 - 2
uniform COMPAT_PRECISION float vignette_intensity;		// 0 - 1
uniform COMPAT_PRECISION float aberration_amount;		// 0 - 1
uniform COMPAT_PRECISION float roll_line_amount;		// 0 - 1
uniform COMPAT_PRECISION float roll_speed;				// -8 - 8



float random(vec2 uv){
    return fract(cos(uv.x * 83.4827 + uv.y * 92.2842) * 43758.5453123);
}

vec3 fetch_pixel(vec2 uv, vec2 off){
	vec2 pos = floor(uv * TextureSize + off) / TextureSize + vec2(0.5) / TextureSize;

	float noise = 0.0;
	if(noise_amount > 0.0){
		noise = random(pos + fract(time)) * noise_amount;
	}

	if(max(abs(pos.x - 0.5), abs(pos.y - 0.5)) > 0.5){
		return vec3(0.0, 0.0, 0.0);
	}

	vec3 clr = texture(A2Texture , pos, -16.0).rgb + noise;
	return clr;
}

// Distance in emulated pixels to nearest texel.
vec2 Dist(vec2 pos){ 
	pos = pos * TextureSize;
	return - ((pos - floor(pos)) - vec2(0.5));
}
    
// 1D Gaussian.
float Gaus(float pos, float scale){ return exp2(scale * pos * pos); }

// 3-tap Gaussian filter along horz line.
vec3 Horz3(vec2 pos, float off){
	vec3 b = fetch_pixel(pos, vec2(-1.0, off));
	vec3 c = fetch_pixel(pos, vec2( 0.0, off));
	vec3 d = fetch_pixel(pos, vec2( 1.0, off));
	float dst = Dist(pos).x;
	
	// Convert distance to weight.
	float scale = pixel_strength;
	float wb = Gaus(dst - 1.0, scale);
	float wc = Gaus(dst + 0.0, scale);
	float wd = Gaus(dst + 1.0, scale);
	
	// Return filtered sample.
	return (b * wb + c * wc + d * wd) / (wb + wc + wd);
}

// Return scanline weight.
float Scan(vec2 pos, float off){
	float dst = Dist(pos).y;
	
	return Gaus(dst + off, scan_line_strength);
}

// Allow nearest three lines to effect pixel.
vec3 Tri(vec2 pos){
	vec3 clr = fetch_pixel(pos, vec2(0.0));
	if(scan_line_amount > 0.0){
		vec3 a = Horz3(pos,-1.0);
		vec3 b = Horz3(pos, 0.0);
		vec3 c = Horz3(pos, 1.0);

		float wa = Scan(pos,-1.0);
		float wb = Scan(pos, 0.0);
		float wc = Scan(pos, 1.0);

		vec3 scanlines = a * wa + b * wb + c * wc;
		clr = mix(clr, scanlines, scan_line_amount);
	}
	return clr;
}

// Takes in the UV and warps the edges, creating the spherized effect
vec2 warp(vec2 uv){
	vec2 delta = uv - 0.5;
	float delta2 = dot(delta.xy, delta.xy);
	float delta4 = delta2 * delta2;
	float delta_offset = delta4 * warp_amount;
	
	vec2 warped = uv + delta * delta_offset;
	return (warped - 0.5) / mix(1.0,1.2,warp_amount/5.0) + 0.5;
}

float vignette(vec2 uv){
	uv *= 1.0 - uv.xy;
	float vignette = uv.x * uv.y * 15.0;
	return pow(vignette, vignette_intensity * vignette_amount);
}

float floating_mod(float a, float b){
	return a - b * floor(a/b);
}

vec3 grille(vec2 uv){
	float unit = PI / 3.0;
	float scale = 2.0*unit/grille_size;
	float r = smoothstep(0.5, 0.8, cos(uv.x*scale - unit));
	float g = smoothstep(0.5, 0.8, cos(uv.x*scale + unit));
	float b = smoothstep(0.5, 0.8, cos(uv.x*scale + 3.0*unit));
	return mix(vec3(1.0), vec3(r,g,b), grille_amount);
}

float roll_line(vec2 uv){
	float x = uv.y * 3.0 - time * roll_speed;
	float f = cos(x) * cos(x * 2.35 + 1.1) * cos(x * 4.45 + 2.3);
	float roll_line = smoothstep(0.5, 0.9, f);
	return roll_line * roll_line_amount;
}


void main() {
	
	if (POSTPROCESSING_LEVEL == 0.0) {
		FragColor = texture(A2Texture,TexCoords);
		return;
	}
	
// Apply simple horizontal scanline if required and exit
	if (POSTPROCESSING_LEVEL == 1.0) {
		FragColor = texture(A2Texture,TexCoords);
		FragColor.rgb = FragColor.rgb * (1.0 - mod(floor(TexCoords.y * TextureSize.y), 2.0));
		return;
	}

	vec2 pix = v_pos.xy;
	vec2 pos = warp(TexCoords);
	
	float line = 0.0;
	if(roll_line_amount > 0.0){
		line = roll_line(pos);
	}

	vec2 sq_pix = floor(pos * TextureSize) / TextureSize + vec2(0.5) / TextureSize;
	if(interference_amount + roll_line_amount > 0.0){
		float interference = random(sq_pix.yy + fract(time));
		pos.x += (interference * (interference_amount + line * 6.0)) / TextureSize.x;
	}

	vec3 clr = Tri(pos);
	if(aberration_amount > 0.0){
		float chromatic = aberration_amount + line * 2.0;
		vec2 chromatic_x = vec2(chromatic,0.0) / TextureSize.x;
		vec2 chromatic_y = vec2(0.0, chromatic/2.0) / TextureSize.y;
		float r = Tri(pos - chromatic_x).r;
		float g = Tri(pos + chromatic_y).g;
		float b = Tri(pos + chromatic_x).b;
		clr = vec3(r,g,b);
	}
	
	if(grille_amount > 0.0)clr *= grille(pix);
	clr *= 1.0 + scan_line_amount * 0.6 + line * 3.0 + grille_amount * 2.0;
	if(vignette_amount > 0.0)clr *= vignette(pos);
	
	FragColor.rgb = clr;
	FragColor.a = 1.0;
}

#endif
