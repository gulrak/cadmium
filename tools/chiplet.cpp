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


int main(int argc, char* argv[])
{
    using namespace std::chrono;
    using namespace std::chrono_literals;
    ghc::CLI cli(argc, argv);
    bool preprocess = false;
    bool noLineInfo = false;
    bool quiet = false;
    bool verbose = false;
    bool version = false;
    int verbosity = 1;
    int rc = 0;
    std::string outputFile;
    std::vector<std::string> includePaths;
    std::vector<std::string> inputList;

    cli.option({"-P", "--preprocess"}, preprocess, "only preprocess the file and output the result");
    cli.option({"-I", "--include-path"}, includePaths, "add directory to include search path");
    cli.option({"-o", "--output"}, outputFile, "name of output file, default stdout for preprocessor, a.out.ch8 for binary");
    cli.option({"--no-line-info"}, noLineInfo, "omit generation of line info comments in the preprocessed output");
    cli.option({"-q", "--quiet"}, quiet, "suppress all output during operation");
    cli.option({"-v", "--verbose"}, verbose, "more verbose progress output");
    cli.option({"--version"}, version, "just shows version info and exits");
    cli.positional(inputList, "Files or directories to work on");
    cli.parse();

    auto& logstream = preprocess && outputFile.empty() ? std::clog : std::cout;

    if(quiet)
        verbosity = 0;
    else if(verbose)
        verbosity = 100;

    emu::OctoCompiler compiler;
    compiler.generateLineInfos(!noLineInfo);
    compiler.setIncludePaths(includePaths);
    if(!quiet) {
        compiler.setProgressHandler([&](int verbLvl, std::string msg) {
            if (verbLvl <= verbosity) {
                logstream << std::string(verbLvl * 2 - 2, ' ') << msg << std::endl;
            }
        });
    }

    if(!quiet || version) {
        logstream << "Chiplet v" CADMIUM_VERSION ", (c) by Steffen Schümann" << std::endl;
        logstream << "C-Octo backend v1.2, (c) by John Earnest" << std::endl;
        logstream << "Preprocessor syntax based on Octopus by Tim Franssen\n" << std::endl;
        if(version)
            return 0;
        else
            logstream << "current directory: " << fs::current_path().string() << std::endl;
    }
    auto start = steady_clock::now();
    emu::CompileResult result;
    try {
        if (preprocess) {
            result = compiler.preprocessFiles(inputList);
            if(result.resultType == emu::CompileResult::eOK) {
                if (outputFile.empty())
                    compiler.dumpSegments(std::cout);
                else {
                    std::ofstream out(outputFile);
                    compiler.dumpSegments(out);
                }
            }
        }
        else {
            result = compiler.compile(inputList);
            if(result.resultType == emu::CompileResult::eOK) {
                if (outputFile.empty())
                    outputFile = "a.out.ch8";
                std::ofstream out(outputFile, std::ios::binary);
                out.write((const char*)compiler.code(), compiler.codeSize());
            }
        }
        if(result.resultType != emu::CompileResult::eOK) {
            for(auto iter = result.locations.rbegin(); iter != result.locations.rend(); ++iter) {
                switch(iter->type) {
                    case emu::CompileResult::Location::eINCLUDED:
                        std::cerr << "In file included from " << iter->file << ":" << iter->line << ":" << std::endl;
                        break;
                    case emu::CompileResult::Location::eINSTANTIATED:
                        std::cerr << "Instantiated at " << iter->file << ":" << iter->line << ":" << std::endl;
                        break;
                    default:
                        std::cerr << iter->file << ":" << iter->line << ":";
                        if(iter->column)
                            std::cerr << iter->column << ": ";
                        std::cerr << result.errorMessage << "\n" << std::endl;
                        break;
                }
            }
        }
    }
    catch(std::exception& ex) {
        std::cerr << "Internal error: " << ex.what() << std::endl;
        rc = -1;
    }
    auto duration = duration_cast<milliseconds>(steady_clock::now() - start).count();
    if(!quiet)
        logstream << "Duration: " << duration << "ms\n" << std::endl;

    return rc;
}
