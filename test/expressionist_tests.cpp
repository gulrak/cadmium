//---------------------------------------------------------------------------------------
// test/expressionist-tests.cpp
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

#include <emulation/expressionist.cpp>
#include <sstream>

using namespace emu;

TEST_CASE("Expressionist basic")
{
    Expressionist exprContext;
    uint8_t v[16];
    exprContext.define("v5", &v[5]);
    v[5] = 206;
    std::ostringstream os;
    auto [expr, error] = exprContext.parseExpression("34*(5+4)-100==v5");
    expr->dump(os);
    REQUIRE(os.str() == "[B==:(206:(306:34*(9:5+4))-100),v5]");
    REQUIRE(expr->eval() == 1);
}

TEST_CASE("Expressionist isConstant")
{
    using Value = emu::Expressionist::Value;
    uint8_t a{};
    uint16_t b{};
    bool c{};
    REQUIRE(isConstant(Value(1)));
    REQUIRE(!isConstant(Value(&a)));
    REQUIRE(!isConstant(Value(&b)));
    REQUIRE(!isConstant(Value(std::function<int64_t()>())));
}