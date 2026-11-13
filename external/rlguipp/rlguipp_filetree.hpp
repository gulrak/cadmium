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
// A file tree of a folder, e.g. the files of a project
//---------------------------------------------------------------------------------------
#pragma once

#include <rlguipp/rlguipp.hpp>

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace gui {

// How the file tree shows one entry, the defaults keep the normal look
struct FileTreeMark
{
    int icon{-1};           // the icon, -1 for the normal one
    Color color{0, 0, 0, 0}; // the text color, alpha 0 for the normal one
    std::string suffix;     // a dimmer text after the name, e.g. "(main)"
};

// A tree of the folders and files below a root folder. Folders are read when they are opened the first time, refresh()
// reads the opened ones again. Hidden files (a name starting with a dot) are not shown, a filter can limit the files to
// some extensions. A click on a folder opens or closes it, a click on a file selects it and render() returns it. With the
// keyboard focus (a click gives it) the cursor keys move the selection, Left and Right close and open folders, Enter
// returns the selected file.
class FileTree
{
public:
    explicit FileTree(const char* id);

    // The folder whose content is shown, the folders opened below it are kept while it stays the same
    void setRoot(const std::string& path);
    const std::string& root() const { return _root; }
    // Only files with one of the extensions (with the dot, e.g. ".8o", without case) are shown, an empty list shows all
    void setFilter(std::vector<std::string> extensions);
    // Reads the opened folders again, entries that are gone disappear and new ones show up
    void refresh();

    // The selected entry (empty for none), select() doesn't open folders, reveal() opens the folders down to the path,
    // selects it and scrolls it into view
    const std::string& selected() const { return _selected; }
    void select(const std::string& path) { _selected = normalized(path); }
    void reveal(const std::string& path);
    bool isExpanded(const std::string& path) const;
    void setExpanded(const std::string& path, bool expanded);

    // The handler is asked for the look of each shown entry in every frame, it should be quick
    using MarkHandler = std::function<FileTreeMark(const std::string& path, bool isDirectory)>;
    void setMarkHandler(MarkHandler handler) { _markHandler = std::move(handler); }
    // A right-click selects the entry under the mouse and asks the handler for the context menu, the menu starts empty
    // and is only shown if it has items. A right-click below the entries passes the root folder. The tree must outlive
    // its open menu.
    using ContextMenuHandler = std::function<void(PopupMenu::Menu& menu, const std::string& path, bool isDirectory)>;
    void setContextMenuHandler(ContextMenuHandler handler) { _contextMenuHandler = std::move(handler); }

    void setFocus();
    bool hasFocus() const;

    // Shows the tree at the current layout position with the remaining width and the given height (<= 0 uses the
    // remaining height) and handles the input, returns the file the user picked in this frame
    std::optional<std::string> render(float height = 0);

    static constexpr float ROW_HEIGHT = 16;
    static constexpr float INDENT = 12;  // per folder level

private:
    struct Entry
    {
        std::string path;
        std::string name;
        bool isDirectory{false};
    };
    struct Folder
    {
        std::vector<Entry> entries;
        bool expanded{false};
    };
    struct Row
    {
        const Entry* entry;
        int depth;
    };
    static std::string normalized(const std::string& path);
    Folder& folder(const std::string& path);
    void readFolder(const std::string& path, Folder& folder) const;
    bool isShown(const Entry& entry) const;
    void collectRows(const std::string& path, int depth);
    int rowOf(const std::string& path) const;
    void scrollToRow(int row);
    std::optional<std::string> handleKeys();
    void openContextMenu(Vector2 position, const std::string& path, bool isDirectory);

    Id _focusId;
    std::string _id;
    std::string _root;
    std::vector<std::string> _extensions;
    std::map<std::string, Folder> _folders;
    std::vector<Row> _rows;  // the shown entries, collected in each frame
    std::string _selected;
    MarkHandler _markHandler;
    ContextMenuHandler _contextMenuHandler;
    Vector2 _scroll{};
    float _viewHeight{0};
    int _revealFrames{0};  // frames to wait for the selected row to scroll it into view
};

}  // namespace gui
