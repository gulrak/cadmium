#version 330

// Input fragment attributes (from vertex shader)
in vec2 fragTexCoord;

// Input uniform values
uniform sampler2D texture0;

// Uniform to specify the number of scanlines
uniform float totalScanlines;

// Output fragment color
out vec4 fragColor;

void main()
{
    vec2 uv = fragTexCoord;

    // Apply barrel distortion
    vec2 center = vec2(0.5, 0.5);
    vec2 toCenter = uv - center;
    float dist = length(toCenter);
    float power = 0.1; // Adjust this value for more or less distortion
    uv = center + toCenter * (1.0 + power * dist * dist);

    // If UV coordinates are out of bounds, discard the fragment
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0)
    {
        discard;
    }

    // Sample the texture at the distorted UV coordinates
    vec3 color = texture(texture0, uv).rgb;

    // Convert the color to grayscale
    float gray = dot(color, vec3(0.299, 0.587, 0.114));

    // Calculate the scanline effect to match the number of scanlines
    float scanlineFreq = totalScanlines * 3.14159265359; // totalScanlines * PI
    float scanline = sin(uv.y * scanlineFreq) * 0.05; // Adjust intensity as needed
    gray *= 1.0 - scanline;

    // Apply vignetting effect
    float vignette = smoothstep(0.8, 0.5, dist);
    gray *= vignette;

    // Set the final fragment color
    fragColor = vec4(vec3(gray), 1.0);
}
