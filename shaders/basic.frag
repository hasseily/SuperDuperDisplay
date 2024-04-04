#ifdef GL_ES
#define COMPAT_PRECISION mediump
precision mediump float;
#else
#define COMPAT_PRECISION
#endif

in vec2 vTexCoords;
out vec4 fragColor;
uniform sampler2D Texture;

void main()
{
	fragColor = texture(Texture, vTexCoords);
}
