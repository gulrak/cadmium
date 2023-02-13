//---------------------------------------------------------------------------------------
// tests/fuzzer.hpp
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

#include "fuzzer.hpp"

#include <random>

namespace fuzz {

static std::seed_seq g_seed{3457,236};
static std::mt19937 g_rand{g_seed};
static std::uniform_int_distribution<uint8_t> g_randomByte{0,0xFF};
static std::uniform_int_distribution<uint16_t> g_randomWord{0,0xFFFF};

void rndSeed(uint64_t seed)
{
    auto seedSeq = std::seed_seq({seed});
    g_rand.seed(seedSeq);
}

uint8_t rndByte()
{
    return g_randomByte(g_rand);
}

uint16_t rndWord()
{
    return g_randomWord(g_rand);
}



}

