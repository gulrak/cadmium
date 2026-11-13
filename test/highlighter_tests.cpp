#include <doctest/doctest.h>
#include <highlighter.hpp>

#include <string>
#include <vector>

namespace {

// The style of each byte of the line as a letter
std::string roles(const Highlighter& highlighter, std::string_view line)
{
    std::vector<uint8_t> styles(line.size(), 0);
    highlighter.highlightLine(line, 0, styles);
    std::string result;
    for (const auto style : styles) {
        switch (style) {
            case Highlighter::eOPCODE: result += 'o'; break;
            case Highlighter::eNUMBER: result += 'n'; break;
            case Highlighter::eREGISTER: result += 'r'; break;
            case Highlighter::eLABEL: result += 'l'; break;
            case Highlighter::eCOMMENT: result += 'c'; break;
            case Highlighter::eDIRECTIVE: result += 'd'; break;
            case Highlighter::eSTRING: result += 's'; break;
            case Highlighter::eNORMAL: result += '.'; break;
            default: result += '?'; break;
        }
    }
    return result;
}

}  // namespace

TEST_CASE("Highlighter gets past control characters in a line")
{
    Highlighter highlighter;
    highlighter.setDialect(Highlighter::eCHIP8OCTO);
    for (int control = 0; control < ' '; ++control) {
        CAPTURE(control);
        // The ":asm" behind the control character only changes the state if the scan gets there.
        const auto line = std::string("jump0") + static_cast<char>(control) + ":asm cdp1802 {";
        CHECK(highlighter.nextLineState(line, 0) != 0);
        // highlighting stops at a line break, but gets through the line otherwise
        CHECK(roles(highlighter, line).starts_with("ooooo"));
    }
}

TEST_CASE("Octo source is highlighted")
{
    Highlighter highlighter;
    CHECK(roles(highlighter, ": main") == "l.llll");
    CHECK(roles(highlighter, "v0 := 0x12 # set") == "rr.oo.nnnn.ccccc");
    CHECK(roles(highlighter, "i := label") == "r.oo......");
    CHECK(roles(highlighter, ":const ab 1") == "dddddd....n");
    // multi-byte characters keep the style of their token on every byte
    CHECK(roles(highlighter, "# \xc3\xa4") == "cccc");
}

TEST_CASE("Octo :asm blocks switch the dialect")
{
    Highlighter highlighter;
    auto state = highlighter.nextLineState(":asm cdp1802 {", 0);
    CHECK(state != 0);
    std::vector<uint8_t> styles(9, 0);
    highlighter.highlightLine("LDI #12 }", state, styles);
    CHECK(styles[0] == Highlighter::eOPCODE);
    CHECK(styles[4] == Highlighter::eNORMAL);
    CHECK(styles[5] == Highlighter::eNUMBER);
    CHECK(highlighter.nextLineState("LDI #12 }", state) == 0);
}

TEST_CASE("M6800 disassembly is highlighted")
{
    Highlighter highlighter;
    highlighter.setDialect(Highlighter::eM6800);
    CHECK(roles(highlighter, "LDAA #$12") == "oooo.nnnn");
    CHECK(roles(highlighter, "STX $0024") == "ooo.nnnnn");
    CHECK(roles(highlighter, "JSR $C0C7") == "ooo.nnnnn");
    CHECK(roles(highlighter, "STAB $12,X") == "oooo.nnn.r");
    CHECK(roles(highlighter, "BSR  $C079") == "ooo..nnnnn");
    CHECK(roles(highlighter, "NOP ") == "ooo.");
    CHECK(roles(highlighter, "???") == "...");
    CHECK(roles(highlighter, "BEQ  ???") == "ooo.....");
    CHECK(roles(highlighter, "loop: INX ; next") == "lllll.ooo.cccccc");
}
