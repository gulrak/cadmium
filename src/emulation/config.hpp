//---------------------------------------------------------------------------------------
// src/emulation/config.hpp
//---------------------------------------------------------------------------------------
//
// Copyright (c) 2015, Steffen Schümann <s.schuemann@pobox.com>
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

#include <emulation/videoscreen.hpp>
#include <nlohmann/json_fwd.hpp>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>
#include <utility>

#define UNUSED(x)

namespace emu {

using json = nlohmann::ordered_json;

using cycles_t = uint64_t;

static constexpr int SUPPORTED_SCREEN_WIDTH = 256;
static constexpr int SUPPORTED_SCREEN_HEIGHT = 192;
using VideoType = VideoScreen<uint8_t, SUPPORTED_SCREEN_WIDTH, SUPPORTED_SCREEN_HEIGHT>;
using VideoRGBAType = VideoScreen<uint32_t, SUPPORTED_SCREEN_WIDTH, SUPPORTED_SCREEN_HEIGHT>;

class InternalErrorException : public std::exception
{
public:
    explicit InternalErrorException(std::string  why)
        : _why(std::move(why))
    {
    }
    ~InternalErrorException() noexcept override = default;
    const char* what() const noexcept override { return _why.c_str(); }

private:
    std::string _why;
};

}  // namespace emu
