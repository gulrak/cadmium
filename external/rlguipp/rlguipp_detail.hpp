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
// Internal helpers shared by the rlguipp sources, not part of the API
//---------------------------------------------------------------------------------------
#pragma once

#include <ghc/fs_fwd.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace gui::detail {

// The text editing of the text widgets and the code editor feels the same through these
inline constexpr double TEXT_MULTI_CLICK_TIME = 0.4;    // seconds between clicks of a double or triple click
inline constexpr double TEXT_CURSOR_BLINK_TIME = 0.53;  // seconds the cursor is shown and hidden

bool isMacLike();  // macOS, or a browser on a Mac in the web build

// Word movement stops where the class changes: line break (0), space (1), word character (2, also all non-ASCII), punctuation (3)
int charClass(int codepoint);

// The offset of the next or previous UTF-8 code point
size_t nextOffset(const std::string& text, size_t pos);
size_t prevOffset(const std::string& text, size_t pos);

// An entry of a folder, as the file dialog and the file tree list them
struct DirectoryEntry
{
    std::string path;  // the full path
    std::string name;  // the file name
    bool isDirectory{false};
    uintmax_t size{0};
    ghc::filesystem::file_time_type time{};
};
// The folders and files in a folder, folders first, then sorted by name without case, entries that can't be read are
// skipped, returns false if the folder can't be read at all
bool listDirectory(const std::string& path, std::vector<DirectoryEntry>& entries);

}  // namespace gui::detail
