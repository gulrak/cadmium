//---------------------------------------------------------------------------------------
// src/emulation/hardware/hp48/g48statefile.cpp
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

#include "g48statefile.hpp"

#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string_view>

#include <zstd.h>

namespace hp48::g48statefile {

namespace {

bool looksLikeJson(std::span<const std::uint8_t> bytes)
{
    for (std::uint8_t byte : bytes) {
        if (byte == ' ' || byte == '\t' || byte == '\n' || byte == '\r') {
            continue;
        }
        return byte == '{';
    }
    return false;
}

bool hasCompressedStateMagic(std::span<const std::uint8_t> bytes)
{
    if (bytes.size() < std::size(COMPRESSED_STATE_MAGIC)) {
        return false;
    }
    for (std::size_t i = 0; i < std::size(COMPRESSED_STATE_MAGIC); ++i) {
        if (bytes[i] != COMPRESSED_STATE_MAGIC[i]) {
            return false;
        }
    }
    return true;
}

std::string decompressZstd(std::span<const std::uint8_t> bytes)
{
    if (hasCompressedStateMagic(bytes)) {
        bytes = bytes.subspan(std::size(COMPRESSED_STATE_MAGIC));
    }
    const unsigned long long contentSize = ZSTD_getFrameContentSize(bytes.data(), bytes.size());
    if (contentSize == ZSTD_CONTENTSIZE_ERROR) {
        throw std::runtime_error("state data is neither JSON nor a valid zstd frame");
    }
    if (contentSize == ZSTD_CONTENTSIZE_UNKNOWN) {
        throw std::runtime_error("zstd state frame does not declare decompressed size");
    }
    if (contentSize > static_cast<unsigned long long>(std::numeric_limits<std::size_t>::max())) {
        throw std::runtime_error("zstd state frame is too large for this host");
    }

    std::string decoded(static_cast<std::size_t>(contentSize), '\0');
    const std::size_t result = ZSTD_decompress(decoded.data(), decoded.size(), bytes.data(), bytes.size());
    if (ZSTD_isError(result)) {
        throw std::runtime_error(std::string("failed to decompress zstd state: ") + ZSTD_getErrorName(result));
    }
    decoded.resize(result);
    return decoded;
}

} // namespace

std::vector<std::uint8_t> readFileBytes(const ghc::filesystem::path& path)
{
    std::ifstream in(path.string(), std::ios::binary);
    if (!in) {
        throw std::runtime_error("failed to open state file: " + path.string());
    }
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

std::string decodeStateText(std::span<const std::uint8_t> bytes)
{
    if (looksLikeJson(bytes)) {
        return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
    }
    return decompressZstd(bytes);
}

nlohmann::json readStateJson(const ghc::filesystem::path& path)
{
    const std::vector<std::uint8_t> bytes = readFileBytes(path);
    return stateJsonFromBytes(bytes);
}

nlohmann::json stateJsonFromBytes(std::span<const std::uint8_t> bytes)
{
    return nlohmann::json::parse(decodeStateText(bytes));
}

std::vector<std::uint8_t> encodeStateJson(const nlohmann::json& state, bool compressed)
{
    const std::string json = state.dump(2) + '\n';
    if (!compressed) {
        return {json.begin(), json.end()};
    }

    const std::size_t bound = ZSTD_compressBound(json.size());
    std::vector<std::uint8_t> encoded(bound);
    const std::size_t result = ZSTD_compress(encoded.data(),
                                             encoded.size(),
                                             json.data(),
                                             json.size(),
                                             DEFAULT_ZSTD_COMPRESSION_LEVEL);
    if (ZSTD_isError(result)) {
        throw std::runtime_error(std::string("failed to compress zstd state: ") + ZSTD_getErrorName(result));
    }
    std::vector<std::uint8_t> prefixed;
    prefixed.reserve(std::size(COMPRESSED_STATE_MAGIC) + result);
    prefixed.insert(prefixed.end(), std::begin(COMPRESSED_STATE_MAGIC), std::end(COMPRESSED_STATE_MAGIC));
    prefixed.insert(prefixed.end(), encoded.begin(), encoded.begin() + static_cast<std::ptrdiff_t>(result));
    encoded = std::move(prefixed);
    return encoded;
}

void writeStateJson(const ghc::filesystem::path& path, const nlohmann::json& state, bool compressed)
{
    if (!path.parent_path().empty()) {
        ghc::filesystem::create_directories(path.parent_path());
    }
    const std::vector<std::uint8_t> bytes = encodeStateJson(state, compressed);
    std::ofstream out(path.string(), std::ios::binary);
    if (!out) {
        throw std::runtime_error("failed to open state file for writing: " + path.string());
    }
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!out) {
        throw std::runtime_error("failed to write state file: " + path.string());
    }
}

} // namespace hp48::g48statefile
