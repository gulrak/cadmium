//---------------------------------------------------------------------------------------
// src/emulation/math.hpp
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

#include <emulation/config.hpp>

#include <type_traits>

namespace emu {
namespace math {

inline int64_t mul32by32(int32_t val1, int32_t val2)
{
    return int64_t(val1) * int64_t(val2);
}

inline uint64_t mulu32by32(uint32_t val1, uint32_t val2)
{
    return uint64_t(val1) * uint64_t(val2);
}

inline int32_t div64by32(int64_t dividend, int32_t divisor)
{
    return int32_t(dividend / int64_t(divisor));
}

inline int32_t div64by32(int64_t dividend, int32_t divisor, int32_t& remainder)
{
    remainder = int32_t(dividend % int64_t(divisor));
    return int32_t(dividend / int64_t(divisor));
}

inline uint32_t divu64by32(uint64_t dividend, uint32_t divisor)
{
    return uint32_t(dividend / uint64_t(divisor));
}

inline uint32_t divu64by32(uint64_t dividend, uint32_t divisor, uint32_t& remainder)
{
    remainder = uint32_t(dividend % uint64_t(divisor));
    return uint32_t(dividend / uint64_t(divisor));
}

template <typename T1>
inline uint32_t split64(T1 val64, uint32_t& lower32)
{
    lower32 = uint32_t(val64);
    return uint32_t(uint64_t(val64) >> 32);
}

template <typename T1>
inline void split64(T1 val64, uint32_t& higher32, uint32_t& lower32)
{
    lower32 = uint32_t(val64);
    higher32 = uint32_t(uint64_t(val64) >> 32);
}

inline int64_t combineToInt64(uint32_t hi, uint32_t lo)
{
    return int64_t((uint64_t(hi) << 32) | lo);
}

inline uint64_t combineToUint64(uint32_t hi, uint32_t lo)
{
    return (uint64_t(hi) << 32) | lo;
}

#ifdef __has_builtin
#if __has_builtin(__builtin_add_overflow)
#define HAS_BUILTIN_ADD_OVERFLOW
#endif
#endif
#if __GNUC__ >= 5 || defined(HAS_BUILTIN_ADD_OVERFLOW)
template<typename T>
inline bool addOverflows(T a, T b, T* out)
{
    return __builtin_add_overflow(a, b, out);
}
template<typename T>
inline bool subOverflows(T a, T b, T* out)
{
    return __builtin_sub_overflow(a, b, out);
}
template<typename T>
inline bool mulOverflows(T a, T b, T* out)
{
    return __builtin_mul_overflow(a, b, out);
}
#else
template<typename T>
inline T asUnsigned(T v)
{
    return static_cast<std::make_unsigned<T>>(v);
}
template<typename T>
inline bool addOverflows(T a, T b, T* out)
{
    if constexpr (std::is_unsigned<T>::value) {
        *out = a + b;
        return *out < a;
    }
    else {
        *out = static_cast<T>(asUnsigned(a) + asUnsigned(b));
        return (a < 0 && b < 0 && *out > 0) || (a > 0 && b > 0 && *out < 0);
    }
}
template<typename T>
inline bool subOverflows(T a, T b, T* out)
{
    if constexpr (std::is_unsigned<T>::value) {
        *out = a - b;
        return *out > a;
    }
    else {
        *out = static_cast<T>(asUnsigned(a) - asUnsigned(b));
        return (b < 0 && a > std::numeric_limits<T>::max() + b) || (b > 0 && a < std::numeric_limits<T>::min() + b);
    }
}
template<typename T>
inline bool mulOverflows(T a, T b, T* out)
{
    if constexpr (std::is_unsigned<T>::value) {
        *out = a * b;
        return (b != 0 && a > std::numeric_limits<T>::max() / b);
    }
    else {
        *out = static_cast<T>(asUnsigned(a) * asUnsigned(b));
        return (a == -1 && b == std::numeric_limits<T>::min()) || (b == -1 && a == std::numeric_limits<T>::min()) || (b != 0 && a > std::numeric_limits<T>::max() / b) || (b != 0 && a < std::numeric_limits<T>::min() / b);
    }
}
#endif

}  // namespace math
}  // namespace emu
