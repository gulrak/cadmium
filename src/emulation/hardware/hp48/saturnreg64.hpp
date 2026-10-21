//---------------------------------------------------------------------------------------
// src/emulation/hardware/hp48/saturnreg64.hpp
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

#pragma once

#include <cstdint>
#include <cassert>

namespace hp48 {

enum class NibbleMode { Hex, BCD };

enum class Field { P, WP, XS, X, S, M, B, W, A };

struct Span {
    int lo; // inclusive
    int hi; // inclusive
    int size() const { return hi - lo + 1; }
};

inline Span fieldSpan(Field f)
{
    assert(f != Field::P && f != Field::WP);
    switch (f) {
        case Field::XS: return {2, 2};         // exponent sign nibble
        case Field::X:  return {0, 2};         // exponent 2..0 (same as 0..2)
        case Field::S:  return {15, 15};       // mantissa sign nibble
        case Field::M:  return {3, 14};        // mantissa 14..3
        case Field::B:  return {0, 1};         // byte field 1..0
        case Field::W:  return {0, 15};        // whole reg
        case Field::A:  return {0, 4};         // address field 4..0
        default: return {0,0};
    }
}

inline Span fieldSpan(Field f, int P /*0..15*/) {
    assert(0 <= P && P < 16);
    switch (f) {
        case Field::P:  return {P, P};         // nibble indicated by P
        case Field::WP: return {0, P};         // 0..P inclusive
        case Field::XS: return {2, 2};         // exponent sign nibble
        case Field::X:  return {0, 2};         // exponent 2..0 (same as 0..2)
        case Field::S:  return {15, 15};       // mantissa sign nibble
        case Field::M:  return {3, 14};        // mantissa 14..3
        case Field::B:  return {0, 1};         // byte field 1..0
        case Field::W:  return {0, 15};        // whole reg
        case Field::A:  return {0, 4};         // address field 4..0
    }
    return {0,0};
}

class Reg64 {
public:
    uint64_t v = 0;

    Reg64() = default;
    explicit Reg64(uint64_t val) : v(val) {}

    // nibble index 0..15 (0 = least significant nibble)
    uint8_t getNibble(int i) const {
        assert(0 <= i && i < 16);
        return static_cast<uint8_t>((v >> (i * 4)) & 0xFULL);
    }

    void setNibble(int i, uint8_t nib) {
        assert(0 <= i && i < 16);
        nib &= 0xF;
        auto mask = 0xFULL << (i * 4);
        v = (v & ~mask) | (static_cast<uint64_t>(nib) << (i * 4));
    }

    static uint8_t add_nib(uint8_t a, uint8_t b, uint8_t& carry, NibbleMode mode) {
        uint16_t s = uint16_t(a) + uint16_t(b) + carry;
        if (mode == NibbleMode::Hex) {
            carry = (s >> 4) & 1;
            return uint8_t(s & 0xF);
        } else { // BCD digit 0..9 with decimal adjust
            if (s >= 10) { s -= 10; carry = 1; } else { carry = 0; }
            return uint8_t(s);
        }
    }

    static uint8_t sub_nib(uint8_t a, uint8_t b, uint8_t& borrow, NibbleMode mode) {
        int16_t d = int16_t(a) - int16_t(b) - borrow;
        if (mode == NibbleMode::Hex) {
            if (d < 0) { d += 16; borrow = 1; } else borrow = 0;
            return uint8_t(d & 0xF);
        } else { // BCD
            if (d < 0) {
                d += 10;
                borrow = 1;
                // Saturn behavior: invalid BCD result masked to low 3 bits (matches e48 Nsub)
                if (d < 0) return static_cast<uint8_t>(d) & 7;
            } else {
                borrow = 0;
            }
            return static_cast<uint8_t>(d);
        }
    }

    uint8_t addField(const Reg64& src, Span sp, uint8_t carry_in, NibbleMode mode) {
        uint8_t c = carry_in & 1;
        for (int i = sp.lo; i <= sp.hi; ++i) {
            if (mode == NibbleMode::BCD) {
                // Saturn BCD: sanitize invalid destination digits in-place before adding.
                // In-place write ensures that when src==this, src[i] also sees the sanitized value.
                uint8_t n = getNibble(i);
                if (n >= 10) setNibble(i, n & 0x7);
            }
            uint8_t r = add_nib(getNibble(i), src.getNibble(i), c, mode);
            setNibble(i, r);
        }
        return c; // carry out (0/1)
    }

    uint8_t addField(const Reg64& src, Field f, int P, uint8_t carry_in, NibbleMode mode) {
        return addField(src, fieldSpan(f, P), carry_in, mode);
    }

    uint8_t subField(const Reg64& src, Span sp, uint8_t borrow_in, NibbleMode mode) {
        uint8_t b = borrow_in & 1;
        for (int i = sp.lo; i <= sp.hi; ++i) {
            uint8_t r = sub_nib(getNibble(i), src.getNibble(i), b, mode);
            setNibble(i, r);
        }
        return b; // borrow out (0/1)
    }

    uint8_t subField(const Reg64& src, Field f, int P, uint8_t borrow_in, NibbleMode mode) {
        return subField(src, fieldSpan(f, P), borrow_in, mode);
    }

    void clearField(Span sp) {
        for (int i = sp.lo; i <= sp.hi; ++i) setNibble(i, 0);
    }

    void clearField(Field f, int P) {
        clearField(fieldSpan(f, P));
    }

    void copyField(const Reg64& src, Span sp) {
        for (int i = sp.lo; i <= sp.hi; ++i) setNibble(i, src.getNibble(i));
    }

    void copyField(const Reg64& src, Field f, int P) {
        copyField(src, fieldSpan(f, P));
    }

    int cmpField(const Reg64& other, Span sp) const {
        for (int i = sp.hi; i >= sp.lo; --i) {
            uint8_t da = getNibble(i), db = other.getNibble(i);
            if (da < db) return -1;
            if (da > db) return +1;
        }
        return 0;
    }

    int cmpField(const Reg64& other, Field f, int P) const {
        return cmpField(other, fieldSpan(f, P));
    }

    uint8_t addConst(Span sp, uint32_t imm, NibbleMode mode) {
        uint8_t c = 0;
        for (int i = sp.lo; i <= sp.hi; ++i) {
            uint8_t imm_n = uint8_t(imm & 0xF);
            imm >>= 4;
            uint8_t r = add_nib(getNibble(i), imm_n, c, mode);
            setNibble(i, r);
        }
        return c;
    }

    uint8_t addConst(Field f, int P, uint32_t imm, NibbleMode mode) {
        return addConst(fieldSpan(f, P), imm, mode);
    }
    
    // Logic operations
    void andField(const Reg64& src, Span sp) {
        for (int i = sp.lo; i <= sp.hi; ++i) setNibble(i, getNibble(i) & src.getNibble(i));
    }

    void andField(const Reg64& src, Field f, int P) {
        andField(src, fieldSpan(f, P));
    }

    void orField(const Reg64& src, Span sp) {
        for (int i = sp.lo; i <= sp.hi; ++i) setNibble(i, getNibble(i) | src.getNibble(i));
    }

    void orField(const Reg64& src, Field f, int P) {
        orField(src, fieldSpan(f, P));
    }

    void notField(Span sp, NibbleMode mode) {
        auto base =  mode == NibbleMode::Hex ? 15 : 9;
        for (int i = sp.lo; i <= sp.hi; ++i) {
            auto nibble = base - getNibble(i);
            if ((nibble & 0xF0) != 0)
                nibble &= 0x7;
            setNibble(i, nibble & 0xF);
        }
    }

    void notField(Field f, int P, NibbleMode mode) {
        notField(fieldSpan(f, P), mode);
    }

    bool negField(Span sp, NibbleMode mode) {
        int i;
        for (i = sp.lo; i <= sp.hi && getNibble(i) == 0; ++i) { }
        if (i <= sp.hi) {
            auto base = mode == NibbleMode::Hex ? 15 : 9;
            auto nibble = getNibble(i) - 1;
            while (i <= sp.hi) {
                nibble = base - nibble;
                if ((nibble & 0xF0) != 0)
                    nibble &= 0x7;
                setNibble(i, nibble);
                if (i++ < sp.hi)
                    nibble = getNibble(i);
            }
            return true;
        }
        return false;
    }

    bool negField(Field f, int P, NibbleMode mode) {
        return negField(fieldSpan(f, P), mode);
    }

    bool shiftFieldLeft(Span sp, int bits = 1) {
        if (sp.lo > sp.hi || bits <= 0) return false;
        uint64_t mask = 0;
        for (int i = sp.lo; i <= sp.hi; ++i) {
            mask |= (0xFULL << (i * 4));
        }
        auto fieldValue = v & mask;
        uint64_t shifted;
        bool lostBits;
        if (bits >= 64) {
            shifted = 0;
            lostBits = fieldValue != 0;
        } else {
            shifted = (fieldValue << bits);
            // bits in fieldValue that are "lost" are those that fall outside of the mask
            // after the shift. 
            // Also need to handle bits that are shifted out of the 64-bit value itself.
            // Let's use a simpler approach:
            // Bits are lost if (fieldValue & something) is non-zero.
            // The "something" is the mask of bits that would be shifted out of the field or out of the 64-bit value.
            
            uint64_t shiftOutMask = 0;
            // Bits that move beyond sp.hi * 4 + 3
            // The highest bit index in the field is sp.hi * 4 + 3.
            // Any bit at position p where p + bits > sp.hi * 4 + 3 is lost.
            // So p > sp.hi * 4 + 3 - bits.
            int threshold = sp.hi * 4 + 3 - bits;
            for (int i = sp.lo; i <= sp.hi; ++i) {
                for (int b = 0; b < 4; ++b) {
                    int p = i * 4 + b;
                    if (p > threshold) shiftOutMask |= (1ULL << p);
                }
            }
            lostBits = (fieldValue & shiftOutMask) != 0;
            shifted &= mask;
        }
        v = (v & ~mask) | (shifted & mask);
        return lostBits;
    }

    bool shiftFieldLeft(Field f, int P, int bits = 1) {
        return shiftFieldLeft(fieldSpan(f, P), bits);
    }

    bool shiftFieldRight(Span sp, int bits = 1) {
        if (sp.lo > sp.hi || bits <= 0) return false;
        uint64_t mask = 0;
        for (int i = sp.lo; i <= sp.hi; ++i) {
            mask |= (0xFULL << (i * 4));
        }
        auto fieldVal = v & mask;
        uint64_t shiftOutMask = 0;
        // Bits that move below sp.lo * 4
        // Any bit at position p where p - bits < sp.lo * 4 is lost.
        // So p < sp.lo * 4 + bits.
        int threshold = sp.lo * 4 + bits;
        for (int i = sp.lo; i <= sp.hi; ++i) {
            for (int b = 0; b < 4; ++b) {
                int p = i * 4 + b;
                if (p < threshold) shiftOutMask |= (1ULL << p);
            }
        }
        bool lostBits = (fieldVal & shiftOutMask) != 0;
        uint64_t shifted = (bits >= 64) ? 0 : (fieldVal >> bits);
        v = (v & ~mask) | (shifted & mask);
        return lostBits;
    }

    bool shiftFieldRight(Field f, int P, int bits = 1) {
        return shiftFieldRight(fieldSpan(f, P), bits);
    }

    void rotateFieldLeft(Span sp, int bits = 1) {
        if (sp.lo > sp.hi || bits <= 0) return;
        uint64_t mask = 0;
        for (int i = sp.lo; i <= sp.hi; ++i) {
            mask |= (0xFULL << (i * 4));
        }
        auto fieldBits = sp.size() * 4;
        bits %= fieldBits;
        if (bits == 0) return;

        auto fieldValue = (v & mask) >> (sp.lo * 4);
        uint64_t rotated;
        if (fieldBits >= 64) {
            rotated = (fieldValue << bits) | (fieldValue >> (64 - bits));
        } else {
            uint64_t fmask = (1ULL << fieldBits) - 1;
            rotated = ((fieldValue << bits) | (fieldValue >> (fieldBits - bits))) & fmask;
        }
        v = (v & ~mask) | (rotated << (sp.lo * 4));
    }

    void rotateFieldLeft(Field f, int P, int bits = 1) {
        rotateFieldLeft(fieldSpan(f, P), bits);
    }

    void rotateFieldRight(Span sp, int bits = 1) {
        if (sp.lo > sp.hi || bits <= 0) return;
        uint64_t mask = 0;
        for (int i = sp.lo; i <= sp.hi; ++i) {
            mask |= (0xFULL << (i * 4));
        }
        auto fieldBits = sp.size() * 4;
        bits %= fieldBits;
        if (bits == 0) return;

        uint64_t fieldValue = (v & mask) >> (sp.lo * 4);
        uint64_t rotated;
        if (fieldBits >= 64) {
            rotated = (fieldValue >> bits) | (fieldValue << (64 - bits));
        } else {
            uint64_t fmask = (1ULL << fieldBits) - 1;
            rotated = ((fieldValue >> bits) | (fieldValue << (fieldBits - bits))) & fmask;
        }
        v = (v & ~mask) | (rotated << (sp.lo * 4));
    }

    void rotateFieldRight(Field f, int P, int bits = 1) {
        rotateFieldRight(fieldSpan(f, P), bits);
    }

    bool isBitSet(uint8_t bit) const
    {
        return (v >> bit) & 1;
    }

    bool isBitClear(uint8_t bit) const
    {
        return !((v >> bit) & 1);
    }

    void setBit(uint8_t bit)
    {
        v |= (1ULL << bit);
    }

    void clearBit(uint8_t bit)
    {
        v &= ~(1ULL << bit);
    }
};

} // namespace hp48
