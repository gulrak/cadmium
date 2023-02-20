//---------------------------------------------------------------------------------------
// src/chiplet.cpp
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

#include <emulation/octocompiler.hpp>
#include <emulation/utility.hpp>

#include <ghc/cli.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <nothings/stb_image.h>

#include <chrono>
#include <stdexcept>

void preprocessFile(std::string inputFile, bool noLineInfo)
{
    using namespace std::chrono;
    using namespace std::chrono_literals;    emu::OctoCompiler octo;
    octo.generateLineInfos(!noLineInfo);
    try {
        auto start = steady_clock::now();
        octo.preprocessFile(inputFile);
        octo.dumpSegments(std::cout);
        std::cout << "\n";
        auto duration = duration_cast<milliseconds>(steady_clock::now() - start).count();
        std::clog << "Duration: " << duration << "ms" << std::endl;
    }
    catch(std::runtime_error& e) {
        std::cerr << "\n" << e.what() << std::endl;
        exit(1);
    }
}

int main(int argc, char* argv[])
{


    ghc::CLI cli(argc, argv);
    bool preprocess = false;
    bool noLineInfo = false;
    std::vector<std::string> includePath;
    std::vector<std::string> inputList;

    cli.option({"-P", "--preprocess"}, preprocess, "only preprocess the file and output the result");
    cli.option({"-I", "--include-path"}, includePath, "add directory to include search path");
    cli.option({"--no-line-info"}, noLineInfo, "omit generation of line info comments in the preprocessed output");
    cli.positional(inputList, "Files or directories to work on");
    cli.parse();


    for(auto inputFile : inputList) {
        if(preprocess)
            preprocessFile(inputFile, noLineInfo);
    }

    return 0;
}
