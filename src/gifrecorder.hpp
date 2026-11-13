//---------------------------------------------------------------------------------------
// src/gifrecorder.hpp
//---------------------------------------------------------------------------------------
//
// Copyright (c) 2026, Steffen Schümann <s.schuemann@pobox.com>
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
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <vector>

// Spreads a recording over GIF delays, these are whole hundredths of a second
// and browsers slow down anything below two of them.
class GifFramePacer
{
public:
    explicit GifFramePacer(int minDelay = 3);

    // Advances by the elapsed seconds, returns the delay for a frame that is due now or 0 if there is none.
    int advance(double seconds);

private:
    int _minDelay;
    double _elapsed{0.0};
    int64_t _emitted{0};
    bool _started{false};
};

// Keeps the mouse clicks of a recording around for as long as their animation takes.
class GifClickMarks
{
public:
    struct Mark
    {
        float x, y, age;
    };
    static constexpr float DURATION = 0.35f;

    void clear() { _marks.clear(); }
    void add(float x, float y) { _marks.push_back({x, y, 0.0f}); }
    void advance(float seconds);
    const std::vector<Mark>& marks() const { return _marks; }

private:
    std::vector<Mark> _marks;
};

// The last keyboard shortcut of a recording, shown for a while. The same shortcut again
// while it is still shown counts up and starts the time over.
class GifShortcutCaption
{
public:
    static constexpr float DURATION = 1.5f;

    void clear();
    void show(const std::string& text);
    void advance(float seconds);
    bool visible() const { return !_text.empty(); }
    const std::string& text() const { return _text; }
    int count() const { return _count; }

private:
    std::string _text;
    int _count{0};
    float _age{0.0f};
};

// Writes an animated GIF frame by frame. Every frame only stores the rectangle that changed,
// with the unchanged pixels inside of it being transparent, and a frame without changes just
// extends the previous one. Frames with up to 256 colors stay exact, others get ordered dithering.
class GifRecorder
{
public:
    using Sink = std::function<bool(std::span<const uint8_t>)>;

    GifRecorder(int width, int height, Sink sink);

    // The pixels are R8G8B8A8 with the alpha being ignored, a negative stride stores the rows bottom up.
    // pixels points to the first stored row and stride is given in pixels.
    bool addFrame(const uint32_t* pixels, int stride, int delay);
    bool finish();

    bool good() const { return _good; }
    int width() const { return _width; }
    int height() const { return _height; }
    size_t bytesWritten() const { return _bytesWritten; }

private:
    struct Rect
    {
        int left, top, width, height;
    };
    struct ColorSlot
    {
        uint32_t color;
        uint16_t index;
        uint16_t generation;
    };
    using ChannelTable = std::array<std::array<uint8_t, 256>, 16>;

    bool findChanges(Rect& rect) const;
    void encodeFrame(const Rect& rect, int delay);
    int indexPixels(const Rect& rect, int level);
    void prepareDither(int level);
    void flushPending();
    void write(std::span<const uint8_t> bytes);
    size_t area() const { return static_cast<size_t>(_width) * _height; }

    int _width;
    int _height;
    Sink _sink;
    std::vector<uint32_t> _current;
    std::vector<uint32_t> _previous;
    std::vector<uint8_t> _indices;
    std::vector<uint8_t> _pending;
    std::vector<ColorSlot> _slots;
    std::array<uint32_t, 256> _colors{};
    std::array<ChannelTable, 3> _dither{};
    int _ditherLevel{0};
    int _level{0};
    int _frames{0};
    uint16_t _generation{0};
    size_t _bytesWritten{0};
    bool _good{true};
    bool _finished{false};
};

// Resizes without any filtering, so emulated pixels keep their edges.
void scaleNearest(const uint32_t* source, int sourceWidth, int sourceHeight, int sourceStride, uint32_t* destination, int destinationWidth, int destinationHeight);
