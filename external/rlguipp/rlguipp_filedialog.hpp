//---------------------------------------------------------------------------------------
//
// Copyright (c) 2022, Steffen Schümann <s.schuemann@pobox.com>
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
// A modal file dialog for opening and saving files or selecting folders
//---------------------------------------------------------------------------------------
#pragma once

#include <rlguipp/rlguipp.hpp>

#include <chrono>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace gui {

struct FileDialogInfo
{
    enum DialogType { OpenFile, SaveFile, SelectFolder, SelectMultipleFiles };
    enum CloseAction { None, Okay, Cancel };
    struct DirEntry
    {
        enum Type { Unknown, File, Dir };
        std::string name;
        size_t size{};
        std::chrono::time_point<std::chrono::system_clock> time;
        Type type{Unknown};
        std::string extension;
        int icon{ICON_FILE};
        DirEntry(std::string  name, size_t size, std::chrono::time_point<std::chrono::system_clock> time, Type type, std::string  extension, int icon)
            : name(std::move(name)), size(size), time(time), type(type), extension(std::move(extension)), icon(icon)
        {}
    };
    DialogType type;
    std::string title;
    std::string path;
    std::unordered_map<std::string_view, int> fileIcons;
    std::vector<DirEntry> entries;
    std::vector<std::string> filters;
    std::vector<std::string> selection;
    std::string filename;
    CloseAction action;
    bool firstFrame{true}; // don't touch
};
RLGUIPP_API const std::unordered_map<std::string_view, int>& GetDefaultFileIcons();
RLGUIPP_API void SetDefaultFileIcons(const std::unordered_map<std::string_view, int>& icons);
RLGUIPP_API bool ModalFileDialog(FileDialogInfo& info, bool* isOpen);  // Show the dialog while `*isOpen` is true, returns true when it was closed, `info.action` tells how

}  // namespace gui
