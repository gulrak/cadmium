#include "crtrenderer.hpp"

#include <rlgl.h>

#include <algorithm>
#include <string>

namespace {

#if defined(PLATFORM_WEB)
// WebGL 1: GLSL ES 1.00, fwidth() needs the standard derivatives extension.
constexpr const char* CRT_SHADER_HEADER = R"(#version 100
#extension GL_OES_standard_derivatives : enable
#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif
varying vec2 fragTexCoord;
varying vec4 fragColor;
#define TEXTURE texture2D
#define OUT_COLOR gl_FragColor
)";
#else
constexpr const char* CRT_SHADER_HEADER = R"(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;
#define TEXTURE texture
#define OUT_COLOR finalColor
)";
#endif

constexpr const char* CRT_SHADER_BODY = R"(
uniform sampler2D texture0;
uniform vec4 colDiffuse;

uniform vec2 textureSize;
uniform vec4 sourceRect;
uniform float linesPerRow;
uniform float beamHorizontalRadius;
uniform float beamVerticalRadius;
uniform float beamIntensity;
uniform float vignetteStrength;
uniform float vignetteRadius;
uniform float maskStrength;
uniform float maskPitch;

vec3 sampleTexel(float x, float y)
{
    if (x < sourceRect.x || x >= sourceRect.x + sourceRect.z || y < sourceRect.y || y >= sourceRect.y + sourceRect.w) {
        return vec3(0.0);
    }
    return (TEXTURE(texture0, vec2(x, y) / textureSize) * colDiffuse * fragColor).rgb;
}

float approxErf(float x)
{
    float s = x < 0.0 ? -1.0 : 1.0;
    float t = 1.0 / (1.0 + 0.3275911 * abs(x));
    float poly = ((((1.061405429 * t - 1.453152027) * t + 1.421413741) * t - 0.284496736) * t + 0.254829592) * t;
    return s * (1.0 - poly * exp(-x * x));
}

vec3 sampleBeamLine(float sourceX, float y, float sigmaH)
{
    float center = floor(sourceX) + 0.5;
    float d = sourceX - center;
    float inv = 1.0 / sigmaH;
    float e0 = approxErf((d + 4.5) * inv);
    float e1 = approxErf((d + 3.5) * inv);
    float e2 = approxErf((d + 2.5) * inv);
    float e3 = approxErf((d + 1.5) * inv);
    float e4 = approxErf((d + 0.5) * inv);
    float e5 = approxErf((d - 0.5) * inv);
    float e6 = approxErf((d - 1.5) * inv);
    float e7 = approxErf((d - 2.5) * inv);
    float e8 = approxErf((d - 3.5) * inv);
    float e9 = approxErf((d - 4.5) * inv);
    vec3 color = vec3(0.0);
    color += sampleTexel(center - 4.0, y) * ((e0 - e1) * 0.5);
    color += sampleTexel(center - 3.0, y) * ((e1 - e2) * 0.5);
    color += sampleTexel(center - 2.0, y) * ((e2 - e3) * 0.5);
    color += sampleTexel(center - 1.0, y) * ((e3 - e4) * 0.5);
    color += sampleTexel(center,       y) * ((e4 - e5) * 0.5);
    color += sampleTexel(center + 1.0, y) * ((e5 - e6) * 0.5);
    color += sampleTexel(center + 2.0, y) * ((e6 - e7) * 0.5);
    color += sampleTexel(center + 3.0, y) * ((e7 - e8) * 0.5);
    color += sampleTexel(center + 4.0, y) * ((e8 - e9) * 0.5);
    return color;
}

float rowOfLine(float line)
{
    return sourceRect.y + floor(line / linesPerRow) + 0.5;
}

vec3 sampleBeam(float sourceX, float lineY, float linesPerTargetPixel)
{
    float line = floor(lineY) + 0.5;
    float sigmaH = max(beamHorizontalRadius, 0.001);
    float sigmaV = max(beamVerticalRadius / linesPerTargetPixel, 0.001);

    float dPrev = abs(lineY - (line - 1.0)) / linesPerTargetPixel / sigmaV;
    float dCurr = abs(lineY - line) / linesPerTargetPixel / sigmaV;
    float dNext = abs(lineY - (line + 1.0)) / linesPerTargetPixel / sigmaV;

    vec3 current = sampleBeamLine(sourceX, rowOfLine(line), sigmaH);
    vec3 color = sampleBeamLine(sourceX, rowOfLine(line - 1.0), sigmaH) * exp(-dPrev * dPrev);
    color += current * exp(-dCurr * dCurr);
    color += sampleBeamLine(sourceX, rowOfLine(line + 1.0), sigmaH) * exp(-dNext * dNext);

    float scanLineVisibility = smoothstep(1.5, 3.0, 1.0 / linesPerTargetPixel);
    vec3 flatLine = current * (beamVerticalRadius * 1.7724539);
    return min(mix(flatLine, color, scanLineVisibility) * beamIntensity, vec3(1.0));
}

vec3 applyRgbMask(vec3 color)
{
    const float TAU = 6.28318530;
    float phase = gl_FragCoord.x / maskPitch * TAU;
    vec3 mask = vec3(
        0.5 + 0.5 * cos(phase),
        0.5 + 0.5 * cos(phase - TAU / 3.0),
        0.5 + 0.5 * cos(phase - TAU * 2.0 / 3.0)
    );
    return color * mix(vec3(1.0), mask, maskStrength);
}

void main()
{
    vec2 texel = fragTexCoord * textureSize;
    float linesPerTargetPixel = max(fwidth((texel.y - sourceRect.y) * linesPerRow), 0.0001);
    texel = clamp(texel, sourceRect.xy, sourceRect.xy + sourceRect.zw - vec2(0.0001));
    float lineY = (texel.y - sourceRect.y) * linesPerRow;

    vec3 color = sampleBeam(texel.x, lineY, linesPerTargetPixel);
    if (maskStrength > 0.0) {
        color = applyRgbMask(color);
    }

    vec2 centered = (texel - sourceRect.xy) / sourceRect.zw * 2.0 - vec2(1.0);
    float vignette = 1.0 - vignetteStrength * smoothstep(vignetteRadius, 1.0, dot(centered, centered));

    OUT_COLOR = vec4(color * vignette, 1.0);
}
)";

} // namespace

CrtRenderer::CrtRenderer(CrtParameters parameters)
    : _parameters(parameters)
{
}

CrtRenderer::~CrtRenderer()
{
    unload();
}

bool CrtRenderer::load()
{
    unload();
    const std::string source = std::string(CRT_SHADER_HEADER) + CRT_SHADER_BODY;
    _shader = LoadShaderFromMemory(nullptr, source.c_str());
    if (!isLoaded()) {
        return false;
    }
    _textureSizeLocation = GetShaderLocation(_shader, "textureSize");
    _sourceRectLocation = GetShaderLocation(_shader, "sourceRect");
    _linesPerRowLocation = GetShaderLocation(_shader, "linesPerRow");
    _beamHorizontalRadiusLocation = GetShaderLocation(_shader, "beamHorizontalRadius");
    _beamVerticalRadiusLocation = GetShaderLocation(_shader, "beamVerticalRadius");
    _beamIntensityLocation = GetShaderLocation(_shader, "beamIntensity");
    _vignetteStrengthLocation = GetShaderLocation(_shader, "vignetteStrength");
    _vignetteRadiusLocation = GetShaderLocation(_shader, "vignetteRadius");
    _maskStrengthLocation = GetShaderLocation(_shader, "maskStrength");
    _maskPitchLocation = GetShaderLocation(_shader, "maskPitch");
    applyParameters();
    return true;
}

bool CrtRenderer::isLoaded() const
{
    // raylib substitutes its default shader if the compile fails.
    return _shader.id > 0 && _shader.id != rlGetShaderIdDefault();
}

void CrtRenderer::unload()
{
    if (isLoaded()) {
        UnloadShader(_shader);
    }
    _shader = {};
}

void CrtRenderer::setParameters(const CrtParameters& parameters)
{
    _parameters = parameters;
    applyParameters();
}

void CrtRenderer::draw(const Texture2D& texture, Rectangle source, Rectangle destination, Vector2 origin, float rotation, int linesPerRow) const
{
    if (!isLoaded()) {
        DrawTexturePro(texture, source, destination, origin, rotation, WHITE);
        return;
    }
    const float textureSize[2]{static_cast<float>(texture.width), static_cast<float>(texture.height)};
    const float sourceRect[4]{source.x, source.y, source.width, source.height};
    const float lines = static_cast<float>(std::max(1, linesPerRow));
    SetShaderValue(_shader, _textureSizeLocation, textureSize, SHADER_UNIFORM_VEC2);
    SetShaderValue(_shader, _sourceRectLocation, sourceRect, SHADER_UNIFORM_VEC4);
    SetShaderValue(_shader, _linesPerRowLocation, &lines, SHADER_UNIFORM_FLOAT);
    BeginShaderMode(_shader);
    DrawTexturePro(texture, source, destination, origin, rotation, WHITE);
    EndShaderMode();
}

void CrtRenderer::applyParameters() const
{
    if (!isLoaded()) {
        return;
    }
    SetShaderValue(_shader, _beamHorizontalRadiusLocation, &_parameters.beamHorizontalRadius, SHADER_UNIFORM_FLOAT);
    SetShaderValue(_shader, _beamVerticalRadiusLocation, &_parameters.beamVerticalRadius, SHADER_UNIFORM_FLOAT);
    SetShaderValue(_shader, _beamIntensityLocation, &_parameters.beamIntensity, SHADER_UNIFORM_FLOAT);
    SetShaderValue(_shader, _vignetteStrengthLocation, &_parameters.vignetteStrength, SHADER_UNIFORM_FLOAT);
    SetShaderValue(_shader, _vignetteRadiusLocation, &_parameters.vignetteRadius, SHADER_UNIFORM_FLOAT);
    SetShaderValue(_shader, _maskStrengthLocation, &_parameters.maskStrength, SHADER_UNIFORM_FLOAT);
    SetShaderValue(_shader, _maskPitchLocation, &_parameters.maskPitch, SHADER_UNIFORM_FLOAT);
}
