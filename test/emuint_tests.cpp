//---------------------------------------------------------------------------------------
// test/emuint_tests.cpp
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

#include <doctest/doctest.h>

#include <emulation/hardware/emuint.hpp>

// Tests for FastInt
TEST_CASE("FastInt default construction and basic value") {
    FastInt<4> a;
    CHECK(a.value() == 0);
}

TEST_CASE("FastInt construction from integer and mask behavior") {
    FastInt<4> a(7);
    CHECK(a.value() == 7);
    FastInt<4> b(20); // 20 mod 16 = 4
    CHECK(b.value() == 4);
}

TEST_CASE("FastInt conversion between different sizes") {
    FastInt<8> a(200);
    auto b = FastInt<4>(a);
    // 200 mod 16 = 8
    CHECK(b.value() == 8);
}

TEST_CASE("FastInt arithmetic and bitwise operators") {
    FastInt<4> a(3);
    FastInt<4> b(5);
    auto c = a + b;
    CHECK(c.value() == 8);
    auto d = b - a;
    CHECK(d.value() == 2);
    auto e = a << FastInt<4>(1); // 3 << 1 = 6
    CHECK(e.value() == 6);
    auto f = b >> FastInt<4>(1); // 5 >> 1 = 2
    CHECK(f.value() == 2);
    auto g = a & b; // 3 (0b0011) & 5 (0b0101) = 1 (0b0001)
    CHECK(g.value() == 1);
    auto h = a | b; // 3 | 5 = 7
    CHECK(h.value() == 7);
    auto i = a ^ b; // 3 ^ 5 = 6
    CHECK(i.value() == 6);
}

TEST_CASE("FastInt three-way comparison") {
    FastInt<4> a(3), b(3), c(5);
    CHECK((a <=> b) == std::strong_ordering::equal);
    CHECK((a <=> c) == std::strong_ordering::less);
    CHECK((c <=> a) == std::strong_ordering::greater);
}

TEST_CASE("FastInt to<N>() conversion") {
    FastInt<4> a(15);
    auto b = a.to<8>();
    // 15 remains 15 in lower 4 bits.
    CHECK(b.value() == 15);
}

TEST_CASE("FastInt asSigned() method") {
    FastInt<4> a(7);  // 7 is positive
    CHECK(a.asSigned() == 7);
    FastInt<4> b(15); // 15 in 4 bits should represent -1 (15 - 16 = -1)
    CHECK(b.asSigned() == -1);
}

// Tests for OptInt
TEST_CASE("OptInt default construction is invalid") {
    OptInt<4> a;
    CHECK_FALSE(a.isValid());
}

TEST_CASE("OptInt construction from integer makes it valid") {
    OptInt<4> a(7);
    CHECK(a.isValid());
    CHECK(a.value() == 7);
}

TEST_CASE("OptInt conversion between different sizes") {
    OptInt<8> a(200);
    auto b = OptInt<4>(a);
    CHECK(b.isValid());
    // 200 mod 16 = 8
    CHECK(b.value() == 8);
}

TEST_CASE("OptInt arithmetic returns invalid if any operand is invalid") {
    OptInt<4> a(3);
    OptInt<4> b; // invalid
    auto c = a + b;
    CHECK_FALSE(c.isValid());
    auto d = b - a;
    CHECK_FALSE(d.isValid());
}

TEST_CASE("OptInt arithmetic with valid operands") {
    OptInt<4> a(3);
    OptInt<4> b(5);
    auto c = a + b;
    CHECK(c.isValid());
    CHECK(c.value() == 8);
    auto d = b - a;
    CHECK(d.isValid());
    CHECK(d.value() == 2);
    auto e = a << OptInt<4>(1);
    CHECK(e.isValid());
    CHECK(e.value() == 6);
    auto f = b >> OptInt<4>(1);
    CHECK(f.isValid());
    CHECK(f.value() == 2);
    auto g = a & b;
    CHECK(g.isValid());
    CHECK(g.value() == 1);
    auto h = a | b;
    CHECK(h.isValid());
    CHECK(h.value() == 7);
    auto i = a ^ b;
    CHECK(i.isValid());
    CHECK(i.value() == 6);
}

TEST_CASE("OptInt three-way comparison with invalid operands") {
    OptInt<4> a(3);
    OptInt<4> b;
    CHECK((a <=> b) == std::partial_ordering::unordered);
    CHECK((b <=> a) == std::partial_ordering::unordered);
}

TEST_CASE("OptInt three-way comparison with valid operands") {
    OptInt<4> a(3), b(3), c(5);
    CHECK((a <=> b) == std::partial_ordering::equivalent);
    CHECK((a <=> c) == std::partial_ordering::less);
    CHECK((c <=> a) == std::partial_ordering::greater);
}

TEST_CASE("OptInt to<N>() conversion") {
    OptInt<4> a(15);
    auto b = a.to<8>();
    // a is valid so conversion remains valid.
    CHECK(b.isValid());
    CHECK(b.value() == 15);
}

TEST_CASE("OptInt asSigned() method") {
    OptInt<4> a(7);  // positive
    CHECK(a.asSigned() == 7);
    OptInt<4> b(15); // represents -1
    CHECK(b.asSigned() == -1);
}
