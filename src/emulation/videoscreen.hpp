//---------------------------------------------------------------------------------------
// src/emulation/videoscreen.hpp
//---------------------------------------------------------------------------------------
//
// Copyright (c) 2023, Steffen Schümann <s.schuemann@pobox.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
//---------------------------------------------------------------------------------------
#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <stdendian/stdendian.h>
#include <emulation/properties.hpp>

namespace emu {

template<typename PixelType, int Width, int Height>
class VideoScreen
{
public:
    static constexpr int WIDTH = Width;
    static constexpr int HEIGHT = Height;
    VideoScreen()
    {
        _palette[0] = be32(0x00000000);
        _palette[1] = be32(0xFFFFFFFF);
        _palette[2] = be32(0xCCCCCCFF);
        _palette[3] = be32(0x888888FF);
        _palette[254] = be32(0xFFFFFFFF);
    }
    void setMode(int width, int height, int ratio = -1)
    {
        _width = width;
        _height = height;
        _ratio = ratio > 0 ? ratio : (width/height/2);
    }
    void setOverlayCellHeight(int height) {
        _overlayCellHeight = height;
        if(_overlayCellHeight < 0) {
            std::memset(_colorOverlay.data(), 0, 256);
            _colorOverlay[0] = 2;
            _overlayBackground = 0;
            _overlayCellHeight = 4;
        }
    }
    void setOverlayBackground(int background) { _overlayBackground = background; }
    int width() const { return _width; }
    int height() const { return _height; }
    int stride() const { return _stride; }
    int ratio() const { return _ratio; }
    constexpr static bool isRGBA() { return sizeof(PixelType) == 4; }
    void setPalette(const std::array<uint32_t, 256>& palette)
    {
        for(int i = 0; i < palette.size(); ++i) {
            _palette[i] = be32(palette[i]);
        }
    }
    void setPalette(const Palette& palette)
    {
        for(int i = 0; i < palette.size(); ++i) {
            _palette[i] = be32(palette.colors[i].toRGBAInt());
        }
    }
    uint32_t getPixel(int x, int y) const
    {
        if constexpr (isRGBA()) {
            return _screenBuffer[y * _stride + x];
        }
        return _palette[_screenBuffer[y * _stride + x]];
    }
    PixelType& getPixelRef(int x, int y)
    {
        return _screenBuffer[y * _stride + x];
    }
    void setPixel(int x, int y, PixelType value)
    {
        _screenBuffer[y * _stride + x] = value;
    }
    void setOverlayCell(int x, int y, uint8_t value)
    {
        if(_overlayCellHeight > 0)
            _colorOverlay[((y * _overlayCellHeight)&31) * 8 + (x & 7)] = value & 0xF;
    }
    void convert(uint32_t* destination, int destinationStride, uint8_t alpha, const VideoScreen<PixelType,Width,Height>* background = nullptr) const
    {
        if(isRGBA() || !_overlayCellHeight) {
            if(!background) {
                for (unsigned row = 0; row < _height; ++row) {
                    auto srcPtr = _screenBuffer.data() + row * _stride;
                    auto dstPtr = destination + row * destinationStride;
                    for (unsigned x = 0; x < _width; ++x) {
                        if constexpr (isRGBA()) {
                            blendColorsAlpha(dstPtr++, srcPtr++, alpha);
                        }
                        else {
                            *dstPtr++ = blend(_palette[*srcPtr++],alpha);
                        }
                    }
                }
            }
            else {
                for (unsigned row = 0; row < _height; ++row) {
                    auto srcPtr = _screenBuffer.data() + row * _stride;
                    auto backPtr = background->_screenBuffer.data() + row * _stride;
                    auto dstPtr = destination + row * destinationStride;
                    for (unsigned x = 0; x < _width; ++x) {
                        auto srcCol = *srcPtr++;
                        auto backCol = *backPtr++;
                        if((srcCol & be32(0x000000FF)) == 0) {
                            *dstPtr++ = blend(backCol, alpha);
                        }
                        else {
                            *dstPtr++ = blend(srcCol, alpha);
                        }
                    }
                }
            }
        }
        else {
            for (unsigned row = 0; row < _height; ++row) {
                auto srcPtr = _screenBuffer.data() + row * _stride;
                auto dstPtr = destination + row * destinationStride;
                auto overlayPtr = _colorOverlay.data() + (row/_overlayCellHeight)*_overlayCellHeight * 8;
                for (unsigned x = 0; x < _width; ++x) {
                    auto pxl = *srcPtr++;
                    if(_overlayCellHeight < 0) {
                        *dstPtr++ = pxl ? _palette[7+4] : _palette[_overlayBackground & 3];
                    }
                    else {
                        *dstPtr++ = pxl ? _palette[overlayPtr[x >> 3] + 4] : _palette[_overlayBackground & 3];
                    }
                }
            }
        }
    }
    void setAll(PixelType value)
    {
        if constexpr (isRGBA()) {
            for(auto& pixel : _screenBuffer)
                pixel = value;
        }
        else {
            std::memset(_screenBuffer.data(), value, Width * Height);
        }
    }
    void binaryAND(PixelType mask)
    {
        for(auto& pixel : _screenBuffer)
            pixel &= mask;
    }
    void scrollDown(int n)
    {
        std::memmove(_screenBuffer.data() + n * _stride, _screenBuffer.data(), (_screenBuffer.size() - n * _stride) * sizeof(PixelType));
        if constexpr (isRGBA())
            for(unsigned i = 0; i < n*_stride; ++i) _screenBuffer[i] = _black;
        else
            std::memset(_screenBuffer.data(), 0, n * _stride);
    }
    void scrollUp(int n)
    {
        std::memmove(_screenBuffer.data(), _screenBuffer.data() + n * _stride, (_screenBuffer.size() - n * _stride) * sizeof(PixelType));
        if constexpr (isRGBA())
            for(unsigned i = 0; i < n*_stride; ++i) _screenBuffer[_screenBuffer.size() - n * _stride + i] = _black;
        else
            std::memset(_screenBuffer.data() + _screenBuffer.size() - n * _stride, 0, n * _stride);
    }
    void scrollLeft(int n)
    {
        for(int y = 0; y < Height; ++y) {
            std::memmove(_screenBuffer.data() + y * _stride, _screenBuffer.data() + y * _stride + n, (_stride - n) * sizeof(PixelType));
            if constexpr (isRGBA())
                for(unsigned i = 0; i < n; ++i) _screenBuffer[y * _stride + _stride - n + i] = _black;
            else
                std::memset(_screenBuffer.data() + y * _stride + _stride - n, 0, n);
        }

    }
    void scrollRight(int n)
    {
        for(int y = 0; y < Height; ++y) {
            std::memmove(_screenBuffer.data() + y * _stride + n, _screenBuffer.data() + y * _stride, (_stride - n) * sizeof(PixelType));
            if constexpr (isRGBA())
                for(unsigned i = 0; i < n; ++i) _screenBuffer[y * _stride + i] = _black;
            else
                std::memset(_screenBuffer.data() + y * _stride, 0, n);
        }
    }
    VideoScreen& operator=(const VideoScreen& other)
    {
        _width = other._width;
        _height = other._height;
        _screenBuffer = other._screenBuffer;
        _palette = other._palette;
        return *this;
    }
    bool drawSpritePixel(uint8_t x, uint8_t y, uint8_t planes)
    {
        auto* pixel = _screenBuffer.data() + _stride * y + x;
        bool collision = false;
        if (*pixel & planes)
            collision = true;
        *pixel ^= planes;
        return collision;
    }
    bool drawSpritePixelDoubled(uint8_t x, uint8_t y, uint8_t planes, bool hires)
    {
        auto* pixel = _screenBuffer.data() + _stride * y + x;
        bool collision = false;
        if (*pixel & planes)
            collision = true;
        *pixel ^= planes;
        if(!hires) {
            if (*(pixel + 1) & planes)
                collision = true;
            *(pixel + 1) ^= planes;
            if (*(pixel + _stride) & planes)
                collision = true;
            *(pixel + _stride) ^= planes;
            if (*(pixel + _stride + 1) & planes)
                collision = true;
            *(pixel + _stride + 1) ^= planes;
        }
        return collision;
    }
    bool drawSpritePixelDoubledSC(uint8_t x, uint8_t y, uint8_t planes, bool hires)
    {
        auto* pixel = _screenBuffer.data() + _stride * y + x;
        bool collision = false;
        if (planes) {
            if (*pixel & planes)
                collision = true;
            *pixel ^= planes;
            if (!hires) {
                if (*(pixel + 1) & planes)
                    collision = true;
                *(pixel + 1) ^= planes;
            }
        }
        return collision;
    }
    void copyPixelRow(int x1, int x2, int ySrc, int yDst)
    {
        const auto* src = _screenBuffer.data() + _stride * ySrc + x1;
        auto* dst = _screenBuffer.data() + _stride * yDst + x1;
        while(x1++ < x2)
            *dst++ = *src++;
    }
    void movePixelMasked(int sx, int sy, int dx, int dy, PixelType mask)
    {
        auto& dstPixel = _screenBuffer[dy * _stride + dx];
        dstPixel = (dstPixel & ~mask) | (_screenBuffer[sy * _stride + sx] & mask);
    }
    void clearPixelMasked(int x, int y, PixelType mask)
    {
        _screenBuffer[y * _stride + x] &= ~mask;
    }
protected:
    static uint32_t blend(uint32_t color, uint8_t  alpha)
    {
        auto newAlpha = (color >> 24) * alpha / 255;
        return (color & 0x00ffffff) | (newAlpha << 24);
    }
    static void blendColorsAlpha(uint32_t* dest, const uint32_t* col, uint8_t alpha)
    {
        int a = alpha;
        auto* dst = (uint8_t*)dest;
        const auto* c1 = dst;
        const auto* c2 = (const uint8_t*)col;
        *dst++ = (a * *c2++ + (255 - a) * *c1++) >> 8;
        *dst++ = (a * *c2++ + (255 - a) * *c1++) >> 8;
        *dst++ = (a * *c2 + (255 - a) * *c1) >> 8;
        *dst = 255;
    }
    const int _stride{Width};
    int _width{Width};
    int _height{Height};
    int _ratio{1};
    int _overlayCellHeight{0};
    int _overlayBackground{0};
    PixelType _black{isRGBA() ? be32(0x00000000) : 0};
    PixelType _white{isRGBA() ? be32(0xFFFFFFFF) : 1};
    std::array<PixelType, Width*Height> _screenBuffer;
    std::array<uint32_t, 256> _palette{};
    std::array<uint8_t, 256> _colorOverlay{};
};

}
