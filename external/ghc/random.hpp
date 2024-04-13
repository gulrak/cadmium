//---------------------------------------------------------------------------------------
// src/random.hpp - warning this is not for cryptographic use, it's used for emulation!!!
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

#include <cstdint>
#include <random>

namespace ghc
{

class RandomLCG
{
public:
    enum Type { };
    explicit RandomLCG(uint32_t seed = 1)
        : _state(seed ? seed : 1)
    {}
    uint16_t operator()()
    {
        next();
        return _state >> 16;
    }
private:
    void next()
    {
        _state = ((_state * 1103515245) + 12345) & 0x7FFFFFFF;
    }
    uint32_t _state{1};
};

class RandomMT
{
public:
    RandomMT(uint32_t seed = 0)
        : _engine(seed ? seed : std::random_device()())
    {}
    uint16_t operator()()
    {
        return _engine();
    }
private:
    std::mt19937 _engine;
};

}
