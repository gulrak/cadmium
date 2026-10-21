//---------------------------------------------------------------------------------------
// src/emulation/hardware/hp48/g48statefile.hpp
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

#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include <ghc/filesystem.hpp>
#include <nlohmann/json.hpp>

namespace hp48::g48statefile {

inline constexpr int DEFAULT_ZSTD_COMPRESSION_LEVEL = 19;
inline constexpr std::uint8_t COMPRESSED_STATE_MAGIC[] = {'G', 0x04, 0x08, 'Z'};

[[nodiscard]] std::vector<std::uint8_t> readFileBytes(const ghc::filesystem::path& path);
[[nodiscard]] std::string decodeStateText(std::span<const std::uint8_t> bytes);
[[nodiscard]] nlohmann::json readStateJson(const ghc::filesystem::path& path);
[[nodiscard]] nlohmann::json stateJsonFromBytes(std::span<const std::uint8_t> bytes);
[[nodiscard]] std::vector<std::uint8_t> encodeStateJson(const nlohmann::json& state, bool compressed);
void writeStateJson(const ghc::filesystem::path& path, const nlohmann::json& state, bool compressed);

} // namespace hp48::g48statefile
