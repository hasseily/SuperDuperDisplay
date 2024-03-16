#ifdef GL_ES
#define COMPAT_PRECISION mediump
precision mediump float;
#else
#define COMPAT_PRECISION
#endif

in vec2 aPos;
in vec2 texCoords;
out vec2 TexCoords;

uniform mat4 MVPMatrix;

void main()
{
	TexCoords = texCoords;
    gl_Position = MVPMatrix * vec4(aPos, 0.0, 1.0);
}
