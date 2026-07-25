#ifdef GL_ES
#define COMPAT_PRECISION mediump
precision mediump float;
#else
#define COMPAT_PRECISION
#endif

in vec2 vTexCoords;
out vec4 FragColor;

uniform sampler2D TEXIN;

void main()
{
	FragColor = texture(TEXIN, vec2(vTexCoords.x, 1.0 - vTexCoords.y));
}
