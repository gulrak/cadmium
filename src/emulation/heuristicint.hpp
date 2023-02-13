//---------------------------------------------------------------------------------------
// src/emulation/heuristicint.hpp
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

#include <algorithm>
#include <cstdint>
#include <type_traits>

#include <fmt/format.h>

namespace emu {

template<typename PodType, typename = typename std::enable_if<std::is_arithmetic<PodType>::value, PodType>::type>
class HeuristicInt
{
public:
    HeuristicInt() = default;
    explicit HeuristicInt(const PodType& val) : _val(val), _valid(true) {}
    HeuristicInt(const HeuristicInt<PodType>& other) : _val(other._val), _valid(other._valid) {}

    HeuristicInt& operator=(const PodType& val) { _val = val; _valid = true; return *this; }
    HeuristicInt& operator=(const HeuristicInt<PodType>& other) { _val = other._val; _valid = other._valid; return *this; }
    HeuristicInt& operator+=(const PodType& val) { if(_valid) { _val += val; } return *this; }
    HeuristicInt& operator+=(const HeuristicInt<PodType>& other) { if (_valid && other._valid) _val += other._val; else _valid = false; return *this; }
    HeuristicInt& operator-=(const PodType& val) { if(_valid) { _val -= val; } return *this; }
    HeuristicInt& operator-=(const HeuristicInt<PodType>& other) { if (_valid && other._valid) _val -= other._val; else _valid = false; return *this; }
    HeuristicInt& operator*=(const PodType& val) { if(_valid) { _val *= val; } return *this; }
    HeuristicInt& operator*=(const HeuristicInt<PodType>& other) { if (_valid && other._valid) _val *= other._val; else _valid = false; return *this; }
    HeuristicInt& operator/=(const PodType& val) { if(_valid) { _val *= val; } return *this; }
    HeuristicInt& operator/=(const HeuristicInt<PodType>& other) { if (_valid && other._valid && other._val) _val /= other._val; else _valid = false; return *this; }

    HeuristicInt& operator&=(const PodType& val) { if(_valid) { _val &= val; } return *this; }
    HeuristicInt& operator&=(const HeuristicInt<PodType>& other) { if (_valid && other._valid) _val &= other._val; else _valid = false; return *this; }
    HeuristicInt& operator|=(const PodType& val) { if(_valid) { _val |= val; } return *this; }
    HeuristicInt& operator|=(const HeuristicInt<PodType>& other) { if (_valid && other._valid) _val |= other._val; else _valid = false; return *this; }
    HeuristicInt& operator^=(const PodType& val) { if(_valid) { _val ^= val; } return *this; }
    HeuristicInt& operator^=(const HeuristicInt<PodType>& other) { if (_valid && other._valid) _val ^= other._val; else _valid = false; return *this; }

    HeuristicInt operator+(const PodType& val) const { return {_val + val, _valid}; }
    HeuristicInt operator+(const HeuristicInt& other) const { return {_val + other.val, _valid && other._valid}; }
    HeuristicInt operator-(const PodType& val) const { return {_val - val, _valid}; }
    HeuristicInt operator-(const HeuristicInt& other) const { return {_val - other.val, _valid && other._valid}; }
    HeuristicInt operator*(const PodType& val) const { return {_val * val, _valid}; }
    HeuristicInt operator*(const HeuristicInt& other) const { return {_val * other.val, _valid && other._valid}; }
    HeuristicInt operator/(const PodType& val) const { return {val ? _val - val : 0, _valid && val}; }
    HeuristicInt operator/(const HeuristicInt& other) const { return {other._val ? _val - other.val : 0, _valid && other._valid && other._val}; }

    HeuristicInt operator~() const { return { ~_val, _valid }; }
    HeuristicInt operator&(const PodType& val) const { return {_val & val, _valid}; }
    HeuristicInt operator&(const HeuristicInt& other) const { return {_val & other.val, _valid && other._valid}; }
    HeuristicInt operator|(const PodType& val) const { return {_val | val, _valid}; }
    HeuristicInt operator|(const HeuristicInt& other) const { return {_val | other.val, _valid && other._valid}; }
    HeuristicInt operator^(const PodType& val) const { return {_val ^ val, _valid}; }
    HeuristicInt operator^(const HeuristicInt& other) const { return {_val ^ other.val, _valid && other._valid}; }

    bool operator==(const PodType& val) const { return _valid && _val == val; }
    bool operator==(const HeuristicInt& other) const { return _valid && other._valid && _val == other._val; }
    bool operator!=(const PodType& val) const { return _valid && _val != val; }
    bool operator!=(const HeuristicInt& other) const { return _valid && other._valid && _val != other._val; }
    bool operator<(const PodType& val) const { return _valid && _val < val; }
    bool operator<(const HeuristicInt& other) const { return _valid && other._valid && _val < other._val; }
    bool operator>(const PodType& val) const { return _valid && _val > val; }
    bool operator>(const HeuristicInt& other) const { return _valid && other._valid && _val > other._val; }
    bool operator<=(const PodType& val) const { return _valid && _val <= val; }
    bool operator<=(const HeuristicInt& other) const { return _valid && other._valid && _val <= other._val; }
    bool operator>=(const PodType& val) const { return _valid && _val >= val; }
    bool operator>=(const HeuristicInt& other) const { return _valid && other._valid && _val >= other._val; }

    template<typename PodType2, typename = typename std::enable_if<std::is_arithmetic<PodType2>::value, PodType2>::type>
    explicit operator HeuristicInt<PodType2>() const { return {static_cast<PodType2>(_val), _valid}; }

    bool isValid() const { return _valid; }
    PodType asNative() const { return _val; }

private:
    PodType _val{};
    bool _valid{false};
};

template<typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value, T>::type>
bool isValidInt(const T& t)
{
    return true;
}

template<typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value, T>::type>
bool isValidInt(const HeuristicInt<T>& h)
{
    return h.isValid();
}

template<typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value, T>::type>
T asNativeInt(const T& t)
{
    return t;
}

template<typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value, T>::type>
T asNativeInt(const HeuristicInt<T>& h)
{
    return h.asNative();
}

using h_int8_t = HeuristicInt<int8_t>;
using h_uint8_t = HeuristicInt<uint8_t>;
using h_int16_t = HeuristicInt<int16_t>;
using h_uint16_t = HeuristicInt<uint16_t>;
using h_int32_t = HeuristicInt<int32_t>;
using h_uint32_t = HeuristicInt<uint32_t>;

template<typename T, typename N>
constexpr auto powHelper(T x, N n) -> T {
    return n ? x * powHelper(x, n - 1) : 1;
};

template<typename T>
constexpr auto parseIntHelper(const char *iter, const char *end) -> T {
    size_t distance = std::distance(iter, end);
    if (distance < 1) return {};
    if (*iter < '0' || *iter > '9') return {};
    return ((*iter - '0') * powHelper(10, distance - 1)) + parseIntHelper<T>(iter + 1, end);
}


template<typename ValueType = uint8_t>
class Bitfield {
public:
    Bitfield() = delete;
    Bitfield(std::string names)
        : _names(std::move(names))
    {
    }
    Bitfield(std::string names, ValueType positions, ValueType values)
        : _names(std::move(names))
    {
        setFromVal(positions, values);
    }
    void setFromVal(ValueType positions, ValueType values)
    {
        _val = (_val & ~positions) | (values & positions);
    }

    void setFromBool(ValueType positions, bool asOnes)
    {
        if(asOnes)
            set(positions);
        else
            clear(positions);
    }

    void set(ValueType positions)
    {
        _val |= positions;
    }

    void clear(ValueType positions)
    {
        _val &= ~positions;
    }

    bool isValue(ValueType positions, ValueType values) const
    {
        return (_val & positions) == (values & positions);
    }

    bool isSet(ValueType positions) const
    {
        return (_val & positions) == positions;
    }

    bool isUnset(ValueType positions) const
    {
        return (_val & positions) == 0;
    }

    bool isValid(ValueType positions) const
    {
        return true;
    }

    ValueType asNumber() const { return _val; }
    ValueType validity() const { return ~static_cast<ValueType>(0); }
    std::string asString() const
    {
        std::string result;
        result.reserve(_names.size());
        ValueType bit = 0x80;
        for(auto c : _names) {
            if(_val & bit) {
                result.push_back(std::toupper(c));
            }
            else {
                result.push_back('-');
            }
            bit >>= 1;
        }
        return result;
    }

private:
    std::string _names;
    ValueType _val{};
};

template<typename ValueType = h_uint8_t>
class HeuristicBitfield {
public:
    HeuristicBitfield() = delete;
    HeuristicBitfield(std::string names)
        : _names(std::move(names))
    {
    }
    HeuristicBitfield(std::string names, ValueType positions, ValueType values)
        : _names(std::move(names))
    {
        setFromVal(positions, values);
    }

    void setFromVal(ValueType positions, ValueType values)
    {
        if(isValid(values)) {
            _val = (_val & ~positions) | (values & positions);
            _valid |= positions;
        }
        else {
            invalidate(positions);
        }
    }

    template<typename Expr>
    void setFromBool(ValueType positions, Expr asOnes)
    {
        if(isValid(asOnes)) {
            if (asOnes)
                set(positions);
            else
                clear(positions);
            _valid |= positions;
        }
        else {
            invalidate(positions);
        }
    }

    void set(ValueType positions)
    {
        _val |= positions;
        _valid |= positions;
    }

    void clear(ValueType positions)
    {
        _val &= ~positions;
        _valid |= positions;
    }

    bool isValue(ValueType positions, ValueType values) const
    {
        return (_val & _valid & positions) == (values & positions);
    }

    bool isSet(ValueType positions) const
    {
        return (_val & _valid & positions) == positions;
    }

    bool isUnset(ValueType positions) const
    {
        return ((_val | ~_valid) & ~positions) == 0;
    }

    bool isValid(ValueType positions) const
    {
        return (_valid & ~positions) == positions;
    }

    void invalidate(ValueType positions)
    {
        _valid &= ~positions;
    }

    ValueType asNumber() const { return _val; }
    ValueType validity() const { return _valid; }
    std::string asString() const
    {
        std::string result;
        result.reserve(_names.size());
        ValueType bit = 0x80;
        for(auto c : _names) {
            if(_valid & bit) {
                if (_val & bit) {
                    result.push_back(std::toupper(c));
                }
                else {
                    result.push_back('-');
                }
            }
            else {
                result.push_back('?');
            }
            bit >>= 1;
        }
        return result;
    }
private:
    std::string _names;
    ValueType _val{};
    ValueType _valid{};
};

}

template<typename PodType>
struct fmt::formatter<emu::HeuristicInt<PodType>> {
    int width = 0;
    char type = 'X';
    constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin()) {
        auto len = ctx.end() - ctx.begin();
        if (len > 1) {
            width = emu::parseIntHelper<int>(ctx.begin(), ctx.end() - 1);
        }
        else if(len) {
            type = *(ctx.end() - 1);
        }
        return ctx.end();
    }

    template <typename FormatContext>
    auto format(const emu::HeuristicInt<PodType>& hi, FormatContext& ctx) const -> decltype(ctx.out()) {
        if(hi.isValid()) {
            auto fmtstr = fmt::format("{{:0{}{}}}", width ? width : sizeof(PodType) / 2, type);
            return fmt::format_to(ctx.out(), fmtstr, hi.asNative());
        }
        else {
            auto fmtstr = fmt::format("{{:?<{}}}", width ? width : sizeof(PodType) / 2);
            return fmt::format_to(ctx.out(), fmtstr, "");
        }
    }
};

using flags8_t = emu::Bitfield<uint8_t>;
using h_flags8_t = emu::HeuristicBitfield<uint8_t>;