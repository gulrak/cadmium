
#include <chiplet/utility.hpp>
#include <fmt/format.h>
#include "hpsaturnbase.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include "../../chiplet/include/chiplet/stb_image.h"

#include <algorithm>
#include <iostream>

/* https://www.hpmuseum.org/cgi-sys/cgiwrap/hpmuseum/articles.cgi?read=1218
  HP48         Unicode
Dec   Hex    Code    Name
---------    ------------
31     1F    2026    Ellipsis
127    7F    2592    Medium Shade
128    80    2220    Measured Angle
129    81    0101    Latin Small Letter a with Macron (originally an x with Macron)
130    82    2207    Nabla
131    83    221A    Square Root
132    84    222B    Integral
133    85    03A3    Greek Capital Letter Sigma
134    86    25B6    Black Right-Pointing Triangle
135    87    03C0    Greek Small Letter Pi
136    88    2202    Partial Differential
137    89    2264    Less-Than or Equal To
138    8A    2265    Greater-Than or Equal To
139    8B    2260    Not Equal To
140    8C    03B1    Greek Small Letter Alpha
141    8D    2192    Rightwards Arrow
142    8E    2190    Leftwards Arrow
143    8F    2193    Downwards Arrow
144    90    2191    Upwards Arrow
145    91    03B3    Greek Small Letter Gamma
146    92    03B4    Greek Small Letter Delta
147    93    03B5    Greek Small Letter Epsilon
148    94    03B7    Greek Small Letter Eta
149    95    03B8    Greek Small Letter Theta
150    96    03BB    Greek Small Letter Lamda
151    97    03C1    Greek Small Letter Rho
152    98    03C3    Greek Small Letter Sigma
153    99    03C4    Greek Small Letter Tau
154    9A    03C9    Greek Small Letter Omega
155    9B    0394    Greek Capital Letter Delta
156    9C    03A0    Greek Capital Letter Pi
157    9D    03A9    Greek Capital Letter Omega
158    9E    25A0    Black Square
159    9F    221E    Infinity
*/
std::string_view getNextInstruction(std::string_view source, std::string_view::const_iterator& iter)
{
    while (iter != source.cend()) {
        // Find the position of the next newline character
        const auto end = std::find(iter, source.cend(), '\n');
        std::string_view line(&(*iter), std::distance(iter, end) - (end != iter && *(end - 1) == '\r' ? 1 : 0));
        if (const auto pos = line.find(';'); pos != std::string_view::npos) {
            line = line.substr(0, pos);
        }
        if (const auto pos = line.find(':'); pos != std::string_view::npos) {
            line = line.substr(pos + 1);
        }
        line = trim(line);
        iter = end != source.end() ? std::next(end) : end;
        if (!line.empty() && line.find('=') == std::string_view::npos) {
            return line;
        }
    }
    return {"---"};
}

int main()
{
    constexpr std::string_view magic = "HPHP48-";
    auto data = loadFile("/home/schuemann/attic/dev/c8/cadmium/c48");
    if (!std::equal(magic.begin(), magic.end(), data.begin(), [](char a, uint8_t b) { return static_cast<uint8_t>(a) == b; })) {
        std::cerr << "Not a hp object file!" << std::endl;
        exit(1);
    }
    auto reference = loadTextFile("/home/schuemann/attic/dev/c8/cadmium/c48.asap");
    //std::ranges::transform(data, data.begin(), [](uint8_t byte) { return ((byte & 0x0F) << 4) | ((byte & 0xF0) >> 4); });
    HpSaturnBase saturn{};
    saturn.loadData(0x71000, data);
    uint32_t address = 0x71010;
//    for (int i = 0; i < 16; ++i)
//        std::cout << fmt::format("{:01x}", saturn.readNibble(index + i));
//    std::cout << std::endl;
    std::cout << fmt::format("Type: {:05x}", saturn.readNibbles<5>(address)) << std::endl;
    std::cout << fmt::format("Size: {:05x}", saturn.readNibbles<5>(address)) << std::endl;
    auto refSV = std::string_view(reference);
    auto refIter = refSV.begin();
    for (int i = 0; i < 10; i++) {
        getNextInstruction(refSV, refIter); // skip data header
    }
    while (true) {
        auto refInstruction = getNextInstruction(refSV, refIter);
        auto opcodeAddress = address;
        auto [opcode, disassembly] = saturn.disassembleOpcode(address);
        if (disassembly.empty()) {
            std::cerr << "Reference: " << refInstruction << std::endl;
            break;
        }
        std::cout << fmt::format("{:5x}: {:21} {:32} - {}", opcodeAddress, opcode, disassembly, refInstruction) << std::endl;
    }
}
