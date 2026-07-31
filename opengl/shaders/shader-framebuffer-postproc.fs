#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D screenTexture;

const float kernelSharpen[9] = float[](
    2.0, 2.0, 2.0,
    2.0, -15.0, 2.0,
    2.0, 2.0, 2.0
);

const float kernelBlur[9] = float[](
    1.0 / 16, 2.0 / 16, 1.0 / 16,
    2.0 / 16, 4.0 / 16, 2.0 / 16,
    1.0 / 16, 2.0 / 16, 1.0 / 16
);

const float kernelEdge[9] = float[](
    1.0, 1.0, 1.0,
    1.0, -8.0, 1.0,
    1.0, 1.0, 1.0
);

// kernel[0] is the top-left tap, kernel[8] the bottom-right
vec3 applyKernel(sampler2D tex, vec2 uv, float kernel[9])
{
    vec2 texel = 1.0 / vec2(textureSize(tex, 0));
    vec3 col = vec3(0.0);
    for (int i = 0; i < 9; i++) {
        vec2 offset = vec2(i % 3 - 1, 1 - i / 3) * texel;
        col += texture(tex, uv + offset).rgb * kernel[i];
    }
    return col;
}

void main()
{
    // normal
    // FragColor = vec4(texture(screenTexture, TexCoords).rgb, 1.0);

    // invert
    // FragColor = vec4(vec3(1.0 - texture(screenTexture, TexCoords)), 1.0);

    // grayscale
    // vec4 color = texture(screenTexture, TexCoords);
    // float average = 0.2126 * color.r + 0.7152 * color.g + 0.0722 * color.b;
    // FragColor = vec4(average, average, average, 1.0);

    // sharpen
    // FragColor = vec4(applyKernel(screenTexture, TexCoords, kernelSharpen), 1.0);

    // blur
    // FragColor = vec4(applyKernel(screenTexture, TexCoords, kernelBlur), 1.0);

    // edge
    FragColor = vec4(applyKernel(screenTexture, TexCoords, kernelEdge), 1.0);
}
