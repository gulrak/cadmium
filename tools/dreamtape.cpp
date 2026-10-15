//---------------------------------------------------------------------------------------
// tools/dreamtape.cpp
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

#include <emulation/dreamtape.hpp>
#include <chiplet/wavfile.hpp>
#include <ghc/cli.hpp>

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <stdexcept>

int main(int argc, char* argv[])
{
    try {
        for (int index = 1; index < argc; ++index) {
            const std::string_view arg(argv[index]);
            if (arg == "-h" || arg == "--help" || arg == "-?") {
                std::cout << "Usage: dreamtape [--baud N] [--length N] [--verify reference.bin] input.wav output.bin\n"
                             "DREAM6800 Kansas City cassette decoder: nominal 300 baud, 1200/2400 Hz, 8N1.\n"
                             "Uses Chiplet WAV loading (8/16-bit PCM or 32-bit float, stereo mixed to mono).\n"
                             "Convert MP3 externally first. Tapes carry no load address or checksum.\n";
                return 0;
            }
        }
        ghc::CLI cli(argc, argv);
        cli.category("DREAM6800 cassette decoding");
        std::vector<std::string> paths;
        std::string reference;
        std::string baudArgument;
        int64_t length = -1;
        cli.option({"--baud"}, baudArgument, "Override automatic baud measurement (270-330)");
        cli.option({"--length"}, length, "Require an exact payload length").range(0, INT64_MAX);
        cli.option({"--verify"}, reference, "Require byte-for-byte equality with a reference binary");
        cli.positional(paths, "Input PCM WAV and output binary");
        cli.parse();
        if (paths.size() != 2)
            throw std::runtime_error("Expected input WAV and output binary; see --help");
        const auto sameFile = [](const auto& first, const auto& second) {
            return std::filesystem::weakly_canonical(first) == std::filesystem::weakly_canonical(second)
                   || (std::filesystem::exists(first) && std::filesystem::exists(second)
                       && std::filesystem::equivalent(first, second));
        };
        if (sameFile(paths[0], paths[1]) || (!reference.empty() && sameFile(reference, paths[1])))
            throw std::runtime_error("Output must not overwrite the WAV or reference binary");
        std::optional<double> baud;
        if (!baudArgument.empty()) {
            size_t consumed = 0;
            baud = std::stod(baudArgument, &consumed);
            if (consumed != baudArgument.size())
                throw std::runtime_error("Invalid baud: " + baudArgument);
        }
        const WavFile<int16_t> wav(paths[0]);
        const auto image = emu::decodeDreamTape(wav.samples(), wav.sampleRate(), baud);
        if (length >= 0 && image.data.size() != static_cast<uint64_t>(length))
            throw std::runtime_error("Expected " + std::to_string(length) + " bytes, decoded " + std::to_string(image.data.size()));
        if (!reference.empty()) {
            std::ifstream input(reference, std::ios::binary);
            if (!input)
                throw std::runtime_error("Cannot read reference binary: " + reference);
            const std::vector<uint8_t> expected{std::istreambuf_iterator<char>(input), {}};
            if (image.data != expected)
                throw std::runtime_error("Decoded data differs from the reference binary");
        }
        std::ofstream output(paths[1], std::ios::binary);
        output.write(reinterpret_cast<const char*>(image.data.data()), image.data.size());
        output.close();
        if (!output)
            throw std::runtime_error("Cannot write output binary: " + paths[1]);
        std::cout << "Decoded " << image.data.size() << " bytes at " << std::fixed << std::setprecision(2)
                  << image.baud << " baud -> " << paths[1] << '\n';
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "dreamtape: " << error.what() << '\n';
        return 1;
    }
}
