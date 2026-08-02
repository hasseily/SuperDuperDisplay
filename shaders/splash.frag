#ifdef GL_ES
#define COMPAT_PRECISION mediump
precision mediump float;
#else
#define COMPAT_PRECISION
#endif

in vec2 vTexCoords;
out vec4 FragColor;

uniform sampler2D TEXIN;
uniform COMPAT_PRECISION float splashProgress;

void main()
{
	vec2 flippedUV = vec2(vTexCoords.x, 1.0 - vTexCoords.y);

	// Hold the clean image, then enlarge its source pixels as the splash ends.
	float effectProgress = smoothstep(0.45, 1.0, splashProgress);
	float pixelSize = mix(1.0, 96.0, effectProgress * effectProgress);
	vec2 textureDimensions = vec2(textureSize(TEXIN, 0));
	vec2 pixelatedUV =
		(floor((flippedUV * textureDimensions) / pixelSize) + 0.5) *
		pixelSize / textureDimensions;

	vec4 splashColor = texture(TEXIN, pixelatedUV);
	float brightness = 1.0 - smoothstep(0.55, 1.0, splashProgress);
	FragColor = vec4(splashColor.rgb * brightness, splashColor.a);
}
