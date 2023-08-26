//---------------------------------------------------------------------------------------
// src/emulation/c8capturehost.hpp
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

#include <cstdint>
#include <type_traits>

template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
inline T be_dec(uint8_t*& data)
{
    T value{};

    if constexpr (sizeof(T) == 1) {
        return static_cast<T>(*data++);
    }
    else if constexpr (sizeof(T) == 2) {
        value = static_cast<T>((static_cast<uint32_t>(data[0]) << 8) | data[1]);
        data += 2;
        return value;
    }
    else if constexpr (sizeof(T) == 4) {
        value = static_cast<T>((static_cast<uint32_t>(data[0]) << 24) | (static_cast<uint32_t>(data[1]) << 16) | (static_cast<uint32_t>(data[2]) << 8) | data[3]);
        data += 4;
        return value;
    }
    else if constexpr (sizeof(T) == 8) {
        value = static_cast<T>((static_cast<uint64_t>(data[0]) << 56) | (static_cast<uint64_t>(data[1]) << 48) | (static_cast<uint64_t>(data[2]) << 40) | (static_cast<uint64_t>(data[3]) << 32) | (static_cast<uint32_t>(data[4]) << 24) |
                               (static_cast<uint32_t>(data[5]) << 16) | (static_cast<uint32_t>(data[6]) << 8) | data[7]);
        data += 8;
        return value;
    }
    else {
        return 0;
    }
}

template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
inline T le_dec(uint8_t*& data)
{
    T value{};

    if constexpr (sizeof(T) == 1) {
        return static_cast<T>(*data++);
    }
    else if constexpr (sizeof(T) == 2) {
        value = static_cast<T>((static_cast<uint32_t>(data[1]) << 8) | data[0]);
        data += 2;
        return value;
    }
    else if constexpr (sizeof(T) == 4) {
        value = static_cast<T>((static_cast<uint32_t>(data[3]) << 24) | (static_cast<uint32_t>(data[2]) << 16) | (static_cast<uint32_t>(data[1]) << 8) | data[0]);
        data += 4;
        return value;
    }
    else if constexpr (sizeof(T) == 8) {
        value = static_cast<T>((static_cast<uint64_t>(data[7]) << 56) | (static_cast<uint64_t>(data[6]) << 48) | (static_cast<uint64_t>(data[5]) << 40) | (static_cast<uint64_t>(data[4]) << 32) | (static_cast<uint32_t>(data[3]) << 24) |
                               (static_cast<uint32_t>(data[2]) << 16) | (static_cast<uint32_t>(data[1]) << 8) | data[0]);
        data += 8;
        return value;
    }
    else {
        return 0;
    }
}
