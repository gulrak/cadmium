#pragma once

#include <raylib.h>

class TextureScaler
{
public:
    enum Mode { POINT, POINT_LINEAR, LINEAR};
    TextureScaler(int width, int height, Mode mode = POINT_LINEAR)
    : _mode(mode)
    , _width(width)
    , _height(height)
    {
        _renderTexture = LoadRenderTexture(_width, _height);
        setOutputSize(_width, _height);
        SetTextureFilter(_renderTexture.texture, _mode == LINEAR ? TEXTURE_FILTER_BILINEAR : TEXTURE_FILTER_POINT);
    }
    ~TextureScaler()
    {
        if (_renderTexture.id) UnloadRenderTexture(_renderTexture);
        if (_intermediateTexture.id) UnloadRenderTexture(_intermediateTexture);
    }
    void setMode(Mode mode)
    {
        _mode = mode;
    }
    void setOutputSize(int width, int height)
    {
        _outputWidth = width;
        _outputHeight = height;
        _intermediateWidth = ((_outputWidth + _width/2) / _width ) * _width;
        _intermediateHeight = ((_outputHeight + _height/2) / _height) * _height;
        if (_intermediateTexture.id) {
            UnloadRenderTexture(_intermediateTexture);
        }
        if (_mode == POINT_LINEAR) {
            _intermediateTexture = LoadRenderTexture(_intermediateWidth, _intermediateHeight);
            SetTextureFilter(_intermediateTexture.texture, TEXTURE_FILTER_BILINEAR);
        }
        else {
            _intermediateTexture.id = 0;
        }
        SetTextureFilter(_renderTexture.texture, _mode == LINEAR ? TEXTURE_FILTER_BILINEAR : TEXTURE_FILTER_POINT);
    }
    RenderTexture& getRenderTexture() { return _renderTexture; }
    void updateIntermediateTexture() const
    {
        if (!_intermediateTexture.id || _mode != POINT_LINEAR)
            return;
        BeginTextureMode(_intermediateTexture);
        DrawTexturePro(_renderTexture.texture, (Rectangle){0, 0, (float)_renderTexture.texture.width, -(float)_renderTexture.texture.height}, (Rectangle){0, 0, (float)_intermediateTexture.texture.width, (float)_intermediateTexture.texture.height},
                       (Vector2){0, 0}, 0.0f, WHITE);
        EndTextureMode();
    }
    void draw(const float x = 0.0f, const float y = 0.0f) const
    {
        if (_mode == POINT_LINEAR && _intermediateTexture.id) {
            DrawTexturePro(_intermediateTexture.texture, (Rectangle){0, 0, (float)_intermediateTexture.texture.width, -(float)_intermediateTexture.texture.height}, (Rectangle){x, y, (float)_outputWidth, (float)_outputHeight},
                (Vector2){0, 0}, 0.0f, WHITE);
        }
        else {
            DrawTexturePro(_renderTexture.texture, (Rectangle){0, 0, (float)_renderTexture.texture.width, -(float)_renderTexture.texture.height}, (Rectangle){x, y, (float)_outputWidth, (float)_outputHeight},
                (Vector2){0, 0}, 0.0f, WHITE);
        }
    }
private:
    Mode _mode{POINT_LINEAR};
    int _width{};
    int _height{};
    int _outputWidth{};
    int _outputHeight{};
    int _intermediateWidth{};
    int _intermediateHeight{};
    RenderTexture _renderTexture{};
    RenderTexture _intermediateTexture{};
};
