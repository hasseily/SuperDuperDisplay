#ifdef GL_ES
	#define COMPAT_PRECISION mediump
	precision mediump float;
#else
	#define COMPAT_PRECISION
#endif

in vec2 vTexCoords;
out vec4 fragColor;
uniform sampler2D Texture;
uniform COMPAT_PRECISION float POSTPROCESSING_LEVEL;
uniform COMPAT_PRECISION vec2 TextureSize;

void main()
{
	
	fragColor = texture(Texture, vTexCoords);

	// Apply simple horizontal scanline if required
	if (POSTPROCESSING_LEVEL == 1.0) {
		ivec2 texSize = textureSize(Texture, 0);
		fragColor.rgb = fragColor.rgb * (1.0 - mod(floor(vTexCoords.y * float(TextureSize.y)), 2.0));
		return;
	}
}
