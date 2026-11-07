//---------------------------------------------------------------------------------------
// Copyright (c) 2019, Steffen Schümann <s.schuemann@pobox.com>
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its contributors
//    may be used to endorse or promote products derived from this software without
//    specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//---------------------------------------------------------------------------------------
#include <fstream>
#include <iostream>
#include <cstdint>
#include <iterator>
#include <map>
#include <set>
#include <stdexcept>
#include <vector>
#include <string>
#include <ghc/filesystem.hpp>
#include <zstd.h>

namespace fs = ghc::filesystem;

constexpr std::uint32_t RESOURCE_FLAG_ZSTD_COMPRESSED = 0x01;
constexpr int ZSTD_COMPRESSION_LEVEL = 3;
constexpr std::uint32_t MAX_PACKED_SIZE_NUMERATOR = 3;
constexpr std::uint32_t MAX_PACKED_SIZE_DENOMINATOR = 4;

struct ResourceFile {
    std::string path;
    std::vector<unsigned char> data;
    std::vector<unsigned char> storedData;
    std::uint32_t compressedSize = 0;
    std::uint32_t flags = 0;

    [[nodiscard]] std::uint32_t originalSize() const { return static_cast<std::uint32_t>(data.size()); }
    [[nodiscard]] std::uint32_t storedSize() const { return static_cast<std::uint32_t>(storedData.size()); }
    [[nodiscard]] std::uint32_t entrySize() const
    {
        return static_cast<std::uint32_t>(16 + path.size() + storedData.size());
    }
};

typedef std::map<std::string, ResourceFile> FileInfos;

// An excluded entry is a file or a folder (then everything below it), relative to the input directory.
static bool isExcluded(const std::set<std::string>& excluded, const std::string& filepath)
{
    for (const auto& entry : excluded) {
        if (filepath == entry || (filepath.size() > entry.size() && filepath.compare(0, entry.size(), entry) == 0 && filepath[entry.size()] == '/')) {
            return true;
        }
    }
    return false;
}

std::string lineBuffer;
int globalCount = 0;

bool resourceIsPacked(const ResourceFile& file)
{
    return (file.flags & RESOURCE_FLAG_ZSTD_COMPRESSED) != 0;
}

bool compressedSizeIsWorthStoring(std::size_t originalSize, std::size_t compressedSize)
{
    return compressedSize * MAX_PACKED_SIZE_DENOMINATOR <= originalSize * MAX_PACKED_SIZE_NUMERATOR;
}

std::string percentString(std::uint64_t numerator, std::uint64_t denominator)
{
    if (denominator == 0) {
        return "0.0%";
    }

    const std::uint64_t tenths = numerator * 1000 / denominator;
    return std::to_string(tenths / 10) + "." + std::to_string(tenths % 10) + "%";
}

std::string sizeReductionString(std::uint32_t originalSize, std::uint32_t storedSize)
{
    if (originalSize <= storedSize) {
        return "0.0% saved";
    }
    return percentString(originalSize - storedSize, originalSize) + " saved";
}

void writeByte(std::ostream& stream, unsigned char byte)
{
    lineBuffer += std::to_string((int)byte) + ",";
    ++globalCount;
    if (lineBuffer.length() > 75) {
        stream << lineBuffer << std::endl;
        lineBuffer.clear();
    }
}
void writeInt(std::ostream& stream, int val)
{
    writeByte(stream, (unsigned char)(val & 0xff));
    writeByte(stream, (unsigned char)((val >> 8) & 0xff));
    writeByte(stream, (unsigned char)((val >> 16) & 0xff));
    writeByte(stream, (unsigned char)((val >> 24) & 0xff));
}
void writeString(std::ostream& stream, const std::string& str)
{
    for (unsigned i = 0; i < str.size(); ++i)
        writeByte(stream, (unsigned char)(str[i]));
}

std::vector<unsigned char> readFile(const fs::path& path)
{
    std::ifstream file(path.string(), std::ios::binary);
    if (!file) {
        throw std::runtime_error("failed to open resource file: " + path.string());
    }
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

void prepareStoredData(ResourceFile& file)
{
    file.storedData = file.data;
    file.compressedSize = 0;
    file.flags = 0;

    if (file.data.empty()) {
        return;
    }

    const std::size_t bound = ZSTD_compressBound(file.data.size());
    std::vector<unsigned char> compressed(bound);
    const std::size_t result = ZSTD_compress(compressed.data(),
                                             compressed.size(),
                                             file.data.data(),
                                             file.data.size(),
                                             ZSTD_COMPRESSION_LEVEL);
    if (ZSTD_isError(result)) {
        std::clog << "    warning: zstd compression failed for '" << file.path << "': "
                  << ZSTD_getErrorName(result) << std::endl;
        return;
    }

    compressed.resize(result);
    file.compressedSize = static_cast<std::uint32_t>(compressed.size());
    if (compressedSizeIsWorthStoring(file.data.size(), compressed.size())) {
        file.storedData = std::move(compressed);
        file.flags = RESOURCE_FLAG_ZSTD_COMPRESSED;
    }
}

void printCompressionDecision(const ResourceFile& resource)
{
    std::clog << "  " << resource.path << ": ";
    if (resourceIsPacked(resource)) {
        std::clog << "packed zstd " << resource.originalSize() << " -> " << resource.storedSize()
                  << " (" << sizeReductionString(resource.originalSize(), resource.storedSize()) << ")";
    } else if (resource.compressedSize != 0) {
        std::clog << "kept verbatim " << resource.originalSize() << " bytes; zstd would be "
                  << resource.compressedSize << " ("
                  << sizeReductionString(resource.originalSize(), resource.compressedSize)
                  << ", below 25% reduction threshold)";
    } else {
        std::clog << "kept verbatim " << resource.originalSize() << " bytes";
    }
    std::clog << std::endl;
}

void printCompressionSummary(const FileInfos& files, int totalSize, int storedTotalSize)
{
    int packedCount = 0;
    int savedBytes = 0;
    for (const auto& [name, resource] : files) {
        (void)name;
        if (resourceIsPacked(resource)) {
            ++packedCount;
            savedBytes += static_cast<int>(resource.originalSize() - resource.storedSize());
        }
    }

    std::clog << "Resource compression decisions:" << std::endl;
    for (const auto& [name, resource] : files) {
        (void)name;
        printCompressionDecision(resource);
    }

    std::clog << "Resource compression summary: packed " << packedCount << " of " << files.size()
              << " files, stored " << storedTotalSize << " of " << totalSize << " bytes";
    if (savedBytes > 0) {
        std::clog << " (" << savedBytes << " bytes, " << percentString(savedBytes, totalSize) << " saved)";
    } else {
        std::clog << " (no reduction)";
    }
    std::clog << std::endl;
}

int main(int argc, char* argv[])
{
    std::clog << "starting resourcer..." << std::endl;
    FileInfos files;
    int totalSize = 0;
    int storedTotalSize = 0;
    int filenamesSize = 0;
    if (argc < 3) {
        puts("USAGE: resourcer <outputfile> <directory> [--exclude <relative file or folder>]... [--add <name> <file>]...");
        exit(-1);
    }
    std::set<std::string> excluded;
    // Generated files (e.g. the assembled HP-48SX replacement ROM) under a resource name of their own
    std::map<std::string, fs::path> added;
    for (int argIdx = 3; argIdx < argc; ++argIdx) {
        if (std::string(argv[argIdx]) == "--exclude" && argIdx + 1 < argc) {
            excluded.insert(argv[++argIdx]);
        }
        else if (std::string(argv[argIdx]) == "--add" && argIdx + 2 < argc) {
            const std::string name = argv[++argIdx];
            added[name] = argv[++argIdx];
        }
        else {
            std::cerr << "ERROR: unexpected argument '" << argv[argIdx] << "'" << std::endl;
            exit(-1);
        }
    }

    // Process all directory arguments starting from argv[2]
    //for (int argIdx = 2; argIdx < argc; ++argIdx) {
        std::string dir = argv[2];
        if (!dir.empty() && dir[dir.length() - 1] != '/' && dir[dir.length() - 1] != '\\') {
            dir += "/";
        }

        fs::path inputDir = fs::canonical(dir);
        std::clog << "Reading content of '" << inputDir.string() << "' ..." << std::endl;

        for (fs::recursive_directory_iterator dir(inputDir); dir != fs::recursive_directory_iterator(); ++dir) {
            // generic_string() so names in subfolders use '/' on every platform
            std::string filepath = dir->path().generic_string().substr(inputDir.generic_string().length() + 1);
            if (isExcluded(excluded, filepath)) {
                std::clog << "Excluding '" << filepath << "'" << std::endl;
                continue;
            }
            if (!filepath.empty() && filepath[0] != '.' && fs::is_regular_file(*dir)) {
                ResourceFile resource;
                resource.path = filepath;
                resource.data = readFile(dir->path());
                prepareStoredData(resource);
                totalSize += static_cast<int>(resource.originalSize());
                storedTotalSize += static_cast<int>(resource.storedSize());
                filenamesSize += filepath.length();
                files[filepath] = std::move(resource);
            }
        }
    //}
    for (const auto& [name, path] : added) {
        if (!fs::is_regular_file(path)) {
            std::cerr << "ERROR: file '" << path.string() << "' for resource '" << name << "' not found" << std::endl;
            exit(-1);
        }
        std::clog << "Adding '" << path.string() << "' as '" << name << "'" << std::endl;
        ResourceFile resource;
        resource.path = name;
        resource.data = readFile(path);
        prepareStoredData(resource);
        totalSize += static_cast<int>(resource.originalSize());
        storedTotalSize += static_cast<int>(resource.storedSize());
        filenamesSize += name.length();
        files[name] = std::move(resource);
    }

    std::clog << "Found " << files.size() << " files with " << totalSize
              << " bytes of data, storing " << storedTotalSize << " bytes, processing..." << std::endl;
    printCompressionSummary(files, totalSize, storedTotalSize);

    std::ofstream output(argv[1], std::ios::trunc);
    int resourceDataSize = (storedTotalSize + 4 + files.size() * 20 + filenamesSize + 1);
    output << "const int g_resourceDataSize = " << resourceDataSize << ";" << std::endl;
    output << "const unsigned char g_resourceData[" << resourceDataSize << "] = {" << std::endl;
    writeInt(output, files.size());
    int offset = 4 + files.size() * 4;
    for (FileInfos::const_iterator iter = files.begin(); iter != files.end(); ++iter) {
        writeInt(output, offset);
        offset += iter->second.entrySize();
    }
    int checkByteNum = 0;
    offset = 4 + files.size() * 4;
    for (FileInfos::const_iterator iter = files.begin(); iter != files.end(); ++iter) {
        const ResourceFile& resource = iter->second;
        std::clog << "packing '" << resource.path << "' ..." << std::endl;
        if (globalCount != offset) {
            std::clog << "    error: expected offset " << offset << " current offset " << globalCount << " !!!" << std::endl;
        }
        writeInt(output, resource.storedSize());
        writeInt(output, resource.originalSize());
        writeInt(output, resource.flags);
        writeInt(output, resource.path.length());
        writeString(output, resource.path);
        for (unsigned char byte : resource.storedData) {
            writeByte(output, byte);
            ++checkByteNum;
        }
        std::clog << " imported " << resource.path << " with " << checkByteNum << " bytes";
        if (resourceIsPacked(resource)) {
            std::clog << " zstd from " << resource.originalSize()
                      << " (" << sizeReductionString(resource.originalSize(), resource.storedSize()) << ")";
        }
        std::clog << std::endl;
        offset += resource.entrySize();
        checkByteNum = 0;
    }
    output << lineBuffer << "0" << std::endl;
    output << "};" << std::endl;
    output.close();
    std::clog << globalCount << " / " << offset << " bytes done." << std::endl;
    return 0;
}
