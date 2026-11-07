#include <doctest/doctest.h>
#include <highlighter.hpp>

#include <string>

namespace {

class TestHighlighter : public Highlighter
{
public:
    using Highlighter::eCHIP8OCTO;
    using Highlighter::eM6800;
    using Highlighter::eNORMAL;
    using Highlighter::eNUMBER;
    using Highlighter::eOPCODE;
    using Highlighter::eREGISTER;
    using Highlighter::eLABEL;
    using Highlighter::eCOMMENT;
    // The role of each character of the last highlighted line, by comparing its color.
    std::string roles(std::string_view line)
    {
        highlightLine(line.data(), line.data() + line.size());
        std::string result;
        for (size_t i = 0; i < line.size(); ++i) {
            const auto& c = _highlighting[i].front;
            auto is = [&](int role) { return c.r == _colors[role].r && c.g == _colors[role].g && c.b == _colors[role].b; };
            result += is(eOPCODE) ? 'o' : is(eNUMBER) ? 'n' : is(eREGISTER) ? 'r' : is(eLABEL) ? 'l' : is(eCOMMENT) ? 'c' : is(eNORMAL) ? '.' : '?';
        }
        return result;
    }
    using Highlighter::highlightLine;
    using Highlighter::LineState;
    using Highlighter::scanLineState;
    using Highlighter::setDialect;
};

}  // namespace

TEST_CASE("Highlighter gets past control characters in a line")
{
    TestHighlighter highlighter;
    highlighter.setDialect(TestHighlighter::eCHIP8OCTO);
    for (int control = 0; control < ' '; ++control) {
        CAPTURE(control);
        // The ":asm" behind the control character only changes the state if the scan gets there.
        const auto line = std::string("jump0") + static_cast<char>(control) + ":asm cdp1802 {";
        const auto* begin = line.data();
        const auto* end = begin + line.size();
        CHECK(highlighter.scanLineState(begin, end, {}) != TestHighlighter::LineState{});
        CHECK(highlighter.highlightLine(begin, end) != TestHighlighter::LineState{});
    }
}

TEST_CASE("M6800 disassembly is highlighted")
{
    TestHighlighter highlighter;
    highlighter.setDialect(TestHighlighter::eM6800);
    CHECK(highlighter.roles("LDAA #$12") == "oooo.nnnn");
    CHECK(highlighter.roles("STX $0024") == "ooo.nnnnn");
    CHECK(highlighter.roles("JSR $C0C7") == "ooo.nnnnn");
    CHECK(highlighter.roles("STAB $12,X") == "oooo.nnn.r");
    CHECK(highlighter.roles("BSR  $C079") == "ooo..nnnnn");
    CHECK(highlighter.roles("NOP ") == "ooo.");
    CHECK(highlighter.roles("???") == "...");
    CHECK(highlighter.roles("BEQ  ???") == "ooo.....");
    CHECK(highlighter.roles("loop: INX ; next") == "lllll.ooo.cccccc");
}
