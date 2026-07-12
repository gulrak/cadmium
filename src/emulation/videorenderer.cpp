#include "videorenderer.hpp"

#include <cmath>

namespace emu {
namespace {

constexpr const char* CRT_FRAGMENT_SHADER = R"(
#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

out vec4 finalColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

uniform vec2 sourceSize;
uniform float beamHorizontalRadius;
uniform float beamVerticalRadius;
uniform float beamIntensity;
uniform float vignetteStrength;
uniform float vignetteRadius;

vec3 sampleTexel(float sourceX, float sourceY)
{
    vec2 texelCoord = vec2(clamp(sourceX, 0.5, sourceSize.x - 0.5), clamp(sourceY, 0.5, sourceSize.y - 0.5)) / sourceSize;
    return (texture(texture0, texelCoord) * colDiffuse * fragColor).rgb;
}

vec3 sampleSignal(float sourcePosition, float sourceY)
{
    float sourcePixel = floor(sourcePosition);
    float sourceCenter = sourcePixel + 0.5;
    return sampleTexel(sourceCenter, sourceY).rgb;
}

vec3 sampleBeamLine(float sourcePosition, float sourceY, float sourcePixelsPerTargetPixel, float targetPixelsPerSourceLine)
{
    vec3 sharpSignal = sampleSignal(sourcePosition, sourceY);
    vec3 spreadSignal = sharpSignal;
    float totalWeight = 1.0;

    float radius = max(beamHorizontalRadius * targetPixelsPerSourceLine, 0.001);
    float weight1 = exp(-pow(1.0 / radius, 2.0));
    float weight2 = exp(-pow(2.0 / radius, 2.0));
    float weight3 = exp(-pow(3.0 / radius, 2.0));
    float weight4 = exp(-pow(4.0 / radius, 2.0));
    float weight5 = exp(-pow(5.0 / radius, 2.0));
    float weight6 = exp(-pow(6.0 / radius, 2.0));
    float weight7 = exp(-pow(7.0 / radius, 2.0));
    float weight8 = exp(-pow(8.0 / radius, 2.0));

    spreadSignal += sampleSignal(sourcePosition - sourcePixelsPerTargetPixel, sourceY) * weight1;
    spreadSignal += sampleSignal(sourcePosition + sourcePixelsPerTargetPixel, sourceY) * weight1;
    totalWeight += weight1 * 2.0;
    spreadSignal += sampleSignal(sourcePosition - sourcePixelsPerTargetPixel * 2.0, sourceY) * weight2;
    spreadSignal += sampleSignal(sourcePosition + sourcePixelsPerTargetPixel * 2.0, sourceY) * weight2;
    totalWeight += weight2 * 2.0;
    spreadSignal += sampleSignal(sourcePosition - sourcePixelsPerTargetPixel * 3.0, sourceY) * weight3;
    spreadSignal += sampleSignal(sourcePosition + sourcePixelsPerTargetPixel * 3.0, sourceY) * weight3;
    totalWeight += weight3 * 2.0;
    spreadSignal += sampleSignal(sourcePosition - sourcePixelsPerTargetPixel * 4.0, sourceY) * weight4;
    spreadSignal += sampleSignal(sourcePosition + sourcePixelsPerTargetPixel * 4.0, sourceY) * weight4;
    totalWeight += weight4 * 2.0;
    spreadSignal += sampleSignal(sourcePosition - sourcePixelsPerTargetPixel * 5.0, sourceY) * weight5;
    spreadSignal += sampleSignal(sourcePosition + sourcePixelsPerTargetPixel * 5.0, sourceY) * weight5;
    totalWeight += weight5 * 2.0;
    spreadSignal += sampleSignal(sourcePosition - sourcePixelsPerTargetPixel * 6.0, sourceY) * weight6;
    spreadSignal += sampleSignal(sourcePosition + sourcePixelsPerTargetPixel * 6.0, sourceY) * weight6;
    totalWeight += weight6 * 2.0;
    spreadSignal += sampleSignal(sourcePosition - sourcePixelsPerTargetPixel * 7.0, sourceY) * weight7;
    spreadSignal += sampleSignal(sourcePosition + sourcePixelsPerTargetPixel * 7.0, sourceY) * weight7;
    totalWeight += weight7 * 2.0;
    spreadSignal += sampleSignal(sourcePosition - sourcePixelsPerTargetPixel * 8.0, sourceY) * weight8;
    spreadSignal += sampleSignal(sourcePosition + sourcePixelsPerTargetPixel * 8.0, sourceY) * weight8;
    totalWeight += weight8 * 2.0;

    spreadSignal /= totalWeight;
    return spreadSignal;
}

vec3 sampleBeam(vec2 sourceUv)
{
    float sourceX = sourceUv.x * sourceSize.x;
    float sourceY = sourceUv.y * sourceSize.y;
    float sourcePixelsPerTargetPixelX = max(fwidth(sourceX), 0.0001);
    float sourcePixelsPerTargetPixelY = max(fwidth(sourceY), 0.0001);
    float targetPixelsPerSourceLine = 1.0 / sourcePixelsPerTargetPixelY;
    float sourceLine = floor(sourceY) + 0.5;
    float radius = max(beamVerticalRadius * targetPixelsPerSourceLine, 0.001);

    float previousDistance = abs(sourceY - (sourceLine - 1.0)) / sourcePixelsPerTargetPixelY;
    float currentDistance = abs(sourceY - sourceLine) / sourcePixelsPerTargetPixelY;
    float nextDistance = abs(sourceY - (sourceLine + 1.0)) / sourcePixelsPerTargetPixelY;
    float previousWeight = exp(-pow(previousDistance / radius, 2.0));
    float currentWeight = exp(-pow(currentDistance / radius, 2.0));
    float nextWeight = exp(-pow(nextDistance / radius, 2.0));

    vec3 color = sampleBeamLine(sourceX, sourceLine - 1.0, sourcePixelsPerTargetPixelX, targetPixelsPerSourceLine) * previousWeight;
    color += sampleBeamLine(sourceX, sourceLine, sourcePixelsPerTargetPixelX, targetPixelsPerSourceLine) * currentWeight;
    color += sampleBeamLine(sourceX, sourceLine + 1.0, sourcePixelsPerTargetPixelX, targetPixelsPerSourceLine) * nextWeight;
    return min(color * beamIntensity, vec3(1.0));
}

void main()
{
    vec2 sourceUv = clamp(fragTexCoord, vec2(0.0), vec2(0.999999));
    vec3 color = sampleBeam(sourceUv);

    vec2 centered = fragTexCoord * 2.0 - vec2(1.0);
    float vignette = 1.0 - vignetteStrength * smoothstep(vignetteRadius, 1.0, dot(centered, centered));

    finalColor = vec4(color * vignette, 1.0);
}
)";

constexpr const char* AFTERGLOW_FRAGMENT_SHADER = R"(
#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

out vec4 finalColor;

uniform sampler2D texture0;
uniform sampler2D previousTexture;
uniform float decay;

void main()
{
    vec4 current = texture(texture0, fragTexCoord) * fragColor;
    vec4 previous = texture(previousTexture, vec2(fragTexCoord.x, 1.0 - fragTexCoord.y));
    finalColor = vec4(max(current.rgb, previous.rgb * decay), current.a);
}
)";

void setShaderFloat(const Shader& shader, int location, const float& value)
{
    if (shader.id != 0 && location >= 0) {
        SetShaderValue(shader, location, &value, SHADER_UNIFORM_FLOAT);
    }
}

void setShaderVec2(const Shader& shader, int location, const float* value)
{
    if (shader.id != 0 && location >= 0) {
        SetShaderValue(shader, location, value, SHADER_UNIFORM_VEC2);
    }
}

void setShaderTexture(const Shader& shader, int location, const Texture2D& texture)
{
    if (shader.id != 0 && location >= 0) {
        SetShaderValueTexture(shader, location, texture);
    }
}

} // namespace

VideoRenderer::VideoRenderer(VideoRenderParameters parameters)
    : _parameters(parameters)
{
    if (_parameters.crtEnabled) {
        reloadShader();
    }
}

VideoRenderer::~VideoRenderer()
{
    unload();
}

void VideoRenderer::setParameters(const VideoRenderParameters& parameters)
{
    const bool shaderWasEnabled = _parameters.crtEnabled;
    const bool afterglowWasEnabled = _parameters.afterglowEnabled;
    _parameters = parameters;

    if (!_parameters.crtEnabled) {
        unloadShader();
    }
    else if (!shaderWasEnabled || _crtShader.shader.id == 0) {
        reloadShader();
    }
    else {
        applyShaderParameters();
    }

    if (!_parameters.afterglowEnabled) {
        unloadAfterglowTextures();
        unloadAfterglowShader();
    }
    else if (!afterglowWasEnabled || _afterglowShader.shader.id == 0) {
        ensureAfterglowResources();
    }
}

void VideoRenderer::setCrtEnabled(bool enabled)
{
    VideoRenderParameters parameters = _parameters;
    parameters.crtEnabled = enabled;
    setParameters(parameters);
}

void VideoRenderer::setAfterglowEnabled(bool enabled)
{
    VideoRenderParameters parameters = _parameters;
    parameters.afterglowEnabled = enabled;
    setParameters(parameters);
}

bool VideoRenderer::hasFrame() const
{
    return _hasFrame && _frameTexture.id != 0;
}

void VideoRenderer::draw(Rectangle destination)
{
    if (!hasFrame()) {
        return;
    }

    const Texture2D& texture = displayTexture();
    const Rectangle source = displaySourceRectangle();
    if (_parameters.crtEnabled && _crtShader.shader.id != 0) {
        const float sourceSize[2]{
            static_cast<float>(texture.width),
            static_cast<float>(texture.height),
        };
        setShaderVec2(_crtShader.shader, _crtShader.sourceSizeLocation, sourceSize);
        BeginShaderMode(_crtShader.shader);
        DrawTexturePro(texture, source, destination, Vector2{0, 0}, 0, WHITE);
        EndShaderMode();
    }
    else {
        DrawTexturePro(texture, source, destination, Vector2{0, 0}, 0, WHITE);
    }
}

void VideoRenderer::reloadShader()
{
    if (_parameters.crtEnabled) {
        unloadShader();
        _crtShader.shader = LoadShaderFromMemory(nullptr, CRT_FRAGMENT_SHADER);
        _crtShader.sourceSizeLocation = GetShaderLocation(_crtShader.shader, "sourceSize");
        _crtShader.beamHorizontalRadiusLocation = GetShaderLocation(_crtShader.shader, "beamHorizontalRadius");
        _crtShader.beamVerticalRadiusLocation = GetShaderLocation(_crtShader.shader, "beamVerticalRadius");
        _crtShader.beamIntensityLocation = GetShaderLocation(_crtShader.shader, "beamIntensity");
        _crtShader.vignetteStrengthLocation = GetShaderLocation(_crtShader.shader, "vignetteStrength");
        _crtShader.vignetteRadiusLocation = GetShaderLocation(_crtShader.shader, "vignetteRadius");
        applyShaderParameters();
    }

    if (_parameters.afterglowEnabled) {
        unloadAfterglowShader();
        ensureAfterglowResources();
    }
}

void VideoRenderer::unload()
{
    unloadTexture();
    unloadAfterglowTextures();
    unloadShader();
    unloadAfterglowShader();
}

void VideoRenderer::ensureTextureSize(std::size_t width, std::size_t height)
{
    if (_frameTexture.id != 0 && _frameTexture.width == static_cast<int>(width) && _frameTexture.height == static_cast<int>(height)) {
        return;
    }

    unloadTexture();
    unloadAfterglowTextures();
    Image image = GenImageColor(static_cast<int>(width), static_cast<int>(height), BLACK);
    _frameTexture = LoadTextureFromImage(image);
    SetTextureFilter(_frameTexture, TEXTURE_FILTER_POINT);
    UnloadImage(image);
}

void VideoRenderer::ensureAfterglowResources()
{
    if (!_parameters.afterglowEnabled || _frameTexture.id == 0) {
        return;
    }

    if (_afterglowShader.shader.id == 0) {
        _afterglowShader.shader = LoadShaderFromMemory(nullptr, AFTERGLOW_FRAGMENT_SHADER);
        _afterglowShader.previousTextureLocation = GetShaderLocation(_afterglowShader.shader, "previousTexture");
        _afterglowShader.decayLocation = GetShaderLocation(_afterglowShader.shader, "decay");
    }

    const bool texturesReady = _afterglowTextures[0].id != 0
        && _afterglowTextures[1].id != 0
        && _afterglowTextures[0].texture.width == _frameTexture.width
        && _afterglowTextures[0].texture.height == _frameTexture.height
        && _afterglowTextures[1].texture.width == _frameTexture.width
        && _afterglowTextures[1].texture.height == _frameTexture.height;
    if (texturesReady) {
        return;
    }

    unloadAfterglowTextures();
    _afterglowTextures[0] = LoadRenderTexture(_frameTexture.width, _frameTexture.height);
    _afterglowTextures[1] = LoadRenderTexture(_frameTexture.width, _frameTexture.height);
    SetTextureFilter(_afterglowTextures[0].texture, TEXTURE_FILTER_POINT);
    SetTextureFilter(_afterglowTextures[1].texture, TEXTURE_FILTER_POINT);
    for (RenderTexture2D& texture : _afterglowTextures) {
        BeginTextureMode(texture);
        ClearBackground(BLACK);
        EndTextureMode();
    }
    _afterglowWriteIndex = 0;
    _afterglowHasHistory = false;
}

void VideoRenderer::applyShaderParameters()
{
    setShaderFloat(_crtShader.shader, _crtShader.beamHorizontalRadiusLocation, _parameters.beamHorizontalRadius);
    setShaderFloat(_crtShader.shader, _crtShader.beamVerticalRadiusLocation, _parameters.beamVerticalRadius);
    setShaderFloat(_crtShader.shader, _crtShader.beamIntensityLocation, _parameters.beamIntensity);
    setShaderFloat(_crtShader.shader, _crtShader.vignetteStrengthLocation, _parameters.vignetteStrength);
    setShaderFloat(_crtShader.shader, _crtShader.vignetteRadiusLocation, _parameters.vignetteRadius);
}

void VideoRenderer::updateAfterglow(float deltaTime)
{
    if (!_parameters.afterglowEnabled || _frameTexture.id == 0) {
        return;
    }

    ensureAfterglowResources();
    if (_afterglowShader.shader.id == 0 || _afterglowTextures[0].id == 0 || _afterglowTextures[1].id == 0) {
        return;
    }

    const int previousIndex = 1 - _afterglowWriteIndex;
    RenderTexture2D& target = _afterglowTextures[_afterglowWriteIndex];
    const RenderTexture2D& previous = _afterglowTextures[previousIndex];
    const float afterglowTime = _parameters.afterglowTime > 0.0f ? _parameters.afterglowTime : 0.001f;
    const float decay = _afterglowHasHistory ? std::exp(-deltaTime / afterglowTime) : 0.0f;
    const Rectangle source{0, 0, static_cast<float>(_frameTexture.width), static_cast<float>(_frameTexture.height)};
    const Rectangle destination{0, 0, static_cast<float>(_frameTexture.width), static_cast<float>(_frameTexture.height)};

    BeginTextureMode(target);
    ClearBackground(BLACK);
    BeginShaderMode(_afterglowShader.shader);
    setShaderTexture(_afterglowShader.shader, _afterglowShader.previousTextureLocation, previous.texture);
    setShaderFloat(_afterglowShader.shader, _afterglowShader.decayLocation, decay);
    DrawTexturePro(_frameTexture, source, destination, Vector2{0, 0}, 0, WHITE);
    EndShaderMode();
    EndTextureMode();

    _afterglowWriteIndex = previousIndex;
    _afterglowHasHistory = true;
}

void VideoRenderer::unloadTexture()
{
    if (_frameTexture.id != 0) {
        UnloadTexture(_frameTexture);
        _frameTexture = {};
    }
    _hasFrame = false;
}

void VideoRenderer::unloadAfterglowTextures()
{
    for (RenderTexture2D& texture : _afterglowTextures) {
        if (texture.id != 0) {
            UnloadRenderTexture(texture);
            texture = {};
        }
    }
    _afterglowWriteIndex = 0;
    _afterglowHasHistory = false;
}

void VideoRenderer::unloadShader()
{
    if (_crtShader.shader.id != 0) {
        UnloadShader(_crtShader.shader);
        _crtShader = {};
    }
}

void VideoRenderer::unloadAfterglowShader()
{
    if (_afterglowShader.shader.id != 0) {
        UnloadShader(_afterglowShader.shader);
        _afterglowShader = {};
    }
}

const Texture2D& VideoRenderer::displayTexture() const
{
    if (_parameters.afterglowEnabled && _afterglowHasHistory) {
        return _afterglowTextures[1 - _afterglowWriteIndex].texture;
    }
    return _frameTexture;
}

Rectangle VideoRenderer::displaySourceRectangle() const
{
    const Texture2D& texture = displayTexture();
    if (_parameters.afterglowEnabled && _afterglowHasHistory) {
        return Rectangle{0, 0, static_cast<float>(texture.width), -static_cast<float>(texture.height)};
    }
    return Rectangle{0, 0, static_cast<float>(texture.width), static_cast<float>(texture.height)};
}

} // namespace emu
