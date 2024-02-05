#ifdef GL_ES
#define COMPAT_PRECISION mediump
precision mediump float;
#else
#define COMPAT_PRECISION
#endif

in vec2 TexCoords;
out vec4 fragColor;
uniform sampler2D Texture;

void main()
{
	fragColor = texture(Texture, TexCoords);
	fragColor = fragColor * vec4(0.8, 1.2, 1.0, 1.0);
}
