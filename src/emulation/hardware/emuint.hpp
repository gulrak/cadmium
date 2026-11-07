
//---------------------------------------------------------------------------------------
// src/emulation/emuint.hpp
//---------------------------------------------------------------------------------------
//
// Copyright (c) 2025, Steffen Schümann <s.schuemann@pobox.com>
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
#include <concepts>
#include <compare>
#include <bit>

// Helper: select the best-fitting unsigned type for a given number of bits.
template<int Bits>
struct underlying_type_selector {
    static_assert(Bits >= 4 && Bits <= 64, "Bits must be between 4 and 64");
    using type = std::conditional_t<
        Bits <= 8, uint8_t,
        std::conditional_t<
            Bits <= 16, uint16_t,
            std::conditional_t<
                Bits <= 32, uint32_t,
                uint64_t
            >
        >
    >;
};

template<int Bits>
using uint_for_bits = typename underlying_type_selector<Bits>::type;

// Helper: choose the signed type for asSigned().
// When the bit–width exactly equals the full width of the chosen unsigned type (except for 64),
// we use the next larger signed type.
template<int Bits>
struct signed_type_for_bits {
    static_assert(Bits >= 4 && Bits <= 64, "Bits must be between 4 and 64");
    using type = std::conditional_t<
        (Bits <= 8),
            std::conditional_t<(Bits == 8), int16_t, int8_t>,
        std::conditional_t<
            (Bits <= 16),
            std::conditional_t<(Bits == 16), int32_t, int16_t>,
            std::conditional_t<
                (Bits <= 32),
                std::conditional_t<(Bits == 32), int64_t, int32_t>,
                int64_t
            >
        >
    >;
};

template<int Bits>
using as_signed_t = typename signed_type_for_bits<Bits>::type;

// -----------------------
// FastInt: always valid
// -----------------------
template<int Bits>
class FastInt {
public:
    using storage_type = uint_for_bits<Bits>;

    // Returns a mask that keeps only the lower Bits bits.
    static constexpr storage_type mask() noexcept {
        if constexpr (Bits == 64)
            return ~storage_type(0);
        else
            return (storage_type(1) << Bits) - 1;
    }

    // Default constructor (always valid, value 0)
    constexpr FastInt() noexcept : _value(0) {}

    // Constructible from any integral type.
    template<std::integral T>
    constexpr FastInt(T value) noexcept // NOLINT
        : _value(static_cast<storage_type>(value) & mask()) {}

    // Explicit conversion from a FastInt of (possibly) different bit–size.
    template<int OtherBits>
    constexpr explicit FastInt(const FastInt<OtherBits>& other) noexcept
        : _value(static_cast<storage_type>(other.value()) & mask()) {}

    // Accessor to the underlying value.
    constexpr bool isValid() const noexcept { return true; }
    constexpr storage_type value() const noexcept { return _value; }

    // Conversion to another FastInt with N bits.
    template<int N>
    constexpr FastInt<N> to() const noexcept {
        return FastInt<N>(_value);
    }

    // asSigned(): returns the two's complement interpretation.
    constexpr as_signed_t<Bits> asSigned() const noexcept {
        if constexpr (Bits == 64) {
            // Use bit_cast to reinterpret the bits.
            return std::bit_cast<int64_t>(_value);
        } else {
            using signed_t = as_signed_t<Bits>;
            storage_type threshold = storage_type(1) << (Bits - 1);
            if (_value < threshold)
                return static_cast<signed_t>(_value);
            else
                return static_cast<signed_t>(_value) - static_cast<signed_t>(storage_type(1) << Bits);
        }
    }

private:
    storage_type _value;
};

// Binary operator implementations for FastInt.
// For two FastInt objects (possibly of different bit–widths)
// the result type has a bit–width equal to the maximum.
template<int LBits, int RBits>
constexpr auto operator+(const FastInt<LBits>& lhs, const FastInt<RBits>& rhs) noexcept {
    constexpr int R = (LBits > RBits ? LBits : RBits);
    using result_t = FastInt<R>;
    return result_t(static_cast<typename result_t::storage_type>(lhs.value())
                    + static_cast<typename result_t::storage_type>(rhs.value()));
}

template<int LBits, int RBits>
constexpr auto operator-(const FastInt<LBits>& lhs, const FastInt<RBits>& rhs) noexcept {
    constexpr int R = (LBits > RBits ? LBits : RBits);
    using result_t = FastInt<R>;
    return result_t(static_cast<typename result_t::storage_type>(lhs.value())
                    - static_cast<typename result_t::storage_type>(rhs.value()));
}

template<int LBits, int RBits>
constexpr auto operator<<(const FastInt<LBits>& lhs, const FastInt<RBits>& rhs) noexcept {
    constexpr int R = (LBits > RBits ? LBits : RBits);
    using result_t = FastInt<R>;
    return result_t(static_cast<typename result_t::storage_type>(lhs.value())
                    << static_cast<typename result_t::storage_type>(rhs.value()));
}

template<int LBits, int RBits>
constexpr auto operator>>(const FastInt<LBits>& lhs, const FastInt<RBits>& rhs) noexcept {
    constexpr int R = (LBits > RBits ? LBits : RBits);
    using result_t = FastInt<R>;
    return result_t(static_cast<typename result_t::storage_type>(lhs.value())
                    >> static_cast<typename result_t::storage_type>(rhs.value()));
}

template<int LBits, int RBits>
constexpr auto operator&(const FastInt<LBits>& lhs, const FastInt<RBits>& rhs) noexcept {
    constexpr int R = (LBits > RBits ? LBits : RBits);
    using result_t = FastInt<R>;
    return result_t(lhs.value() & rhs.value());
}

template<int LBits, int RBits>
constexpr auto operator|(const FastInt<LBits>& lhs, const FastInt<RBits>& rhs) noexcept {
    constexpr int R = (LBits > RBits ? LBits : RBits);
    using result_t = FastInt<R>;
    return result_t(lhs.value() | rhs.value());
}

template<int LBits, int RBits>
constexpr auto operator^(const FastInt<LBits>& lhs, const FastInt<RBits>& rhs) noexcept {
    constexpr int R = (LBits > RBits ? LBits : RBits);
    using result_t = FastInt<R>;
    return result_t(lhs.value() ^ rhs.value());
}

// For FastInt, the three–way comparison returns a std::strong_ordering.
template<int LBits, int RBits>
constexpr std::strong_ordering operator<=>(const FastInt<LBits>& lhs, const FastInt<RBits>& rhs) noexcept {
    constexpr int R = (LBits > RBits ? LBits : RBits);
    using common_t = FastInt<R>;
    auto l = static_cast<typename common_t::storage_type>(lhs.value());
    auto r = static_cast<typename common_t::storage_type>(rhs.value());
    if (l < r)
        return std::strong_ordering::less;
    if (l > r)
        return std::strong_ordering::greater;
    return std::strong_ordering::equal;
}

// -----------------------
// OptInt: may be invalid
// -----------------------
template<int Bits>
class OptInt {
public:
    using storage_type = uint_for_bits<Bits>;

    static constexpr storage_type mask() noexcept {
        if constexpr (Bits == 64)
            return ~storage_type(0);
        else
            return (storage_type(1) << Bits) - 1;
    }

    // Default constructor: invalid.
    constexpr OptInt() noexcept : _valid(false), _value(0) {}

    // Constructible from an integral: becomes valid.
    template<std::integral T>
    constexpr OptInt(T value) noexcept // NOLINT
        : _valid(true), _value(static_cast<storage_type>(value) & mask()) {}

    // Conversion from an OptInt of different bit–width.
    template<int OtherBits>
    constexpr explicit OptInt(const OptInt<OtherBits>& other) noexcept
        : _valid(other.isValid()),
          _value(other.isValid() ? static_cast<storage_type>(other.value()) & mask() : 0) {}

    constexpr bool isValid() const noexcept { return _valid; }
    constexpr storage_type value() const noexcept { return _value; }

    // Conversion to an OptInt of another bit–width.
    template<int N>
    constexpr OptInt<N> to() const noexcept {
        if (!_valid)
            return OptInt<N>(); // remains invalid
        return OptInt<N>(_value);
    }

    // asSigned(): returns the two's complement interpretation.
    constexpr as_signed_t<Bits> asSigned() const noexcept {
        if constexpr (Bits == 64) {
            return std::bit_cast<int64_t>(_value);
        } else {
            using signed_t = as_signed_t<Bits>;
            storage_type threshold = storage_type(1) << (Bits - 1);
            if (_value < threshold)
                return static_cast<signed_t>(_value);
            else
                return static_cast<signed_t>(_value) - static_cast<signed_t>(storage_type(1) << Bits);
        }
    }

private:
    bool _valid;
    storage_type _value;
};

// For every binary operation on OptInt, if either operand is not valid, the result is invalid.
template<int LBits, int RBits>
constexpr auto operator+(const OptInt<LBits>& lhs, const OptInt<RBits>& rhs) noexcept {
    constexpr int R = (LBits > RBits ? LBits : RBits);
    using result_t = OptInt<R>;
    if (!lhs.isValid() || !rhs.isValid())
        return result_t();
    return result_t(static_cast<typename result_t::storage_type>(lhs.value())
                    + static_cast<typename result_t::storage_type>(rhs.value()));
}

template<int LBits, int RBits>
constexpr auto operator-(const OptInt<LBits>& lhs, const OptInt<RBits>& rhs) noexcept {
    constexpr int R = (LBits > RBits ? LBits : RBits);
    using result_t = OptInt<R>;
    if (!lhs.isValid() || !rhs.isValid())
        return result_t();
    return result_t(static_cast<typename result_t::storage_type>(lhs.value())
                    - static_cast<typename result_t::storage_type>(rhs.value()));
}

template<int LBits, int RBits>
constexpr auto operator<<(const OptInt<LBits>& lhs, const OptInt<RBits>& rhs) noexcept {
    constexpr int R = (LBits > RBits ? LBits : RBits);
    using result_t = OptInt<R>;
    if (!lhs.isValid() || !rhs.isValid())
        return result_t();
    return result_t(static_cast<typename result_t::storage_type>(lhs.value())
                    << static_cast<typename result_t::storage_type>(rhs.value()));
}

template<int LBits, int RBits>
constexpr auto operator>>(const OptInt<LBits>& lhs, const OptInt<RBits>& rhs) noexcept {
    constexpr int R = (LBits > RBits ? LBits : RBits);
    using result_t = OptInt<R>;
    if (!lhs.isValid() || !rhs.isValid())
        return result_t();
    return result_t(static_cast<typename result_t::storage_type>(lhs.value())
                    >> static_cast<typename result_t::storage_type>(rhs.value()));
}

template<int LBits, int RBits>
constexpr auto operator&(const OptInt<LBits>& lhs, const OptInt<RBits>& rhs) noexcept {
    constexpr int R = (LBits > RBits ? LBits : RBits);
    using result_t = OptInt<R>;
    if (!lhs.isValid() || !rhs.isValid())
        return result_t();
    return result_t(lhs.value() & rhs.value());
}

template<int LBits, int RBits>
constexpr auto operator|(const OptInt<LBits>& lhs, const OptInt<RBits>& rhs) noexcept {
    constexpr int R = (LBits > RBits ? LBits : RBits);
    using result_t = OptInt<R>;
    if (!lhs.isValid() || !rhs.isValid())
        return result_t();
    return result_t(lhs.value() | rhs.value());
}

template<int LBits, int RBits>
constexpr auto operator^(const OptInt<LBits>& lhs, const OptInt<RBits>& rhs) noexcept {
    constexpr int R = (LBits > RBits ? LBits : RBits);
    using result_t = OptInt<R>;
    if (!lhs.isValid() || !rhs.isValid())
        return result_t();
    return result_t(lhs.value() ^ rhs.value());
}

// For OptInt, the three–way comparison returns std::partial_ordering.
// If either operand is invalid, the result is unordered.
template<int LBits, int RBits>
constexpr std::partial_ordering operator<=>(const OptInt<LBits>& lhs, const OptInt<RBits>& rhs) noexcept {
    if (!lhs.isValid() || !rhs.isValid())
        return std::partial_ordering::unordered;
    constexpr int R = (LBits > RBits ? LBits : RBits);
    using common_t = OptInt<R>;
    auto l = static_cast<typename common_t::storage_type>(lhs.value());
    auto r = static_cast<typename common_t::storage_type>(rhs.value());
    if (l < r)
        return std::partial_ordering::less;
    if (l > r)
        return std::partial_ordering::greater;
    return std::partial_ordering::equivalent;
}


using FastUInt8 = FastInt<8>;
using FastUInt16 = FastInt<16>;
using FastUInt32 = FastInt<32>;

using OptUInt8 = OptInt<8>;
using OptUInt16 = OptInt<16>;
using OptUInt32 = OptInt<32>;
