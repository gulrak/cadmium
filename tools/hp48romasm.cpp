//---------------------------------------------------------------------------------------
// tools/hp48romasm.cpp
//---------------------------------------------------------------------------------------
//
// Copyright (c) 2026, Steffen Schümann <s.schuemann@pobox.com>
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
//
// Assembles an HP-48SX ROM source (ASAP syntax, "#xxxxx:" places code at an address)
// into a 256 KB packed image, as Nce1RomController loads it. The build uses it for
// Cadmium's replacement ROM (src/emulation/hardware/hp48), so it only needs AsaCpp.
//
// Usage: hp48romasm <source.asap> <image> [--listing]
//
//---------------------------------------------------------------------------------------

#include <emulation/hardware/hp48/asacpp/asacpp.hpp>

#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

namespace {

constexpr std::uint32_t ROM_NIBBLES = 0x80000;

}  // namespace

int main(int argc, char* argv[])
{
    if (argc < 3) {
        std::cerr << "Usage: hp48romasm <source.asap> <image> [--listing]\n";
        return 2;
    }
    const std::string sourcePath = argv[1];
    const std::string imagePath = argv[2];
    const bool listing = argc > 3 && std::string(argv[3]) == "--listing";
    std::ifstream in(sourcePath, std::ios::binary);
    if (!in) {
        std::cerr << "error: unable to read " << sourcePath << "\n";
        return 2;
    }
    const std::string source{std::istreambuf_iterator<char>(in), {}};
    AsaCpp assembler;
    assembler.compile(sourcePath, std::span<const char>(source.data(), source.size()));
    if (assembler.isError()) {
        std::cerr << "error: " << assembler.compileResult().errorMessage << "\n";
        return 1;
    }
    try {
        const auto image = assembler.placedCode(ROM_NIBBLES);
        std::ofstream out(imagePath, std::ios::binary);
        out.write(reinterpret_cast<const char*>(image.data()), static_cast<std::streamsize>(image.size()));
        if (!out) {
            std::cerr << "error: unable to write " << imagePath << "\n";
            return 2;
        }
    }
    catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << "\n";
        return 1;
    }
    if (listing)
        std::cout << assembler.listing();
    return 0;
}
