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
    CHECK(styles[3] == Highlighter::eNORMAL);
    CHECK(styles[4] == Highlighter::eNUMBER);  // the prefix is part of the number, in every language
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

TEST_CASE("CDP1802 source is highlighted")
{
    Highlighter highlighter(Highlighter::eCDP1802);
    CHECK(roles(highlighter, "START:  LDI #12 .. load") == "llllll..ooo.nnn.ccccccc");
    CHECK(roles(highlighter, "        OUT4") == "........oooo");
    CHECK(roles(highlighter, "        SEX R2 ; GLO RF") == "........ooo.rr...ooo.rr");
    CHECK(roles(highlighter, "        ORG #0200") == "........ddd.nnnnn");
    // the shorthand without a space, based numbers and text
    CHECK(roles(highlighter, "        LDI#00") == "........ooonnn");
    CHECK(roles(highlighter, "        DC X'1F',T'HI YOU'") == "........dd.nnnnn.sssssssss");
}

TEST_CASE("M6800 source is highlighted")
{
    Highlighter highlighter(Highlighter::eM6800);
    // a label in the first column, also without a colon, then the mnemonic or directive
    CHECK(roles(highlighter, "START   LDAA    #$10") == "lllll...oooo....nnnn");
    CHECK(roles(highlighter, "IRQV    EQU     $0000   ; VECTOR") == "llll....ddd.....nnnnn...cccccccc");
    CHECK(roles(highlighter, "        RTS") == "........ooo");
    // the accumulator as an operand, indexed addressing
    CHECK(roles(highlighter, "CLEAR   STA A   0,X") == "lllll...ooo.r...n.r");
    CHECK(roles(highlighter, "        LDA B   #1") == "........ooo.r...nn");
    // comment lines and strings
    CHECK(roles(highlighter, "* the display") == "ccccccccccccc");
    CHECK(roles(highlighter, "MSG     FCC     \"HI THERE\"") == "lll.....ddd.....ssssssssss");
    // a mnemonic in the first column stays one, like in the disassembly
    CHECK(roles(highlighter, "INX") == "ooo");
}

TEST_CASE("Saturn source in the ASAP syntax is highlighted")
{
    Highlighter highlighter(Highlighter::eSATURN);
    CHECK(roles(highlighter, "do_in_c:=#01160\t; in.4 c") == "lllllll..nnnnnn.cccccccc");
    CHECK(roles(highlighter, "o_end:=454") == "lllll..nnn");
    CHECK(roles(highlighter, "ref17:\tmove.p5\thexfont-ref17,c") == "llllll.ooooooo...............r");
    CHECK(roles(highlighter, "        move.a  @d0,a") == "........oooooo...rr.r");
    CHECK(roles(highlighter, "!CODE") == "ddddd");
    CHECK(roles(highlighter, "        data.s  \"READY\", 0") == "........dddddd..sssssss..n");
    CHECK(roles(highlighter, "        include \"lib.asap\"") == "........ddddddd.ssssssssss");
    CHECK(roles(highlighter, "        retclrc         ; done") == "........ooooooo.........cccccc");
}

TEST_CASE("Words a language doesn't know stay plain")
{
    // also where a mnemonic is expected, the operands are still what they are
    CHECK(roles(Highlighter(Highlighter::eSATURN), "        frobnicate.a c") == ".....................r");
    CHECK(roles(Highlighter(Highlighter::eSATURN), "        ldi #12") == "............nnn");
    CHECK(roles(Highlighter(Highlighter::eCDP1802), "        LDAA R1") == ".............rr");
    CHECK(roles(Highlighter(Highlighter::eM6800), "LOOP    MOVE    $10") == "llll............nnn");
    // Saturn character constants
    CHECK(roles(Highlighter(Highlighter::eSATURN), "        data.b  'H'") == "........dddddd..sss");
}
