

#ifdef GL_ES
#define COMPAT_PRECISION mediump
precision mediump float;
#else
#define COMPAT_PRECISION
#endif

#if defined(VERTEX)

in vec2 VertexCoord;
in vec2 TexCoord;    // Texture coordinates input
out vec2 vTexCoord;  // Pass texture coordinates to fragment shader

uniform COMPAT_PRECISION int FrameDirection;
uniform COMPAT_PRECISION int FrameCount;
uniform COMPAT_PRECISION vec2 OutputSize;
uniform COMPAT_PRECISION vec2 TextureSize;
uniform COMPAT_PRECISION vec2 InputSize;

void main()
{
    gl_Position = vec4(VertexCoord, 0.0, 1.0);
    vTexCoord = TexCoord;
}

#elif defined(FRAGMENT)

#define PI 3.1415926538

// Samplers
in vec2 vTexCoord;          // Received from vertex shader
uniform sampler2D Texture;  // The texture sampler
out vec4 fragColor;         // Output fragment color

// Parameters
uniform vec2 curvature;
uniform COMPAT_PRECISION vec2 InputSize;
uniform COMPAT_PRECISION vec2 OutputSize;
uniform float vignetteOpacity;
uniform float brightness;
uniform float vignetteRoundness;


vec2 curveRemapUV(vec2 uv)
{
    // as we near the edge of our screen apply greater distortion using a sinusoid.

    uv = uv * 2.0 - 1.0;
    vec2 offset = abs(uv.yx) * vec2(curvature.x, curvature.y);
    uv = uv + uv * offset * offset;
    uv = uv * 0.5 + 0.5;
    return uv;
}

vec4 vignetteIntensity(vec2 uv, vec2 resolution, float opacity, float roundness)
{
    float intensity = uv.x * uv.y * (1.0 - uv.x) * (1.0 - uv.y);
    return vec4(vec3(clamp(pow((resolution.x / roundness) * intensity, opacity), 0.0, 1.0)), 1.0);
}

void main(void)
{
    vec2 remappedUV = curveRemapUV(vTexCoord);
    vec4 baseColor = texture(Texture, remappedUV);

    baseColor *= vignetteIntensity(remappedUV, OutputSize, vignetteOpacity, vignetteRoundness);

    baseColor *= vec4(vec3(brightness), 1.0);

    if (remappedUV.x < 0.0 || remappedUV.y < 0.0 || remappedUV.x > 1.0 || remappedUV.y > 1.0){
        fragColor = vec4(0.0, 0.0, 0.0, 1.0);
    } else {
        fragColor = baseColor;
    }
//    fragColor = vec4(remappedUV, 0.0, 1.0); // Visualize UV as colors

}
#endif