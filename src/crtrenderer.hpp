#pragma once

#include <raylib.h>

struct CrtParameters
{
    float beamHorizontalRadius = 0.2f;   // Gaussian beam radius in source pixels.
    float beamVerticalRadius = 0.38f;    // Gaussian beam radius as fraction of the scan line pitch.
    float beamIntensity = 1.6f;          // Gain compensating the dark gaps between scan lines.
    float vignetteStrength = 0.1f;
    float vignetteRadius = 0.35f;
    float maskStrength = 0.0f;           // RGB shadow mask blend, 0 disables it.
    float maskPitch = 12.0f;             // Width of an RGB mask triad in screen pixels.
};

class CrtRenderer
{
public:
    explicit CrtRenderer(CrtParameters parameters = {});
    ~CrtRenderer();

    CrtRenderer(const CrtRenderer&) = delete;
    CrtRenderer& operator=(const CrtRenderer&) = delete;

    bool load();
    void unload();
    [[nodiscard]] bool isLoaded() const;

    void setParameters(const CrtParameters& parameters);
    [[nodiscard]] const CrtParameters& parameters() const { return _parameters; }

    /// Like DrawTexturePro(), but through the CRT shader. Each texture row of
    /// @p source is one scan line, or @p linesPerRow scan lines for coarse
    /// framebuffers that don't repeat their rows per scan line themselves.
    /// Falls back to a plain draw if the shader isn't loaded.
    void draw(const Texture2D& texture, Rectangle source, Rectangle destination, Vector2 origin, float rotation, int linesPerRow = 1) const;

private:
    void applyParameters() const;

    CrtParameters _parameters;
    Shader _shader{};
    int _textureSizeLocation = -1;
    int _sourceRectLocation = -1;
    int _linesPerRowLocation = -1;
    int _beamHorizontalRadiusLocation = -1;
    int _beamVerticalRadiusLocation = -1;
    int _beamIntensityLocation = -1;
    int _vignetteStrengthLocation = -1;
    int _vignetteRadiusLocation = -1;
    int _maskStrengthLocation = -1;
    int _maskPitchLocation = -1;
};
