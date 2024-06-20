#ifdef GL_ES
#define COMPAT_PRECISION mediump
precision mediump float;
precision highp usampler2D;
precision highp int;
#else
#define COMPAT_PRECISION
layout(pixel_center_integer) in vec4 gl_FragCoord;
#endif

in vec2 vFragPos;       // The fragment position in pixels
out vec4 fragColor;

void main()
{
	fragColor = vec4(1.f, 0.0f, 0.5f, 1.f);
}
