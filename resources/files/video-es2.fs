// OpenGL ES 2.0 shaders use GLSL ES 1.0
precision mediump float;

// Input from vertex shader
varying vec2 fragTexCoord;

// Input uniform values
uniform sampler2D texture0;

// Define constants
const float PI = 3.14159265359;

// Uniform to specify the number of scanlines
uniform float totalScanlines;

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
    vec3 color = texture2D(texture0, uv).rgb;

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
    gl_FragColor = vec4(vec3(gray), 1.0);
}
