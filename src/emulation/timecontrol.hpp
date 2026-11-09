//---------------------------------------------------------------------------------------
// src/emulation/timecontrol.hpp
//---------------------------------------------------------------------------------------
//
// Copyright (c) 2024, Steffen Schümann <s.schuemann@pobox.com>
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

#include <cstddef>
#include <cstdint>
#include <limits>

namespace emu {

class FpsMeasure
{
public:
    static constexpr int MAX_GAP_MS{1000};
    static constexpr size_t MIN_SAMPLES{8};
    float getFps() const { return _fps; }
    float getDelta() const { return _delta; }
    float getConfidence() const { return _fill / 128.0f; }
    void add(int64_t frameTime_ms)
    {
        if(_fill && frameTime_ms - _history[(_index-1)&127] > MAX_GAP_MS) {
            reset();
        }
        if(_fill < 128) {
            ++_fill;
            if(_fill >= MIN_SAMPLES) {
                // The oldest of the _fill samples is the first one, so they span _fill - 1 frames.
                updateRate(frameTime_ms - _history[0], _fill - 1);
            }
        }
        else {
            // The sample about to be replaced is 128 frames old.
            updateRate(frameTime_ms - _history[_index], 128);
        }
        _history[_index] = frameTime_ms;
        _index = (_index + 1) & 127;
    }
    void reset()
    {
        _fill = _index = 0;
        _delta = 0;
        _fps = NO_RATE;
    }
private:
    static constexpr float NO_RATE{std::numeric_limits<float>::quiet_NaN()};
    void updateRate(int64_t span_ms, size_t frames)
    {
        _delta = static_cast<int32_t>(span_ms);
        _fps = _delta ? frames * 1000.0f / _delta : NO_RATE;
    }
    float _fps{NO_RATE};
    int32_t _delta{};
    size_t _fill{0};
    size_t _index{0};
    int64_t _history[128]{};
};

}
