#ifdef GL_ES
	#define COMPAT_PRECISION mediump
	precision mediump float;
#else
	#define COMPAT_PRECISION
#endif

in vec2 vTexCoords;
out vec4 fragColor;
uniform sampler2D A2TextureCurrent;
uniform COMPAT_PRECISION int POSTPROCESSING_LEVEL;
uniform COMPAT_PRECISION float GhostingPercent;
uniform COMPAT_PRECISION vec2 TextureSize;

void main()
{
	fragColor = texture(A2TextureCurrent, vTexCoords);

	// Apply simple horizontal scanline if required
	if (POSTPROCESSING_LEVEL == 1) {
		ivec2 texSize = textureSize(A2TextureCurrent, 0);
		fragColor.rgb = fragColor.rgb * (1.0 - mod(floor(vTexCoords.y * float(TextureSize.y)), 2.0));
		return;
	}
}
