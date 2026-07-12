#pragma once

#include <raylib.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace emu {

// Beam radii are fractions of the rendered distance between source scanline centers:
// 0.5 reaches halfway to the next line, 1.0 reaches the next line center.
constexpr float DEFAULT_CRT_BEAM_HORIZONTAL_RADIUS = 0.38f;
constexpr float DEFAULT_CRT_BEAM_VERTICAL_RADIUS = 0.38f;
constexpr float DEFAULT_CRT_BEAM_INTENSITY = 1.80f;
constexpr float DEFAULT_CRT_VIGNETTE_STRENGTH = 0.28f;
constexpr float DEFAULT_CRT_VIGNETTE_RADIUS = 0.35f;
constexpr float DEFAULT_AFTERGLOW_TIME = 0.02f;

struct VideoRenderParameters {
    bool crtEnabled = false;
    bool afterglowEnabled = false;
    float beamHorizontalRadius = DEFAULT_CRT_BEAM_HORIZONTAL_RADIUS;
    float beamVerticalRadius = DEFAULT_CRT_BEAM_VERTICAL_RADIUS;
    float beamIntensity = DEFAULT_CRT_BEAM_INTENSITY;
    float vignetteStrength = DEFAULT_CRT_VIGNETTE_STRENGTH;
    float vignetteRadius = DEFAULT_CRT_VIGNETTE_RADIUS;
    float afterglowTime = DEFAULT_AFTERGLOW_TIME;
};

class VideoRenderer {
public:
    explicit VideoRenderer(VideoRenderParameters parameters = {});
    ~VideoRenderer();

    VideoRenderer(const VideoRenderer&) = delete;
    VideoRenderer& operator=(const VideoRenderer&) = delete;

    void setParameters(const VideoRenderParameters& parameters);
    void setCrtEnabled(bool enabled);
    void setAfterglowEnabled(bool enabled);

    bool hasFrame() const;

    template <typename Frame>
    void updateFrame(const Frame& frame, float deltaTime = 1.0f / 60.0f)
    {
        _hasFrame = !frame.empty();
        if (!_hasFrame) {
            return;
        }

        ensureTextureSize(frame.width(), frame.height());
        _hasFrame = _frameTexture.id != 0;
        if (!_hasFrame) {
            return;
        }

        const uint32_t* pixels = frame.data();
        if (frame.stride() != frame.width()) {
            _contiguousFrame.resize(frame.width() * frame.height());
            for (std::size_t y = 0; y < frame.height(); ++y) {
                std::copy(frame.row(y).begin(), frame.row(y).end(), _contiguousFrame.begin() + static_cast<std::ptrdiff_t>(y * frame.width()));
            }
            pixels = _contiguousFrame.data();
        }
        UpdateTexture(_frameTexture, pixels);
        updateAfterglow(deltaTime);
    }

    void draw(Rectangle destination);
    void reloadShader();
    void unload();

private:
    struct CrtShader {
        Shader shader{};
        int sourceSizeLocation = -1;
        int beamHorizontalRadiusLocation = -1;
        int beamVerticalRadiusLocation = -1;
        int beamIntensityLocation = -1;
        int vignetteStrengthLocation = -1;
        int vignetteRadiusLocation = -1;
    };

    struct AfterglowShader {
        Shader shader{};
        int previousTextureLocation = -1;
        int decayLocation = -1;
    };

    void ensureTextureSize(std::size_t width, std::size_t height);
    void ensureAfterglowResources();
    void applyShaderParameters();
    void updateAfterglow(float deltaTime);
    void unloadTexture();
    void unloadAfterglowTextures();
    void unloadShader();
    void unloadAfterglowShader();
    const Texture2D& displayTexture() const;
    Rectangle displaySourceRectangle() const;

    VideoRenderParameters _parameters;
    CrtShader _crtShader;
    AfterglowShader _afterglowShader;
    Texture2D _frameTexture{};
    RenderTexture2D _afterglowTextures[2]{};
    int _afterglowWriteIndex = 0;
    bool _hasFrame = false;
    bool _afterglowHasHistory = false;
    std::vector<uint32_t> _contiguousFrame;
};

} // namespace emu
