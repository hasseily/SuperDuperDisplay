#ifdef GL_ES
	#define COMPAT_PRECISION mediump
	precision mediump float;
#else
	#define COMPAT_PRECISION
#endif

// Shader for text overlays

in vec2 vTexCoords;
uniform sampler2D uTex;	// font atlas
uniform vec4 uColor;	// font color
out vec4 FragColor;
void main() {
	if (vTexCoords.x < -0.001) {		// the log overlay
		FragColor = vec4(0.,0.,0.,0.7);
	} else {
		float a = texture(uTex, vTexCoords).r;
		FragColor = vec4(uColor.rgb, uColor.a * a);
	}
}
