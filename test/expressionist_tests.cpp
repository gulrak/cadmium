//---------------------------------------------------------------------------------------
// test/expressionist_tests.cpp
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
namespace {

struct ExprFixture
{
    ExprFixture()
    {
        v[0] = 3;
        v[1] = 0x20;
        v[15] = 1;
        ctx.define("v", &v[0], 0xF);
        for (int n = 0; n < 16; ++n) {
            ctx.define(fmt::format("v{:x}", n), &v[n]);
        }
        ctx.define("i", &i);
        ctx.define("ram", std::function([](uint32_t address) -> int64_t { return address == 0x1234 ? 42 : address & 0xFF; }));
    }
    int64_t eval(std::string_view text)
    {
        auto [expr, error] = ctx.parseExpression(text);
        INFO(text, ": ", error);
        REQUIRE(expr);
        return expr->eval();
    }
    bool fails(std::string_view text) { return !ctx.parseExpression(text).first; }
    Expressionist ctx;
    uint8_t v[16]{};
    uint16_t i{0x300};
};

}

TEST_CASE_FIXTURE(ExprFixture, "Expressionist operators")
{
    CHECK(eval("v0 == 3") == 1);
    CHECK(eval("v0 != 3") == 0);
    CHECK(eval("v0!=4") == 1);
    CHECK(eval("!v0") == 0);
    CHECK(eval("!(v0 == 4)") == 1);
    CHECK(eval("v1 % 7") == 4);
    CHECK(eval("10 - 2 * 3 % 4") == 8);
    CHECK(eval("v0 / 0") == 0);
    CHECK(eval("v0 % 0") == 0);
    CHECK(eval("1 / 0") == 0);  // folded while parsing
    CHECK(eval("1 << 64") == 0);
    CHECK(eval("1 << 4") == 16);
    CHECK(eval("i > 700 && v0") == 1);
    CHECK(fails("v0 = 3"));
    CHECK(fails("v0 ? 3"));
}

TEST_CASE_FIXTURE(ExprFixture, "Expressionist number literals")
{
    CHECK(eval("0x20 == v1") == 1);
    CHECK(eval("0X1f") == 31);
    CHECK(eval("0b101") == 5);
    CHECK(eval("i == 0x300") == 1);
    CHECK(eval("0") == 0);
    CHECK(fails("0x"));
    CHECK(fails("0b102"));
    CHECK(fails("12ab"));
    CHECK(fails("99999999999999999999999"));
}

TEST_CASE_FIXTURE(ExprFixture, "Expressionist indexing")
{
    CHECK(eval("v[1]") == 0x20);
    CHECK(eval("v[0+1] == v1") == 1);
    CHECK(eval("v[17]") == 0x20);  // masked to v1, not past the array
    CHECK(eval("v[0xf]") == 1);
    CHECK(eval("ram[0x1234]") == 42);  // function bases are not masked
    CHECK(fails("v + 1"));
    CHECK(fails("v0[1]"));
    CHECK(fails("v[v]"));
}

TEST_CASE_FIXTURE(ExprFixture, "Expressionist format")
{
    CHECK(ctx.format("hit v0={v0} v1={v1:02x} i={i:04X}") == "hit v0=3 v1=20 i=0300");
    CHECK(ctx.format("plain text") == "plain text");
    CHECK(ctx.format("{{v0}} is {v0}") == "{v0} is 3");
    CHECK(ctx.format("v[0+1]={v[0+1]}") == "v[0+1]=32");
    CHECK(ctx.format("bad {nosuch}") == "bad {nosuch: Unknown identifier}");
    CHECK(ctx.format("x {v0") == "x {v0");
    CHECK(ctx.format("x } y") == "x } y");
    CHECK(ctx.format("{v0:q}").starts_with("{v0:q: "));
}

TEST_CASE_FIXTURE(ExprFixture, "Expressionist splitConditionalFormat")
{
    using CF = Expressionist::ConditionalFormat;
    auto split = [&](std::string_view text) { return ctx.splitConditionalFormat(text); };
    auto same = [](const CF& a, const CF& b) { return a.condition == b.condition && a.format == b.format; };
    CHECK(same(split("v0 == 3"), {"v0 == 3", "v0 == 3"}));
    CHECK(same(split("  v0 == 3  "), {"v0 == 3", "v0 == 3"}));
    CHECK(same(split("v0 == 3 ? hit v1={v1:02x}"), {"v0 == 3", "hit v1={v1:02x}"}));
    CHECK(same(split("0 ? never"), {"0", "never"}));
    CHECK(same(split("v0 == 3 ?"), {"v0 == 3", ""}));
    CHECK(same(split("hit v0={v0}"), {"", "hit v0={v0}"}));
    CHECK(same(split("what? {v0}"), {"", "what? {v0}"}));
    CHECK(same(split("? {v0}"), {"", "? {v0}"}));
    CHECK(same(split("plain name"), {"", "plain name"}));
    CHECK(same(split(""), {"", ""}));
}

TEST_CASE_FIXTURE(ExprFixture, "Expressionist Octo fields in placeholders")
{
    CHECK(ctx.format("{0x10:%x}") == "10");
    CHECK(ctx.format("{0x10:%2x}") == "1011");
    CHECK(ctx.format("{0x41:%3c}") == "ABC");
    CHECK(ctx.format("{0x80:%b}") == "10000000");
    CHECK(ctx.format("{0xFF:%2i}") == "65280");  // 0xFF, 0x00 big-endian
    CHECK(ctx.format("{0xFE:%4i}") == "-16842751");  // 0xFEFF0001 as int32 like Octo
    CHECK(ctx.format("{i:%x}") == "00");  // i is 0x300, ram reads its low byte
    CHECK(ctx.format("{v1:02x} {v1:%x}") == "20 20");  // fmt uses the value, % the byte at it
    CHECK(ctx.format("{0x10:%0x}").starts_with("{0x10:%0x: "));
    CHECK(ctx.format("{0x10:%q}").starts_with("{0x10:%q: "));
    CHECK(ctx.format("{0x10:%2}").starts_with("{0x10:%2: "));
    Expressionist noRam;
    CHECK(noRam.format("{1:%x}") == "{1:%x: No ram to read from}");
}

TEST_CASE_FIXTURE(ExprFixture, "Expressionist source symbols")
{
    CHECK(fails("data"));
    ctx.setSourceSymbols({{"data", 0x10}, {"SPEED", 3}, {"i", 5}});
    CHECK(eval("data + SPEED") == 19);
    CHECK(eval("i") == 0x300);  // the CPU register wins over a source symbol
    CHECK(eval("ram[data]") == 0x10);
    CHECK(ctx.format("{data:%2x} {SPEED}") == "1011 3");
    auto split = ctx.splitConditionalFormat("v0 == SPEED ? at {data:%x}");
    CHECK(split.condition == "v0 == SPEED");
    CHECK(split.format == "at {data:%x}");
    ctx.setSourceSymbols({});
    CHECK(fails("data"));
}
