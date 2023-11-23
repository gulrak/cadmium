//---------------------------------------------------------------------------------------
// src/emulation/hpsaturnl2.hpp
//---------------------------------------------------------------------------------------
//
// Copyright (c) 2022, Steffen Schümann <s.schuemann@pobox.com>
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
#include <algorithm>
#include <cstdint>
#include <type_traits>
#include <limits>

#include <fmt/format.h>

namespace emu {
// clang-format off
namespace detail {
template< typename T >
struct next_safe;

template<> struct next_safe<uint8_t> { using type = uint16_t; };
template<> struct next_safe<int8_t> { using type = int16_t; };
template<> struct next_safe<uint16_t> { using type = uint32_t; };
template<> struct next_safe<int16_t> { using type = int32_t; };
template<> struct next_safe<uint32_t> { using type = uint64_t; };
template<> struct next_safe<int32_t> { using type = int64_t; };
// clang-format on
}

template<typename PodType, typename = typename std::enable_if_t<std::is_integral_v<PodType>, PodType>>
class Integer
{
public:
    using Self = Integer<PodType>;
    using value_type = PodType;
    using safe_type = typename detail::next_safe<value_type>::type;

    Integer() : _value{0} {}
    explicit Integer(value_type val) : _value(val) {}
    Integer(Self other) : _value(other._value) {}

    PodType asNative() const { return _value; }

    Self& operator=(const Self& other) { _value = other._value; }
    template<typename PodType2, typename = typename std::enable_if<std::is_arithmetic<PodType2>::value, PodType2>::type>
    explicit operator Integer<PodType2>() const { return Integer<PodType2>((PodType2)_value); }

    template<typename Other, typename std::enable_if_t<std::is_integral_v<Other> && sizeof(Other) <= sizeof(PodType)>>
    Self& operator+=(const Integer<Other>& other) { _value = value_type((safe_type)_value + other._value); }
    template<typename Other, typename std::enable_if_t<std::is_integral_v<Other> && sizeof(Other) <= sizeof(PodType)>>
    Self& operator-=(const Integer<Other>& other) { _value = value_type((safe_type)_value - other._value); }
    template<typename Other, typename std::enable_if_t<std::is_integral_v<Other> && sizeof(Other) <= sizeof(PodType)>>
    Self& operator*=(const Integer<Other>& other) { _value = value_type((safe_type)_value * other._value); }
    template<typename Other, typename std::enable_if_t<std::is_integral_v<Other> && sizeof(Other) <= sizeof(PodType)>>
    bool addTo_overflow(const Integer<Other>& other)
    {
        auto result = safe_type(_value) + other._value;
        _value = value_type(result);
        return result < std::numeric_limits<value_type>::min() || result > std::numeric_limits<value_type>::max();
    }
    template<typename Other, typename std::enable_if_t<std::is_integral_v<Other> && sizeof(Other) <= sizeof(PodType)>>
    bool subTo_overflow(const Integer<Other>& other)
    {
        auto result = safe_type(_value) - other._value;
        _value = value_type(result);
        return result < std::numeric_limits<value_type>::min() || result > std::numeric_limits<value_type>::max();
    }
    template<typename Other, typename std::enable_if_t<std::is_integral_v<Other> && sizeof(Other) <= sizeof(PodType)>>
    bool mulTo_overflow(const Integer<Other>& other)
    {
        auto result = safe_type(_value) * other._value;
        _value = value_type(result);
        return result < std::numeric_limits<value_type>::min() || result > std::numeric_limits<value_type>::max();
    }
    template<typename Other, typename std::enable_if_t<std::is_integral_v<Other> && sizeof(Other) <= sizeof(PodType)>>
    Self operator+(const Integer<Other>& other) const
    {
        return Self(value_type(safe_type(_value) + other._value));
    }
    template<typename Other, typename std::enable_if_t<std::is_integral_v<Other> && sizeof(Other) <= sizeof(PodType)>>
    Self operator-(const Integer<Other>& other) const
    {
        return Self(value_type(safe_type(_value) - other._value));
    }
    template<typename Other, typename std::enable_if_t<std::is_integral_v<Other> && sizeof(Other) <= sizeof(PodType)>>
    Self operator*(const Integer<Other>& other) const
    {
        return Self(value_type(safe_type(_value) * other._value));
    }
    Self operator<<(unsigned other) const
    {
        return Self(_value << other);
    }
    Self operator>>(unsigned other) const
    {
        if constexpr (std::is_signed_v<PodType>) {
            if(_value >= 0)
                return Self(_value >> other);
            else
                return Self(-(-safe_type(_value) >> other));
        }
        else {
            return Self(_value << other);
        }
    }
    template<typename Other, typename std::enable_if_t<std::is_integral_v<Other> && sizeof(Other) <= sizeof(PodType)>>
    Self operator&(const Integer<Other>& other) const
    {
        return Self(_value & other._value);
    }
    template<typename Other, typename std::enable_if_t<std::is_integral_v<Other> && sizeof(Other) <= sizeof(PodType)>>
    Self operator|(const Integer<Other>& other) const
    {
        return Self(_value & other._value);
    }
    template<typename Other, typename std::enable_if_t<std::is_integral_v<Other> && sizeof(Other) <= sizeof(PodType)>>
    std::pair<Self, bool> add_overflow(const Integer<Other>& other) const
    {
        auto result{*this};
        bool overflow = result.addTo_overflow(other);
        return {result, overflow};
    }
    template<typename Other, typename std::enable_if_t<std::is_integral_v<Other> && sizeof(Other) <= sizeof(PodType)>>
    std::pair<Self, bool> sub_overflow(const Integer<Other>& other) const
    {
        auto result{*this};
        bool overflow = result.subTo_overflow(other);
        return {result, overflow};
    }
    template<typename Other, typename std::enable_if_t<std::is_integral_v<Other> && sizeof(Other) <= sizeof(PodType)>>
    std::pair<Self, bool> mul_overflow(const Integer<Other>& other) const
    {
        auto result{*this};
        bool overflow = result.mulTo_overflow(other);
        return {result, overflow};
    }
    bool operator<(const Self& other) const { return _value < other._value; }
    bool operator>(const Self& other) const { return _value > other._value; }
    bool operator<=(const Self& other) const { return _value <= other._value; }
    bool operator>=(const Self& other) const { return _value >= other._value; }
    bool operator==(const Self& other) const { return _value == other._value; }
    bool operator!=(const Self& other) const { return _value != other._value; }

private:
    PodType _value;
};

using RI8 = Integer<int8_t>;
using RU8 = Integer<uint8_t>;
using RI16 = Integer<int16_t>;
using RU16 = Integer<uint16_t>;
using RI32 = Integer<int32_t>;
using RU32 = Integer<uint32_t>;

template<typename ValueType = uint8_t>
class Bitfield {
public:
    Bitfield() = delete;
    Bitfield(std::string names) : _names(std::move(names)) {}
    Bitfield(std::string names, ValueType positions, ValueType values) : _names(std::move(names)) { setFromVal(positions, values); }
    void setFromVal(ValueType positions, ValueType values) { _val = (_val & ~positions) | (values & positions); }
    void setFromBool(ValueType positions, bool asOnes) { if(asOnes) set(positions); else clear(positions); }
    void set(ValueType positions) { _val |= positions; }
    void clear(ValueType positions) { _val &= ~positions; }
    bool isValue(ValueType positions, ValueType values) const { return (_val & positions) == (values & positions); }
    bool isSet(ValueType positions) const { return (_val & positions) == positions; }
    bool isUnset(ValueType positions) const { return (_val & positions) == 0; }
    bool isValid(ValueType positions) const { return true; }
    ValueType asNumber() const { return _val; }
    ValueType validity() const { return ~static_cast<ValueType>(0); }
    std::string asString() const {
        std::string result;
        result.reserve(_names.size());
        ValueType bit = 0x80;
        for(auto c : _names) {
            if(_val & bit) { result.push_back(std::toupper(c)); } else { result.push_back('-'); }
            bit >>= 1;
        }
        return result;
    }
private:
    std::string _names;
    ValueType _val{};
};
using flags8_t = Bitfield<uint8_t>;
template<typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value, T>::type>
constexpr bool isValidInt(const Integer<T>& t) { return true; }
template<typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value, T>::type>
constexpr T asNativeInt(const Integer<T>& t) { return t.asNative(); }

}

