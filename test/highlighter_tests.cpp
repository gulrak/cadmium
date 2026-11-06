#include <doctest/doctest.h>
#include <highlighter.hpp>

#include <string>

namespace {

class TestHighlighter : public Highlighter
{
public:
    using Highlighter::eCHIP8OCTO;
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
