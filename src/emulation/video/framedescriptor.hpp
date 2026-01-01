//---------------------------------------------------------------------------------------
// src/video/framedescriptor.hpp
//---------------------------------------------------------------------------------------
//
// Copyright (c) 2025, Steffen Schümann <s.schuemann@pobox.com>
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
#include <span>
#include <variant>

namespace video {

struct PaletteFrame
{
    std::span<const uint8_t> pixels;
    std::span<const uint32_t> palette;  // RGBA colors
    int width;
    int height;
    int pitch;
};

struct RGBAFrame
{
    std::span<const uint32_t> pixels;
    int width;
    int height;
    int pitch;
    bool hasBorders = false;  // true for PAL/NTSC with borders
};

using SourceFrame = std::variant<PaletteFrame, RGBAFrame>;

struct RenderTarget
{
    uint32_t* pixels;  // RGBA output buffer
    int width;
    int height;
    int pitch;  // bytes per row (may differ from width * 4)
};

struct ScaledRegion
{
    int x, y;           // Top-left position in target
    int width, height;  // Scaled dimensions
    int scaleFactor;    // Integer scaling factor applied
};

}  // namespace video
