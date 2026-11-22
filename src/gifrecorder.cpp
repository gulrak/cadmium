//---------------------------------------------------------------------------------------
// src/gifrecorder.cpp
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
#include <gifrecorder.hpp>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <iostream>
#include <utility>

#include <ghc/lzw.hpp>

namespace {

constexpr int MAX_DELAY = 65535;
constexpr uint32_t OPAQUE_ALPHA = 0xFF000000u;
constexpr uint32_t COLOR_MASK = 0x00FFFFFFu;
constexpr size_t COLOR_SLOTS = 2048;
constexpr int PROBE_INTERVAL = 16;
constexpr size_t DELAY_OFFSET = 4;

struct ChannelBits
{
    int red, green, blue;
};

// The last level has less than 256 colors, so it fits even with the transparent index.
constexpr std::array<ChannelBits, 8> QUALITY_LEVELS{{{8, 8, 8}, {5, 6, 5}, {5, 5, 5}, {4, 5, 4}, {4, 4, 4}, {3, 4, 3}, {3, 3, 3}, {2, 3, 2}}};

constexpr uint8_t BAYER[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};

// A plain output iterator that splits the LZW stream into the sub-blocks of a GIF.
class SubBlockOutput
{
public:
    explicit SubBlockOutput(std::vector<uint8_t>& buffer)
        : _buffer(buffer)
        , _start(buffer.size())
    {
        _buffer.push_back(0);
    }
    SubBlockOutput& operator=(uint8_t value)
    {
        _buffer.push_back(value);
        if(++_size == 255) {
            _buffer[_start] = 255;
            _start = _buffer.size();
            _buffer.push_back(0);
            _size = 0;
        }
        return *this;
    }
    SubBlockOutput& operator*() { return *this; }
    SubBlockOutput& operator++(int) { return *this; }
    // After a full block the prepared empty one is the terminator already.
    void finish()
    {
        if(_size) {
            _buffer[_start] = static_cast<uint8_t>(_size);
            _buffer.push_back(0);
        }
    }

private:
    std::vector<uint8_t>& _buffer;
    size_t _start;
    int _size{0};
};

void appendU16(std::vector<uint8_t>& buffer, int value)
{
    buffer.push_back(static_cast<uint8_t>(value & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
}

}  // namespace

GifFramePacer::GifFramePacer(int minDelay)
    : _minDelay(minDelay)
{
}

int GifFramePacer::advance(double seconds)
{
    _elapsed += seconds;
    if(!_started) {
        _started = true;
        return _minDelay;
    }
    // The epsilon keeps the sum of many frame times from landing just below a whole hundredth.
    const auto whole = static_cast<int64_t>(std::floor(_elapsed * 100.0 + 1e-6));
    const auto delay = whole - _emitted;
    if(delay < _minDelay)
        return 0;
    _emitted = whole;
    return static_cast<int>(std::min<int64_t>(delay, MAX_DELAY));
}

void GifClickMarks::advance(float seconds)
{
    for(auto& mark : _marks)
        mark.age += seconds;
    std::erase_if(_marks, [](const Mark& mark) { return mark.age >= DURATION; });
}

void GifShortcutCaption::clear()
{
    _text.clear();
    _count = 0;
    _age = 0.0f;
}

void GifShortcutCaption::show(const std::string& text)
{
    if(text != _text) {
        _text = text;
        _count = 0;
    }
    ++_count;
    _age = 0.0f;
}

void GifShortcutCaption::advance(float seconds)
{
    if(_text.empty())
        return;
    _age += seconds;
    if(_age >= DURATION)
        clear();
}

GifRecorder::GifRecorder(int width, int height, Sink sink)
    : _width(width)
    , _height(height)
    , _sink(std::move(sink))
    , _current(area())
    , _previous(area())
    , _slots(COLOR_SLOTS, ColorSlot{0, 0, 0})
{
    static constexpr char SIGNATURE[] = "GIF89a";
    static constexpr char LOOP_ID[] = "NETSCAPE2.0";
    std::vector<uint8_t> header(SIGNATURE, SIGNATURE + 6);
    appendU16(header, _width);
    appendU16(header, _height);
    // No global color table, every frame has a local one with only the colors it uses.
    header.insert(header.end(), {0x70, 0, 0, 0x21, 0xFF, 11});
    header.insert(header.end(), LOOP_ID, LOOP_ID + 11);
    header.insert(header.end(), {3, 1, 0, 0, 0});
    write(header);
}

bool GifRecorder::addFrame(const uint32_t* pixels, int stride, int delay)
{
    if(!_good || _finished)
        return false;
    delay = std::clamp(delay, 1, MAX_DELAY);
    auto* out = _current.data();
    for(int y = 0; y < _height; ++y) {
        const auto* row = pixels + static_cast<ptrdiff_t>(y) * stride;
        for(int x = 0; x < _width; ++x)
            *out++ = row[x] | OPAQUE_ALPHA;
    }
    Rect rect{0, 0, _width, _height};
    if(_frames && !findChanges(rect)) {
        const int previousDelay = _pending[DELAY_OFFSET] | _pending[DELAY_OFFSET + 1] << 8;
        if(previousDelay + delay <= MAX_DELAY) {
            const int sum = previousDelay + delay;
            _pending[DELAY_OFFSET] = static_cast<uint8_t>(sum & 0xFF);
            _pending[DELAY_OFFSET + 1] = static_cast<uint8_t>(sum >> 8);
            return true;
        }
        // The delay field is full, go on with a frame that shows nothing new.
        rect = {0, 0, 1, 1};
    }
    flushPending();
    encodeFrame(rect, delay);
    _previous.swap(_current);
    ++_frames;
    return _good;
}

bool GifRecorder::finish()
{
    if(_finished)
        return _good;
    flushPending();
    _finished = true;
    const uint8_t trailer = 0x3B;
    write({&trailer, 1});
    return _good;
}

bool GifRecorder::findChanges(Rect& rect) const
{
    const auto rowSize = static_cast<size_t>(_width) * sizeof(uint32_t);
    const auto* current = _current.data();
    const auto* previous = _previous.data();
    int top = 0;
    while(top < _height && !std::memcmp(current + static_cast<size_t>(top) * _width, previous + static_cast<size_t>(top) * _width, rowSize))
        ++top;
    if(top == _height)
        return false;
    int bottom = _height - 1;
    while(!std::memcmp(current + static_cast<size_t>(bottom) * _width, previous + static_cast<size_t>(bottom) * _width, rowSize))
        --bottom;
    int left = _width - 1;
    int right = 0;
    for(int y = top; y <= bottom; ++y) {
        const auto* a = current + static_cast<size_t>(y) * _width;
        const auto* b = previous + static_cast<size_t>(y) * _width;
        for(int x = 0; x < left; ++x) {
            if(a[x] != b[x]) {
                left = x;
                break;
            }
        }
        for(int x = _width - 1; x > right; --x) {
            if(a[x] != b[x]) {
                right = x;
                break;
            }
        }
    }
    right = std::max(left, right);
    rect = {left, top, right - left + 1, bottom - top + 1};
    return true;
}

void GifRecorder::encodeFrame(const Rect& rect, int delay)
{
    _indices.resize(static_cast<size_t>(rect.width) * rect.height);
    // A frame needing fewer colors can follow at any time, so a better level is tried every now and then.
    int level = _level > 0 && _frames % PROBE_INTERVAL == 0 ? _level - 1 : _level;
    int count = indexPixels(rect, level);
    while(!count)
        count = indexPixels(rect, ++level);
    _level = level;

    const int bits = std::max(2, static_cast<int>(std::bit_width(static_cast<unsigned>(count - 1))));
    const bool transparency = _frames != 0;
    _pending.clear();
    // Graphic Control Extension with disposal "do not dispose", the delay has to stay at DELAY_OFFSET
    _pending.insert(_pending.end(), {0x21, 0xF9, 4, static_cast<uint8_t>(1 << 2 | (transparency ? 1 : 0))});
    appendU16(_pending, delay);
    _pending.insert(_pending.end(), {0, 0, 0x2C});
    appendU16(_pending, rect.left);
    appendU16(_pending, rect.top);
    appendU16(_pending, rect.width);
    appendU16(_pending, rect.height);
    _pending.push_back(static_cast<uint8_t>(0x80 | (bits - 1)));
    for(int i = 0; i < (1 << bits); ++i) {
        const uint32_t color = i < count ? _colors[i] : 0;
        _pending.insert(_pending.end(), {static_cast<uint8_t>(color), static_cast<uint8_t>(color >> 8), static_cast<uint8_t>(color >> 16)});
    }
    _pending.push_back(static_cast<uint8_t>(bits));
    SubBlockOutput output(_pending);
    {
        ghc::compression::LzwEncoder<SubBlockOutput> lzw(output, bits);
        lzw.encode(_indices);
    }
    output.finish();
}

int GifRecorder::indexPixels(const Rect& rect, int level)
{
    if(++_generation == 0) {
        std::fill(_slots.begin(), _slots.end(), ColorSlot{0, 0, 0});
        _generation = 1;
    }
    prepareDither(level);
    const bool transparency = _frames != 0;
    int count = transparency ? 1 : 0;
    _colors[0] = 0;
    auto* out = _indices.data();
    uint32_t lastColor = 0xFFFFFFFFu;
    uint8_t lastIndex = 0;
    for(int y = rect.top; y < rect.top + rect.height; ++y) {
        const auto* current = _current.data() + static_cast<size_t>(y) * _width;
        const auto* previous = _previous.data() + static_cast<size_t>(y) * _width;
        for(int x = rect.left; x < rect.left + rect.width; ++x) {
            if(transparency && current[x] == previous[x]) {
                *out++ = 0;
                continue;
            }
            uint32_t color = current[x] & COLOR_MASK;
            if(level) {
                const auto threshold = BAYER[y & 3][x & 3];
                color = _dither[0][threshold][color & 0xFF] | _dither[1][threshold][(color >> 8) & 0xFF] << 8 | static_cast<uint32_t>(_dither[2][threshold][color >> 16]) << 16;
            }
            if(color != lastColor) {
                auto slot = (color * 2654435761u) >> 21;
                while(true) {
                    auto& entry = _slots[slot];
                    if(entry.generation != _generation) {
                        if(count == 256)
                            return 0;
                        entry = {color, static_cast<uint16_t>(count), _generation};
                        _colors[count] = color;
                        lastIndex = static_cast<uint8_t>(count++);
                        break;
                    }
                    if(entry.color == color) {
                        lastIndex = static_cast<uint8_t>(entry.index);
                        break;
                    }
                    slot = (slot + 1) & (COLOR_SLOTS - 1);
                }
                lastColor = color;
            }
            *out++ = lastIndex;
        }
    }
    return count;
}

void GifRecorder::prepareDither(int level)
{
    if(!level || level == _ditherLevel)
        return;
    _ditherLevel = level;
    const auto& bits = QUALITY_LEVELS[level];
    const std::array<int, 3> channelBits{bits.red, bits.green, bits.blue};
    for(size_t channel = 0; channel < 3; ++channel) {
        const int maximum = (1 << channelBits[channel]) - 1;
        for(int threshold = 0; threshold < 16; ++threshold) {
            for(int value = 0; value < 256; ++value) {
                const int quantized = (value * maximum * 16 + threshold * 255) / (255 * 16);
                _dither[channel][threshold][value] = static_cast<uint8_t>(quantized * 255 / maximum);
            }
        }
    }
}

void GifRecorder::flushPending()
{
    if(!_pending.empty()) {
        write(_pending);
        _pending.clear();
    }
}

void GifRecorder::write(std::span<const uint8_t> bytes)
{
    if(!_good)
        return;
    _good = _sink(bytes);
    if(_good)
        _bytesWritten += bytes.size();
}

void scaleNearest(const uint32_t* source, int sourceWidth, int sourceHeight, int sourceStride, uint32_t* destination, int destinationWidth, int destinationHeight)
{
    for(int y = 0; y < destinationHeight; ++y) {
        const auto* row = source + static_cast<ptrdiff_t>(y * sourceHeight / destinationHeight) * sourceStride;
        for(int x = 0; x < destinationWidth; ++x)
            *destination++ = row[x * sourceWidth / destinationWidth];
    }
}
