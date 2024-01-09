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

namespace emu {

class FpsMeasure
{
public:
    inline static constexpr int MAX_GAP_MS{1000};
    float getFps() const { return _fps; }
    float getDelta() const { return _delta; }
    float getConfidence() const { return _fill / 128.0f; }
    void add(int64_t frameTime_ms)
    {
        const auto lastTime_ms = _fill ? _history[(_index-1)&127] : 0;
        if(lastTime_ms && frameTime_ms - lastTime_ms > MAX_GAP_MS) {
            _fill = _index = 0;
        }
        if(_fill < 128) {
            ++_fill;
            _delta = frameTime_ms - _history[0];
            _fps = _delta ? _fill * 1000.0f / (frameTime_ms - _history[0]) : 0;
        }
        else {
            _delta = frameTime_ms - _history[_index];
            _fps = _delta ? 128000.0f / (frameTime_ms - _history[_index]) : 0;
        }
        _history[_index] = frameTime_ms;
        _index = (_index + 1) & 127;
    }
    void reset()
    {
        _fill = _index = 0;
    }
private:
    float _fps{};
    int32_t _delta{};
    size_t _fill{0};
    size_t _index{0};
    int64_t _history[128]{};
};

}
