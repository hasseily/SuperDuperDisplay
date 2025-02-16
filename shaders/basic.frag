#ifdef GL_ES
	#define COMPAT_PRECISION mediump
	precision mediump float;
#else
	#define COMPAT_PRECISION
#endif

in vec2 vTexCoords;
out vec4 fragColor;
uniform sampler2D A2TextureCurrent;
uniform sampler2D PreviousFrame;
uniform COMPAT_PRECISION int POSTPROCESSING_LEVEL;
uniform COMPAT_PRECISION float GhostingPercent;
uniform COMPAT_PRECISION vec2 TextureSize;

vec4 GenerateGhosting(vec2 coords, vec4 currentColor)
{
	vec4 previousColor = texture(PreviousFrame, coords);
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
			blended = mix(currentColor, previousColor, 0.96);
		else
			blended = mix(currentColor, previousColor, GhostingPercent/100.0);
	}
	return blended;
}

void main()
{
	if (GhostingPercent > 0.001)
	{
		fragColor = GenerateGhosting(vTexCoords, texture(A2TextureCurrent, vTexCoords));
	} else {
		fragColor = texture(A2TextureCurrent, vTexCoords);
	}

	// Apply simple horizontal scanline if required
	if (POSTPROCESSING_LEVEL == 1) {
		ivec2 texSize = textureSize(A2TextureCurrent, 0);
		fragColor.rgb = fragColor.rgb * (1.0 - mod(floor(vTexCoords.y * float(TextureSize.y)), 2.0));
		return;
	}
}
