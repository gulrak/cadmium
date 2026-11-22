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
#include <rlguipp/rlguipp_filetree.hpp>
#include <rlguipp/rlguipp_detail.hpp>
#include <rlguipp/rlguipp_filedialog.hpp>
#include <ghc/fs_fwd.hpp>

#include <algorithm>
#include <cctype>

namespace gui {

namespace {

std::string toLower(std::string text)
{
    std::ranges::transform(text, text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

std::string extensionOf(const std::string& name)
{
    return toLower(ghc::filesystem::path(name).extension().string());
}

int fileIcon(const std::string& name)
{
    const auto ext = extensionOf(name);
    const auto& icons = GetDefaultFileIcons();
    const auto it = icons.find(ext.empty() ? std::string_view{} : std::string_view(ext).substr(1));
    return it != icons.end() ? it->second : ICON_FILE;
}

// The small triangle before a folder, pointing right for a closed one and down for an open one, drawn with clipped
// rectangles, so it is clipped like the text
void drawDisclosure(float x, float y, bool open, Color color)
{
    for (int i = 0; i < 4; ++i) {
        if (open)
            DrawRectangleClipped(static_cast<int>(x) + i, static_cast<int>(y) + i - 2, 7 - 2 * i, 1, color);
        else
            DrawRectangleClipped(static_cast<int>(x) + i + 1, static_cast<int>(y) + i - 3, 1, 7 - 2 * i, color);
    }
}

bool isActivated(int key)
{
    return IsKeyPressed(key) || IsKeyPressedRepeat(key);
}

}  // namespace

FileTree::FileTree(const char* id)
    : _focusId(GetId(id))
    , _id(id)
{
}

std::string FileTree::normalized(const std::string& path)
{
    if (path.empty())
        return {};
    auto result = ghc::filesystem::path(path).lexically_normal();
    if (result.has_relative_path() && !result.has_filename())
        result = result.parent_path();  // no separator at the end, but keep a root like "/"
    return result.string();
}

void FileTree::setRoot(const std::string& path)
{
    auto root = normalized(path);
    if (root == _root)
        return;
    _root = std::move(root);
    _folders.clear();
    _rows.clear();
    _selected.clear();
    _scroll = {};
    _revealFrames = 0;
}

void FileTree::setFilter(std::vector<std::string> extensions)
{
    for (auto& ext : extensions)
        ext = toLower(ext);
    _extensions = std::move(extensions);
}

void FileTree::refresh()
{
    for (auto it = _folders.begin(); it != _folders.end();) {
        std::error_code ec;
        if (!ghc::filesystem::is_directory(it->first, ec)) {
            it = _folders.erase(it);
        }
        else {
            readFolder(it->first, it->second);
            ++it;
        }
    }
}

void FileTree::readFolder(const std::string& path, Folder& folder) const
{
    std::vector<detail::DirectoryEntry> listing;
    detail::listDirectory(path, listing);
    folder.entries.clear();
    for (auto& entry : listing) {
        if (!entry.name.empty() && entry.name.front() != '.')
            folder.entries.push_back({std::move(entry.path), std::move(entry.name), entry.isDirectory});
    }
}

FileTree::Folder& FileTree::folder(const std::string& path)
{
    auto [it, inserted] = _folders.try_emplace(path);
    if (inserted)
        readFolder(path, it->second);
    return it->second;
}

bool FileTree::isShown(const Entry& entry) const
{
    return entry.isDirectory || _extensions.empty() || std::ranges::find(_extensions, extensionOf(entry.name)) != _extensions.end();
}

bool FileTree::isExpanded(const std::string& path) const
{
    const auto key = normalized(path);
    if (key == _root)
        return true;
    const auto it = _folders.find(key);
    return it != _folders.end() && it->second.expanded;
}

void FileTree::setExpanded(const std::string& path, bool expanded)
{
    const auto key = normalized(path);
    if (key.empty() || key == _root)
        return;
    if (expanded)
        folder(key).expanded = true;
    else if (auto it = _folders.find(key); it != _folders.end())
        it->second.expanded = false;
}

void FileTree::reveal(const std::string& path)
{
    _selected = normalized(path);
    if (_root.empty())
        return;
    const auto relative = ghc::filesystem::path(_selected).lexically_relative(_root);
    if (relative.empty() || *relative.begin() == "..")
        return;
    auto current = ghc::filesystem::path(_root);
    auto last = std::prev(relative.end());
    for (auto it = relative.begin(); it != last; ++it) {
        current /= *it;
        setExpanded(current.string(), true);
    }
    _revealFrames = 2;  // the opened folders show up in the next frame
}

void FileTree::setFocus()
{
    SetKeyboardFocus(_focusId);
}

bool FileTree::hasFocus() const
{
    return HasKeyboardFocus(_focusId);
}

void FileTree::collectRows(const std::string& path, int depth)
{
    for (const auto& entry : folder(path).entries) {
        if (!isShown(entry))
            continue;
        _rows.push_back({&entry, depth});
        if (entry.isDirectory && isExpanded(entry.path))
            collectRows(entry.path, depth + 1);
    }
}

int FileTree::rowOf(const std::string& path) const
{
    const auto it = std::ranges::find_if(_rows, [&path](const Row& row) { return row.entry->path == path; });
    return it != _rows.end() ? static_cast<int>(it - _rows.begin()) : -1;
}

void FileTree::scrollToRow(int row)
{
    const float top = static_cast<float>(row) * ROW_HEIGHT;
    if (top < -_scroll.y)
        _scroll.y = -top;
    else if (top + ROW_HEIGHT > -_scroll.y + _viewHeight)
        _scroll.y = -(top + ROW_HEIGHT - _viewHeight);
}

std::optional<std::string> FileTree::handleKeys()
{
    if (_rows.empty())
        return std::nullopt;
    const int count = static_cast<int>(_rows.size());
    const int current = rowOf(_selected);
    auto selectRow = [this](int row) {
        _selected = _rows[row].entry->path;
        _revealFrames = 1;
    };
    if (isActivated(KEY_DOWN)) {
        selectRow(current < 0 ? 0 : std::min(current + 1, count - 1));
    }
    else if (isActivated(KEY_UP)) {
        selectRow(current < 0 ? 0 : std::max(current - 1, 0));
    }
    else if (isActivated(KEY_HOME)) {
        selectRow(0);
    }
    else if (isActivated(KEY_END)) {
        selectRow(count - 1);
    }
    else if (isActivated(KEY_PAGE_DOWN) || isActivated(KEY_PAGE_UP)) {
        const int page = std::max(1, static_cast<int>(_viewHeight / ROW_HEIGHT) - 1);
        selectRow(std::clamp((current < 0 ? 0 : current) + (IsKeyDown(KEY_PAGE_DOWN) ? page : -page), 0, count - 1));
    }
    else if (current >= 0) {
        const auto& entry = *_rows[current].entry;
        if (isActivated(KEY_RIGHT) && entry.isDirectory) {
            if (!isExpanded(entry.path))
                setExpanded(entry.path, true);
            else if (current + 1 < count && _rows[current + 1].depth > _rows[current].depth)
                selectRow(current + 1);
        }
        else if (isActivated(KEY_LEFT)) {
            if (entry.isDirectory && isExpanded(entry.path)) {
                setExpanded(entry.path, false);
            }
            else {
                for (int row = current - 1; row >= 0; --row) {
                    if (_rows[row].depth < _rows[current].depth) {
                        selectRow(row);
                        break;
                    }
                }
            }
        }
        else if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
            if (!entry.isDirectory)
                return entry.path;
            setExpanded(entry.path, !isExpanded(entry.path));
        }
    }
    return std::nullopt;
}

void FileTree::openContextMenu(Vector2 position, const std::string& path, bool isDirectory)
{
    if (!_contextMenuHandler)
        return;
    PopupMenu::Menu menu;
    _contextMenuHandler(menu, path, isDirectory);
    if (!menu.empty())
        PopupMenu::Open(position, menu);
}

std::optional<std::string> FileTree::render(float height)
{
    std::optional<std::string> picked;
    _rows.clear();
    if (!_root.empty())
        collectRows(_root, 0);
    const bool focused = hasFocus();
    if (focused && !GuiIsLocked())
        picked = handleKeys();

    const auto font = GuiGetFont();
    const auto mouse = GetMousePosition();
    const auto normalText = GetColor(GetStyle(LISTVIEW, TEXT_COLOR_NORMAL));
    const auto hoverText = GetColor(GetStyle(LISTVIEW, TEXT_COLOR_FOCUSED));
    const auto selectedText = GetColor(GetStyle(LISTVIEW, TEXT_COLOR_PRESSED));
    const auto selectedBase = GetColor(GetStyle(LISTVIEW, focused ? BASE_COLOR_PRESSED : BASE_COLOR_FOCUSED));
    bool onRow = false;
    Begin();
    BeginTableView(height, _id.c_str(), {TableColumn::Stretch()}, &_scroll);
    for (const auto& row : _rows) {
        const auto& entry = *row.entry;
        const bool isSelected = entry.path == _selected;
        Color textColor = normalText;
        if (!TableNextRow(ROW_HEIGHT, [&](Rectangle rect) {
                const bool hovered = !GuiIsLocked() && CheckCollisionPointRec(mouse, rect);
                if (isSelected) {
                    DrawRectangleClipped(rect.x, rect.y, rect.width, rect.height, selectedBase);
                    textColor = selectedText;
                }
                else if (hovered) {
                    DrawRectangleClipped(rect.x, rect.y, rect.width, rect.height, GetColor(GetStyle(LISTVIEW, BASE_COLOR_NORMAL)));
                    textColor = hoverText;
                }
                if (!hovered)
                    return;
                onRow = true;
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    setFocus();
                    _selected = entry.path;
                    if (entry.isDirectory)
                        setExpanded(entry.path, !isExpanded(entry.path));
                    else
                        picked = entry.path;
                }
                else if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
                    setFocus();
                    _selected = entry.path;
                    openContextMenu(mouse, entry.path, entry.isDirectory);
                }
            })) {
            continue;
        }
        TableNextColumn([&](Rectangle rect) {
            const auto mark = _markHandler ? _markHandler(entry.path, entry.isDirectory) : FileTreeMark{};
            if (mark.color.a && !isSelected)
                textColor = mark.color;
            const int icon = mark.icon >= 0 ? mark.icon : entry.isDirectory ? ICON_FOLDER : fileIcon(entry.name);
            const float x = rect.x + 2 + static_cast<float>(row.depth) * INDENT;
            if (entry.isDirectory)
                drawDisclosure(x + 1, rect.y + ROW_HEIGHT / 2, isExpanded(entry.path), textColor);
            GuiDrawIcon(icon, static_cast<int>(x + 10), static_cast<int>(rect.y), 1, textColor);
            DrawTextClipped(font, entry.name.c_str(), {x + 28, rect.y + 4}, textColor);
            if (!mark.suffix.empty())
                DrawTextClipped(font, mark.suffix.c_str(), {x + 28 + GetTextSize(entry.name.c_str()).x + 6, rect.y + 4}, ColorAlpha(textColor, 0.6f));
        });
    }
    EndTableView();
    const auto area = GetLastWidgetRect();
    End();
    _viewHeight = area.height - 2 * static_cast<float>(GuiGetStyle(DEFAULT, BORDER_WIDTH)) - 4;

    if (!GuiIsLocked() && CheckCollisionPointRec(mouse, area)) {
        if (!onRow && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            setFocus();
        else if (!onRow && IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && !_root.empty()) {
            setFocus();
            openContextMenu(mouse, _root, true);
        }
    }
    else if (focused && (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))) {
        SetKeyboardFocus(0);
    }
    if (_revealFrames > 0) {
        if (const int row = rowOf(_selected); row >= 0) {
            scrollToRow(row);
            _revealFrames = 0;
        }
        else {
            --_revealFrames;
        }
    }
    return picked;
}

}  // namespace gui
