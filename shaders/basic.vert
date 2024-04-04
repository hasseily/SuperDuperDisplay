#ifdef GL_ES
#define COMPAT_PRECISION mediump
precision mediump float;
#else
#define COMPAT_PRECISION
#endif

in vec2 aPos;
in vec2 texCoords;
out vec2 vTexCoords;

void main()
{
	vTexCoords = texCoords;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
