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
#include <rlguipp/rlguipp_filedialog.hpp>
#include <ghc/fs_fwd.hpp>

#include <algorithm>
#include <cassert>
#include <ctime>

#include "syshelper.hpp"

namespace gui {

static std::string formatFileTimeIso8601(const std::chrono::time_point<std::chrono::system_clock>& ftime) {
    // Convert the time_point to time_t.
    std::time_t cftime = std::chrono::system_clock::to_time_t(ftime);
    // Convert to local time std::tm.
    std::tm tm = *std::localtime(&cftime);
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm);
    return buffer;
}

static std::vector<std::string> g_lruList;
static std::vector<const char*> g_lruPtrs;
static ssize_t g_lruIndex = -1;

static void updateLRUList()
{
    g_lruPtrs.clear();
    for (const auto& p : g_lruList) {
        g_lruPtrs.push_back(p.c_str());
    }
    g_lruPtrs.push_back(nullptr);
}

static void smartInsert(const std::string& str, ssize_t index, ssize_t maxSize) {
    assert(index <= g_lruList.size());
    if (index == g_lruList.size()) {
        if (std::find(g_lruList.begin(), g_lruList.end(), str) != g_lruList.end()) {
            return;
        }
        g_lruList.push_back(str);
    } else {
        if (std::find(g_lruList.begin(), g_lruList.begin() + index + 1, str) != g_lruList.begin() + index + 1) {
            return;
        }
        auto removeStart = g_lruList.begin() + index;
        g_lruList.erase(std::remove(removeStart, g_lruList.end(), str), g_lruList.end());
        g_lruList.insert(g_lruList.begin() + index, str);
    }
    if (g_lruList.size() > maxSize) {
        g_lruList.erase(g_lruList.begin() + maxSize, g_lruList.end());
    }
    updateLRUList();
}

static std::unordered_map<std::string_view, int> g_fileIconMap = {
    // Text files (documents, office, markdown, pdf, etc.)
    {"txt", ICON_FILETYPE_TEXT},
    {"pdf", ICON_FILETYPE_TEXT},
    {"doc", ICON_FILETYPE_TEXT},
    {"docx", ICON_FILETYPE_TEXT},
    {"xls", ICON_FILETYPE_TEXT},
    {"xlsx", ICON_FILETYPE_TEXT},
    {"ppt", ICON_FILETYPE_TEXT},
    {"pptx", ICON_FILETYPE_TEXT},
    {"odt", ICON_FILETYPE_TEXT},
    {"ods", ICON_FILETYPE_TEXT},
    {"odp", ICON_FILETYPE_TEXT},
    {"rtf", ICON_FILETYPE_TEXT},
    {"md", ICON_FILETYPE_TEXT},

    // Audio files
    {"wav", ICON_FILETYPE_AUDIO},
    {"mp3", ICON_FILETYPE_AUDIO},
    {"flac", ICON_FILETYPE_AUDIO},
    {"ogg", ICON_FILETYPE_AUDIO},
    {"aac", ICON_FILETYPE_AUDIO},

    // Video files
    {"mp4", ICON_FILETYPE_VIDEO},
    {"avi", ICON_FILETYPE_VIDEO},
    {"mkv", ICON_FILETYPE_VIDEO},
    {"mov", ICON_FILETYPE_VIDEO},
    {"wmv", ICON_FILETYPE_VIDEO},

    // Image files
    {"jpg", ICON_FILETYPE_IMAGE},
    {"jpeg", ICON_FILETYPE_IMAGE},
    {"png", ICON_FILETYPE_IMAGE},
    {"gif", ICON_FILETYPE_IMAGE},
    {"bmp", ICON_FILETYPE_IMAGE},
    {"tiff", ICON_FILETYPE_IMAGE},

    // Binary files (executables, raw binary dumps, etc.)
    {"exe", ICON_FILETYPE_BINARY},
    {"bin", ICON_FILETYPE_BINARY},
    {"raw", ICON_FILETYPE_BINARY},
    {"ram", ICON_FILETYPE_BINARY},
    {"rom", ICON_FILETYPE_BINARY}};

const std::unordered_map<std::string_view, int>& GetDefaultFileIcons()
{
    return g_fileIconMap;
}
void SetDefaultFileIcons(const std::unordered_map<std::string_view, int>& icons)
{
    g_fileIconMap = icons;
}

bool ModalFileDialog(FileDialogInfo& info, bool* isOpen)
{
    static Vector2 scroll{};
    static std::string readPath;
    static bool showLRU = false;
    static bool pathBad = false;
    static auto readDirectory = [](FileDialogInfo& fdi) {
        if (fdi.fileIcons.empty()) {
            fdi.fileIcons = g_fileIconMap;
        }
        fdi.entries.clear();
        std::error_code ec;
        if (!ghc::filesystem::exists(fdi.path, ec) || !ghc::filesystem::is_directory(fdi.path, ec)) {
            scroll = {0, 0};
            pathBad = true;
            return;
        }
        pathBad = false;
        if (ghc::filesystem::path(fdi.path).has_parent_path()) {
            fdi.entries.emplace_back("..", 0, ghc::filesystem::file_time_type{}, FileDialogInfo::DirEntry::Dir, "", ICON_FOLDER);
        }
        try {
            for (const auto& entry : ghc::filesystem::directory_iterator(fdi.path, ghc::filesystem::directory_options::skip_permission_denied)) {
                if (entry.is_directory()) {
                    fdi.entries.emplace_back(entry.path().string(), 0, entry.last_write_time(), FileDialogInfo::DirEntry::Dir, "", ICON_FOLDER);
                }
                else if (entry.is_regular_file()) {
                    auto ext = entry.path().extension().string();
                    auto it = fdi.fileIcons.find(ext.empty() ? ext : ext.substr(1));
                    fdi.entries.emplace_back(entry.path().string(), entry.file_size(), entry.last_write_time(), FileDialogInfo::DirEntry::File, entry.path().extension().string(), it != fdi.fileIcons.end() ? it->second : ICON_FILE);
                }
            }
        }
        catch (ghc::filesystem::filesystem_error& e) {
            // ignore for now
        }
        std::ranges::sort(fdi.entries, [](const FileDialogInfo::DirEntry &a, const FileDialogInfo::DirEntry &b) {
            if (a.name == "..") return true;
            if (b.name == "..") return false;
            if (a.type != b.type) {
                return a.type == FileDialogInfo::DirEntry::Dir;
            }
            return std::ranges::lexicographical_compare(a.name, b.name,
                [](unsigned char a, unsigned char b) {
                    return std::tolower(a) < std::tolower(b);
                }
            );
        });
        scroll = {0, 0};
        readPath = fdi.path;
    };
    if (info.firstFrame) {
        info.firstFrame = false;
        scroll = {0.0f, 0.0f};
        if (readPath != info.path) {
            readDirectory(info);
        }
        return false;
    }
    info.action = FileDialogInfo::None;
    const char* actionText = nullptr;
    const char* cancelText = "Cancel";
    bool actionEnabled = false;
    bool editFilename = false;
    switch (info.type) {
        case FileDialogInfo::OpenFile:
            actionText = "Open";
            actionEnabled = info.selection.size() > 0;
            break;
        case FileDialogInfo::SaveFile:
            actionText = "Save";
            editFilename = true;
            actionEnabled = !info.filename.empty() && !pathBad;
            break;
        case FileDialogInfo::SelectFolder:
            actionText = "Choose";
            actionEnabled = info.entries.size() > 0;
            break;
        case FileDialogInfo::SelectMultipleFiles:
            actionText = "Select";
            actionEnabled = info.selection.size() > 0;
            break;
    }
    auto finish = [&info, isOpen]() {
        info.entries.clear();
        info.firstFrame = true;
        readPath.clear();
        *isOpen = false;
        showLRU = false;
        return true;
    };
    if (!BeginWindowBox({-1, -1, 460, 300}, info.title.c_str(), isOpen, static_cast<WindowBoxFlags>(WBF_CLOSABLE | WBF_MOVABLE | WBF_MODAL))) {
        info.action = FileDialogInfo::Cancel;
        return finish();
    }
    BeginColumns();
    SetSpacing(0);
    SetNextWidth(GetContentAvailable().width - 20);
    if(TextBox(info.path, 4096)) {
        // TODO
    }
    auto pathBox = GetLastWidgetRect();
    pathBox.width += 20;
    SetNextWidth(20);
    bool openLRU = false;
    if (Button(GuiIconText(ICON_ARROW_DOWN, "")) && !showLRU) {
        openLRU = true;
    }
    EndColumns();
    Space(4);
    if (info.path != readPath) {
        readDirectory(info);
    }
    Begin();
    auto heightAvailable = GetContentAvailable().height;
    BeginTableView(heightAvailable - 20 - (editFilename ? 20.0f : 0.0f), 3, &scroll);
    auto font = GuiGetFont();
    static constexpr float yo = 4;
    float entryHeight = 15;
    auto normalCol = GetColor(GetStyle(LISTVIEW, TEXT_COLOR_NORMAL));
    auto focusCol = GetColor(GetStyle(LISTVIEW, TEXT_COLOR_FOCUSED));
    auto pressedCol = GetColor(GetStyle(LISTVIEW, TEXT_COLOR_PRESSED));
    for (const auto& entry : info.entries) {
        bool selected = editFilename ? ghc::filesystem::path(entry.name).filename().string() == info.filename : std::ranges::find(info.selection, entry.name) != info.selection.end();
        auto textColor = normalCol;
        if (TableNextRow(entryHeight, [&info,&entry,&textColor, selected, &pressedCol, &focusCol](Rectangle rect) {
            if (selected) {
                DrawRectangleClipped(rect.x, rect.y, rect.width, rect.height, GetColor(GetStyle(LISTVIEW, BASE_COLOR_PRESSED)));
                textColor = pressedCol;
            }
            if (!showLRU && CheckCollisionPointRec(GetMousePosition(), rect)) {
                if (!selected) {
                    DrawRectangleClipped(rect.x, rect.y, rect.width, rect.height, GetColor(GetStyle(LISTVIEW, BASE_COLOR_NORMAL)));
                    textColor = focusCol;
                }
                if (entry.type == FileDialogInfo::DirEntry::Dir && IsMouseButtonPressed(0)) {
                    if (entry.name == "..") {
                        info.path = ghc::filesystem::path(info.path).parent_path().string();
                    }
                    else {
                        info.path = (ghc::filesystem::path(info.path) / entry.name).string();
                    }
                }
                else if (entry.type == FileDialogInfo::DirEntry::File && IsMouseButtonPressed(0)) {
                    if (info.type == FileDialogInfo::SelectMultipleFiles) {
                        if (selected) {
                            info.selection.erase(std::ranges::find(info.selection, entry.name));
                        }
                        else {
                            info.selection.push_back(entry.name);
                        }
                    }
                    else {
                        info.selection.clear();
                        info.selection.push_back(entry.name);
                        info.filename = ghc::filesystem::path(entry.name).filename().string();
                    }
                }
            }
        })) {
            TableNextColumn(272, [&entry,&font,&textColor](Rectangle rect) {
                GuiDrawIcon(entry.icon, rect.x + 1, rect.y, 1, textColor);
                DrawTextClipped(font, ghc::filesystem::path(entry.name).filename().string().c_str(), {rect.x + 20, rect.y + yo}, textColor);
            });
            TableNextColumn(8*6, [&entry,&font,&textColor](Rectangle rect) {
                if (entry.type == FileDialogInfo::DirEntry::Dir) {
                    DrawTextClipped(GuiGetFont(), "  <DIR>", {rect.x + 4, rect.y + yo}, textColor);
                }
                else {
                    if (entry.size < 16384)
                        DrawTextClipped(font, TextFormat("%7d", entry.size), {rect.x + 4, rect.y + yo}, textColor);
                    else if (entry.size < 1024*1024)
                        DrawTextClipped(font, TextFormat("%6dk", entry.size/1024), {rect.x + 4, rect.y + yo}, textColor);
                    else
                        DrawTextClipped(font, TextFormat("%6dM", entry.size/1024/1024), {rect.x + 4, rect.y + yo}, textColor);
                }
            });
            TableNextColumn(20*6, [&entry,&font,&textColor](Rectangle rect) {
                if (entry.time != ghc::filesystem::file_time_type{})
                    DrawTextClipped(font, formatFileTimeIso8601(entry.time).c_str(), {rect.x + 4, rect.y + yo}, textColor);
            });
        }
    }
    EndTableView();
    if (editFilename) {
        Space(4);
        BeginColumns();
        SetSpacing(0);
        SetNextWidth(32);
        Label("Name:");
        TextBox(info.filename, 4096);
        EndColumns();
    }
    Space(4);
    BeginColumns();
    SetSpacing(0);
    auto actionWidth = GetTextSize(actionText).x + 10;
    auto cancelWidth = GetTextSize(cancelText).x + 10;
    auto indent = GetContentAvailable().width - actionWidth - cancelWidth - 10;
    Space(indent);
#ifdef WIN32
    bool actionFirst = true;
#else
    bool actionFirst = false;
#endif
    if (actionFirst) {
        SetNextWidth(actionWidth);
        if (!actionEnabled) { GuiDisable(); }
        if (Button(actionText)) {
            info.action = FileDialogInfo::Okay;
        }
        GuiEnable();
        Space(10);
        SetNextWidth(cancelWidth);
        if (Button(cancelText)) {
            info.action = FileDialogInfo::Cancel;
        }
    }
    else {
        SetNextWidth(cancelWidth);
        if (Button(cancelText)) {
            info.action = FileDialogInfo::Cancel;
        }
        Space(10);
        SetNextWidth(actionWidth);
        if (!actionEnabled) { GuiDisable(); }
        if (Button(actionText)) {
            info.action = FileDialogInfo::Okay;
        }
        GuiEnable();
    }
    EndColumns();
    End();
    if (openLRU || showLRU) {
        static int index = 0;
        static int active = -1;
        if (openLRU) {
            showLRU = true;
            active = -1;
        }
        if (g_lruIndex < 0) {
            auto dirs = getUserDirectories();
            g_lruList.push_back(dirs.home);
            if (!dirs.documents.empty()) g_lruList.push_back(dirs.documents);
            if (!dirs.downloads.empty()) g_lruList.push_back(dirs.downloads);
            g_lruIndex = g_lruList.size();
            updateLRUList();
        }
        Rectangle lruRect = {pathBox.x, pathBox.y + 16, pathBox.width, g_lruList.size() * entryHeight + 4};
        if (!openLRU && IsMouseButtonReleased(0) && !CheckCollisionPointRec(GetMousePosition(), lruRect))
            showLRU = false;
        auto h = GuiGetStyle(LISTVIEW, LIST_ITEMS_HEIGHT);
        auto s = GuiGetStyle(LISTVIEW, LIST_ITEMS_SPACING);
        auto a = GuiGetStyle(LISTVIEW, TEXT_ALIGNMENT);
        GuiSetStyle(LISTVIEW, LIST_ITEMS_HEIGHT, static_cast<int>(entryHeight - 1));
        GuiSetStyle(LISTVIEW, LIST_ITEMS_SPACING, 1);
        GuiSetStyle(LISTVIEW, TEXT_ALIGNMENT, TEXT_ALIGN_LEFT);
        DrawRectangleRec({pathBox.x + 3, pathBox.y + 19, pathBox.width, g_lruList.size() * entryHeight + 4}, {0,0,0,64});
        GuiListViewEx(lruRect, g_lruPtrs.data(), g_lruList.size(), &index, &active, nullptr);
        if (active >= 0) {
            info.path = g_lruList[active];
            showLRU = false;
        }
        DrawLineV({lruRect.x + 1, lruRect.y + 1 + g_lruIndex * entryHeight}, {lruRect.x + lruRect.width - 2, lruRect.y + 1 + g_lruIndex * entryHeight}, GetColor(GuiGetStyle(LISTVIEW, BORDER_COLOR_NORMAL)));
        GuiSetStyle(LISTVIEW, LIST_ITEMS_HEIGHT, h);
        GuiSetStyle(LISTVIEW, LIST_ITEMS_SPACING, s);
        GuiSetStyle(LISTVIEW, TEXT_ALIGNMENT, a);
    }
    EndWindowBox();
    if(IsKeyPressed(KEY_ESCAPE))
        info.action = FileDialogInfo::Cancel;
    if (info.action != FileDialogInfo::None) {
        return finish();
    }
    return false;
}

}  // namespace gui
