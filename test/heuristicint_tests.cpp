//---------------------------------------------------------------------------------------
// test/heuristicint_tests.cpp
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

#include <doctest/doctest.h>

#include <emulation/heuristicint.hpp>
#define M6800_SPECULATIVE_SUPPORT
//#define CADMIUM_WITH_GENERIC_CPU
#include <emulation/hardware/m6800.hpp>

using namespace emu;

TEST_CASE("HeuristicInt - construction")
{
    //auto a = h_uint8_t(12);
    h_uint8_t a;
    CHECK(!a.isValid());
    CHECK(!isValidInt(a));
    h_uint8_t b{42};
    CHECK(b.isValid());
    CHECK(isValidInt(b));
    CHECK(b.asNative() == 42);
    CHECK(asNativeInt(b) == 42);
}

struct M6k8TestBus : public emu::M6800Bus<h_uint8_t, h_uint16_t>
{
    ByteType readByte(WordType addr) const override
    {
        if(!emu::isValidInt(addr))
            return {};
        return ByteType(0);
    }

    void writeByte(WordType addr, ByteType val) override
    {
        if(emu::isValidInt(addr) && addr < 4096);
    }
};

TEST_CASE("Speculative M6800")
{
    M6k8TestBus bus;
    SpeculativeM6800 cpu(bus);
}
