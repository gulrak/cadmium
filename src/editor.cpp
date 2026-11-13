//---------------------------------------------------------------------------------------
// src/editor.cpp
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
#include <editor.hpp>
#include <stylemanager.hpp>
#include <chiplet/utility.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <fstream>
#include <limits>
#include <numeric>

namespace {

std::string lowerExtension(const std::string& file)
{
    auto extension = fs::path(file).extension().string();
    std::ranges::transform(extension, extension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return extension;
}

// Files the tree doesn't open in the editor: images, sounds, binaries and programs
bool isBinaryFile(const std::string& file)
{
    static const std::vector<std::string> binary{".png", ".gif", ".bmp", ".jpg", ".jpeg", ".tga", ".wav", ".bin", ".ram", ".ch8", ".ch10", ".hc8",
                                                 ".sc8", ".xo8", ".mc8", ".c8b", ".c8x", ".c8e", ".zip", ".pdf"};
    return std::ranges::find(binary, lowerExtension(file)) != binary.end();
}

// A file that has a NUL byte in its start is no text, whatever its extension says
bool looksLikeText(const std::string& file)
{
    std::ifstream input(file, std::ios::binary);
    char buffer[4096];
    input.read(buffer, sizeof(buffer));
    return input.gcount() >= 0 && std::find(buffer, buffer + input.gcount(), '\0') == buffer + input.gcount();
}

bool isWordStart(std::string_view text, size_t pos)
{
    if (pos == 0)
        return true;
    const auto before = static_cast<unsigned char>(text[pos - 1]);
    const auto current = static_cast<unsigned char>(text[pos]);
    return before == '/' || before == '\\' || before == '_' || before == '-' || before == '.' || before == ' ' || (std::islower(before) && std::isupper(current));
}

// The best score of the pattern's letters in order in the text from the start position, -1 if they aren't all there:
// each letter 1, at a word start 8 more, right after the previous letter 5 more, after a gap 1 less
int matchScore(std::string_view pattern, std::string_view text, size_t start)
{
    constexpr int NONE = std::numeric_limits<int>::min() / 2;
    const size_t n = text.size();
    if (pattern.empty())
        return 0;
    if (start >= n)
        return -1;
    // at[j]: the best score with the current letter at j, before: with the previous one
    std::vector<int> before(n, NONE), at(n, NONE);
    for (size_t i = 0; i < pattern.size(); ++i) {
        const auto letter = std::tolower(static_cast<unsigned char>(pattern[i]));
        int bestEarlier = NONE;  // the best previous letter at least two places before j
        for (size_t j = start; j < n; ++j) {
            if (i > 0 && j >= start + 2)
                bestEarlier = std::max(bestEarlier, before[j - 2]);
            at[j] = NONE;
            if (std::tolower(static_cast<unsigned char>(text[j])) != letter)
                continue;
            const int own = 1 + (isWordStart(text, j) ? 8 : 0);
            if (i == 0) {
                at[j] = own;
                continue;
            }
            int previous = NONE;
            if (j > start && before[j - 1] > NONE)
                previous = before[j - 1] + 5;
            if (bestEarlier > NONE)
                previous = std::max(previous, bestEarlier - 1);
            if (previous > NONE)
                at[j] = own + previous;
        }
        std::swap(before, at);
    }
    const int best = *std::ranges::max_element(before);
    return best > NONE ? best : -1;
}

std::string withoutCarriageReturns(std::string text)
{
    std::erase(text, '\r');
    return text;
}

}

Editor::Editor()
{
    updateTextStyle({});
    resetFiles(std::make_unique<SourceFile>());
    _fileTree.setMarkHandler([this](const std::string& path, bool isDirectory) { return fileMark(path, isDirectory); });
    _fileTree.setContextMenuHandler([this](gui::PopupMenu::Menu& menu, const std::string& path, bool isDirectory) { fileTreeMenu(menu, path, isDirectory); });
}

void Editor::updateTextStyle(const Font& font)
{
    _textStyle.font = font;
    _textStyle.fontSize = 8;
    _textStyle.cellWidth = 6;
    _textStyle.lineHeight = LINE_SIZE;
    _textStyle.palette.resize(Highlighter::NUM_STYLES);
    for (int i = 0; i < Highlighter::NUM_STYLES; ++i)
        _textStyle.palette[i] = gui::StyleManager::mappedColor(Highlighter::DEFAULT_PALETTE[i]);
    _textStyle.selection = gui::StyleManager::mappedColor(Highlighter::DEFAULT_SELECTION);
    for (auto& file : _files)
        file->code().setTextStyle(_textStyle);
}

// Takes the file into the open files as the current one
SourceFile& Editor::addFile(std::unique_ptr<SourceFile> file)
{
    file->code().setHighlighter(&_highlighter);
    file->code().setTextStyle(_textStyle);
    file->setHistoryDirectory(_historyDirectory);
    auto* entry = file.get();
    file->code().setLinkHandler([this, entry](const gui::CodeContext& context) { return includeLinkAt(*entry, context.offset); });
    file->code().setShortcutHandler([this]() { return _pendingNavigation != 0; });
    _files.insert(_files.begin(), std::move(file));
    return *_files.front();
}

// The folder an include of the file is relative to
std::string Editor::includeBase(const SourceFile& file) const
{
    std::error_code ec;
    if (!file.path().empty())
        return fs::path(file.path()).parent_path().string();
    return _filename.empty() ? std::string{} : fs::absolute(_filename, ec).parent_path().string();
}

// Chiplet includes these as images, binaries and sounds, all other files are sources
static bool isSourceInclude(const std::string& file)
{
    static const std::vector<std::string> binary{".png", ".gif", ".bmp", ".jpg", ".jpeg", ".tga", ".bin", ".ch8", ".wav"};
    return std::ranges::find(binary, lowerExtension(file)) == binary.end();
}

std::optional<gui::CodeLink> Editor::includeLinkAt(SourceFile& file, uint32_t offset)
{
    const auto context = file.code().contextAt(offset);
    const auto line = context.lineText;
    // the byte offset of the line start, the column counts code points
    uint32_t lineStart = context.offset;
    for (uint32_t column = 1; column < context.column && lineStart > 0; ++column) {
        do {
            --lineStart;
        } while (lineStart > 0 && (static_cast<unsigned char>(file.getText()[lineStart]) & 0xC0) == 0x80);
    }
    // ":include" as a token outside of a comment, followed by the quoted file name
    const auto comment = line.find('#');
    auto pos = line.find(":include");
    if (pos == std::string_view::npos || (comment != std::string_view::npos && comment < pos) || (pos > 0 && static_cast<unsigned char>(line[pos - 1]) > ' '))
        return std::nullopt;
    pos += 8;
    if (pos >= line.size() || static_cast<unsigned char>(line[pos]) > ' ')
        return std::nullopt;
    while (pos < line.size() && static_cast<unsigned char>(line[pos]) <= ' ')
        ++pos;
    if (pos >= line.size() || line[pos] != '"')
        return std::nullopt;
    const auto close = line.find('"', pos + 1);
    if (close == std::string_view::npos || close == pos + 1)
        return std::nullopt;
    const uint32_t start = lineStart + static_cast<uint32_t>(pos + 1);
    const uint32_t end = lineStart + static_cast<uint32_t>(close);
    const std::string name(line.substr(pos + 1, close - pos - 1));
    if (context.offset < start || context.offset >= end || !isSourceInclude(name))
        return std::nullopt;
    std::error_code ec;
    auto target = (fs::path(includeBase(file)) / name).lexically_normal().string();
    if (!fs::is_regular_file(target, ec)) {
        // found through the include paths, the last compilation knows where
        const auto included = std::ranges::find_if(includedFiles(), [&](const auto& path) { return fs::path(path).filename() == fs::path(name).filename(); });
        if (included == includedFiles().end())
            return std::nullopt;
        target = *included;
    }
    return gui::CodeLink{start, end, "Open " + fs::path(name).filename().string(),
                         [this, target]() { openFile(target); setFocus(); }};
}

bool Editor::isInBuild(const SourceFile& file) const
{
    return &file == _main || std::ranges::find(includedFiles(), file.path()) != includedFiles().end();
}

// Saves and closes all files, the given one is the main file then
void Editor::resetFiles(std::unique_ptr<SourceFile> main)
{
    const bool focused = !_files.empty() && hasFocus();
    autoSave();
    _revisionBase = contentRevision() + 1;
    _files.clear();
    _main = &addFile(std::move(main));
    if (focused)
        setFocus();
}

void Editor::setText(std::string text, const std::string& diskFile)
{
    std::error_code ec;
    const auto path = diskFile.empty() ? std::string{} : fs::absolute(diskFile, ec).lexically_normal().string();
    if (path.empty() || !isInProject(path)) {
        resetFiles(std::make_unique<SourceFile>(std::move(text)));
        if (!diskFile.empty())
            _main->setDiskFile(diskFile);
        return;
    }
    // a program of the project, the open files of the project stay open
    const bool focused = hasFocus();
    autoSave();
    text = withoutCarriageReturns(std::move(text));
    for (auto iter = _files.begin(); iter != _files.end();) {
        const auto& file = **iter;
        const bool related = isInProject(file.path()) || (!file.path().empty() && std::ranges::find(includedFiles(), file.path()) != includedFiles().end());
        const bool keep = related && (file.path() != path || file.getText() == text);
        if (keep) {
            ++iter;
            continue;
        }
        _revisionBase += file.contentRevision() + 1;
        iter = _files.erase(iter);
    }
    auto iter = std::ranges::find_if(_files, [&](const auto& file) { return file->path() == path; });
    if (iter != _files.end()) {
        _main = iter->get();
        switchTo(*_main);
        ++_revisionBase;  // the build changed, it is compiled again
    }
    else {
        auto file = std::make_unique<SourceFile>(std::move(text));
        file->setDiskFile(path);
        _main = &addFile(std::move(file));
    }
    if (focused)
        setFocus();
}

void Editor::setGeneratedText(std::string text)
{
    setText(std::move(text));
    _compiler.reset();
    _compiledSourceSha1 = {};
    _compiledRevision = contentRevision() - 1;
    _checkedRevision = _lastSeenRevision = contentRevision();
}

uint32_t Editor::contentRevision() const
{
    return std::accumulate(_files.begin(), _files.end(), _revisionBase, [](uint32_t sum, const auto& file) { return sum + file->contentRevision(); });
}

bool Editor::openFile(const std::string& path, uint32_t line, uint32_t column)
{
    std::error_code ec;
    const auto normalized = fs::absolute(path, ec).lexically_normal().string();
    if (line > 0 && currentFile().path() == normalized)
        rememberPlace(currentPlace());  // a jump in the shown file, other files are noticed by trackPlace()
    auto iter = std::ranges::find_if(_files, [&](const auto& file) { return !file->path().empty() && file->path() == normalized; });
    if (iter != _files.end()) {
        switchTo(**iter);
    }
    else {
        auto file = SourceFile::open(normalized);
        if (!file)
            return false;
        const bool focused = hasFocus();
        addFile(std::move(file));
        if (focused)
            setFocus();
    }
    if (line > 0)
        currentFile().code().goTo(line, std::max(column, 1u));
    return true;
}

void Editor::switchTo(SourceFile& file)
{
    auto iter = std::ranges::find_if(_files, [&](const auto& entry) { return entry.get() == &file; });
    if (iter == _files.end() || iter == _files.begin())
        return;
    const bool focused = hasFocus();
    std::rotate(_files.begin(), iter, iter + 1);
    if (focused)
        setFocus();
}

void Editor::closeCurrentFile()
{
    if (&currentFile() == _main)
        return;
    const bool focused = hasFocus();
    currentFile().autoSave();
    _revisionBase += currentFile().contentRevision() + 1;
    _files.erase(_files.begin());
    if (focused)
        setFocus();
}

std::string Editor::title() const
{
    const auto& current = currentFile();
    std::string title = "Editor - " + (&current == _main && current.path().empty() && !_filename.empty() ? fs::path(_filename).filename().string() : current.name());
    if (&current != _main)
        title += " (main: " + (_main->path().empty() && !_filename.empty() ? fs::path(_filename).filename().string() : _main->name()) + ")";
    if (_files.size() > 1)
        title += " [" + std::to_string(_files.size()) + " open]";
    return title;
}

void Editor::openFileMenu(Vector2 position)
{
    gui::PopupMenu::Menu menu;
    for (const auto& file : _files) {
        auto* entry = file.get();
        auto label = (entry == &currentFile() ? "> " : "  ") + entry->name() + (entry == _main ? "  (main)" : isInBuild(*entry) ? "" : "  (not in build)");
        const auto previous = _files.size() > 1 && entry == _files[1].get() ? gui::ShortcutText("Tab", gui::SHORTCUT_CTRL) : std::string{};
        menu.push_back(gui::PopupMenu::Action(label, [this, entry]() { switchTo(*entry); }, previous));
    }
    menu.push_back(gui::PopupMenu::Separator());
    auto close = gui::PopupMenu::Action("Close " + currentFile().name(), [this]() { closeCurrentFile(); }, gui::ShortcutText("W"));
    menu.push_back(&currentFile() != _main ? close : gui::PopupMenu::Disabled(close));
    // the keys of navigationRequest(), the browser keeps them for itself
#ifdef PLATFORM_WEB
    const std::string backShortcut;
    const std::string forwardShortcut;
#elif defined(__APPLE__)
    const auto backShortcut = gui::ShortcutText("←", gui::SHORTCUT_ALT | gui::SHORTCUT_SYS);
    const auto forwardShortcut = gui::ShortcutText("→", gui::SHORTCUT_ALT | gui::SHORTCUT_SYS);
#else
    const auto backShortcut = gui::ShortcutText("Left", gui::SHORTCUT_ALT);
    const auto forwardShortcut = gui::ShortcutText("Right", gui::SHORTCUT_ALT);
#endif
    auto back = gui::PopupMenu::Action("Back", [this]() { goBack(); }, backShortcut);
    menu.push_back(canGoBack() ? back : gui::PopupMenu::Disabled(back));
    auto forward = gui::PopupMenu::Action("Forward", [this]() { goForward(); }, forwardShortcut);
    menu.push_back(canGoForward() ? forward : gui::PopupMenu::Disabled(forward));
#ifndef PLATFORM_WEB
    if (_mainFileHandler) {
        const auto path = currentFile().diskFile();
        auto setMain = gui::PopupMenu::Action("Set " + currentFile().name() + " as Main File", [this, path]() { switchMain(path); });
        menu.push_back(&currentFile() != _main && lowerExtension(path) == ".8o" ? setMain : gui::PopupMenu::Disabled(setMain));
    }
    menu.push_back(gui::PopupMenu::Separator());
    menu.push_back(gui::PopupMenu::Action("Project Files", [this]() { openFileTree(); }, gui::ShortcutText("1")));
    menu.push_back(gui::PopupMenu::Action("Quick Open...", [this]() { openQuickOpen(); }, gui::ShortcutText("P")));
    // a new file goes next to the current one, or into the folder of the tree
    const auto folder = !currentFile().path().empty() ? fs::path(currentFile().path()).parent_path().string() : treeRoot();
    auto newFile = gui::PopupMenu::Action("New File...", [this, folder]() { startNewFile(folder); });
    menu.push_back(folder.empty() ? gui::PopupMenu::Disabled(newFile) : newFile);
    if (_openFolderHandler)
        menu.push_back(gui::PopupMenu::Action("Open Folder...", [this]() { _openFolderHandler(); }));
#endif
    gui::PopupMenu::Open(position, menu);
}

void Editor::setProjectFolder(const std::string& path)
{
    std::error_code ec;
    auto folder = path.empty() ? fs::path() : fs::absolute(path, ec).lexically_normal();
    if (folder.has_relative_path() && !folder.has_filename())
        folder = folder.parent_path();
    _projectFolder = folder.string();
}

bool Editor::isInProject(const std::string& path) const
{
    if (_projectFolder.empty() || path.empty())
        return false;
    const auto relative = fs::path(path).lexically_relative(_projectFolder);
    return !relative.empty() && *relative.begin() != ".." && relative != ".";
}

std::string Editor::treeRoot() const
{
    if (!_projectFolder.empty())
        return _projectFolder;
    if (!_main->path().empty())
        return fs::path(_main->path()).parent_path().string();
    if (!currentFile().path().empty())
        return fs::path(currentFile().path()).parent_path().string();
    return {};
}

void Editor::openFileTree()
{
    _fileTree.setRoot(treeRoot());
    _fileTree.refresh();
    if (!currentFile().path().empty())
        _fileTree.reveal(currentFile().path());
    _fileTree.setFocus();
    _quickOpenOpen = false;
    _fileTreeOpen = true;
}

gui::FileTreeMark Editor::fileMark(const std::string& path, bool isDirectory) const
{
    gui::FileTreeMark mark;
    if (isDirectory)
        return mark;
    if (path == _main->path())
        mark.suffix = "(main)";
    else if (std::ranges::find(includedFiles(), path) != includedFiles().end())
        mark.suffix = "(in build)";
    if (std::ranges::any_of(_files, [&](const auto& file) { return file->path() == path; }))
        mark.color = gui::StyleManager::mappedColor(Highlighter::DEFAULT_PALETTE[Highlighter::eLABEL]);
    else if (isBinaryFile(path))
        mark.color = GetColor(gui::GetStyle(DEFAULT, TEXT_COLOR_DISABLED));
    return mark;
}

void Editor::fileTreeMenu(gui::PopupMenu::Menu& menu, const std::string& path, bool isDirectory)
{
    using namespace gui;
    if (!isDirectory) {
        auto open = PopupMenu::Action("Open", [this, path]() { pickFile(path); });
        menu.push_back(isBinaryFile(path) ? PopupMenu::Disabled(open) : open);
        if (_mainFileHandler) {
            auto setMain = PopupMenu::Action("Set as Main File", [this, path]() {
                _fileTreeOpen = false;
                switchMain(path);
            });
            menu.push_back(lowerExtension(path) == ".8o" && path != _main->path() ? setMain : PopupMenu::Disabled(setMain));
        }
        menu.push_back(PopupMenu::Separator());
    }
    const auto folder = isDirectory ? path : fs::path(path).parent_path().string();
    menu.push_back(PopupMenu::Action("New File...", [this, folder]() { startNewFile(folder); }));
    menu.push_back(PopupMenu::Action("Refresh", [this]() { _fileTree.refresh(); }));
    if (_openFolderHandler) {
        menu.push_back(PopupMenu::Separator());
        menu.push_back(PopupMenu::Action("Open Folder...", [this]() {
            _fileTreeOpen = false;
            _openFolderHandler();
        }));
    }
    if (!_projectFolder.empty())
        menu.push_back(PopupMenu::Action("Close Folder", [this]() {
            setProjectFolder({});
            openFileTree();
        }));
}

int Editor::fuzzyScore(std::string_view pattern, std::string_view path)
{
    if (pattern.empty())
        return 0;
    const auto nameStart = path.find_last_of("/\\");
    const size_t name = nameStart == std::string_view::npos ? 0 : nameStart + 1;
    // all letters in the file name count most, the whole name more
    if (const int inName = matchScore(pattern, path, name); inName >= 0)
        return inName + 20 + (pattern.size() == path.size() - name ? 10 : 0);
    return matchScore(pattern, path, 0);
}

std::string Editor::displayPath(const std::string& path) const
{
    const auto root = treeRoot();
    if (!root.empty()) {
        const auto relative = fs::path(path).lexically_relative(root);
        if (!relative.empty() && *relative.begin() != "..")
            return relative.generic_string();
    }
    return path;
}

void Editor::openQuickOpen()
{
    _quickOpenFiles.clear();
    const auto root = treeRoot();
    std::error_code ec;
    if (!root.empty()) {
        fs::recursive_directory_iterator iter(root, fs::directory_options::skip_permission_denied, ec);
        for (const fs::recursive_directory_iterator end; !ec && iter != end && _quickOpenFiles.size() < QUICK_OPEN_FILES; iter.increment(ec)) {
            const auto name = iter->path().filename().string();
            if (!name.empty() && name.front() == '.') {
                if (iter->is_directory(ec))
                    iter.disable_recursion_pending();
                continue;
            }
            if (iter->is_regular_file(ec) && !isBinaryFile(name))
                _quickOpenFiles.push_back(iter->path().lexically_normal().string());
        }
    }
    for (const auto& file : _files) {
        if (!file->path().empty() && std::ranges::find(_quickOpenFiles, file->path()) == _quickOpenFiles.end())
            _quickOpenFiles.push_back(file->path());
    }
    _fileTreeOpen = false;
    _quickOpenText.clear();
    _quickOpenSearched = "\n";
    _quickOpenSelected = 0;
    _quickOpenScroll = {};
    _quickOpenFocus = true;
    _quickOpenOpen = true;
}

std::vector<std::string> Editor::quickOpenMatches(const std::string& pattern) const
{
    struct Match
    {
        int score;
        int openRank;  // the position in the recently used files, INT_MAX for files that aren't open
        std::string display;
        const std::string* path;
    };
    std::vector<Match> matches;
    for (const auto& path : _quickOpenFiles) {
        auto display = displayPath(path);
        const int score = fuzzyScore(pattern, display);
        if (score < 0)
            continue;
        const auto open = std::ranges::find_if(_files, [&](const auto& file) { return file->path() == path; });
        const int openRank = open == _files.end() ? std::numeric_limits<int>::max() : static_cast<int>(open - _files.begin());
        matches.push_back({score + (openRank != std::numeric_limits<int>::max() ? 3 : 0), openRank, std::move(display), &path});
    }
    std::ranges::sort(matches, [&pattern](const Match& a, const Match& b) {
        if (pattern.empty()) {
            // without a pattern the open files first, the recently used first, then by path
            if (a.openRank != b.openRank)
                return a.openRank < b.openRank;
            return a.display < b.display;
        }
        if (a.score != b.score)
            return a.score > b.score;
        if (a.display.size() != b.display.size())
            return a.display.size() < b.display.size();
        return a.display < b.display;
    });
    std::vector<std::string> result;
    for (size_t i = 0; i < matches.size() && i < QUICK_OPEN_MATCHES; ++i)
        result.push_back(*matches[i].path);
    return result;
}

void Editor::drawQuickOpen(Rectangle codeArea)
{
    using namespace gui;
    constexpr float rowHeight = 16;
    const float width = std::min(420.0f, codeArea.width - 16);
    const float height = std::min(260.0f, codeArea.height - 16);
    const bool wasOpen = _quickOpenOpen;
    if (!BeginDrawer({std::floor(codeArea.x + (codeArea.width - width) / 2), codeArea.y + 8, width, height}, "quick-open", &_quickOpenOpen)) {
        if (wasOpen && IsKeyPressed(KEY_ESCAPE))
            setFocus();
        return;
    }
    SetSpacing(2);
    SetRowHeight(18);
    if (_quickOpenSearched != _quickOpenText) {
        _quickOpenResults = quickOpenMatches(_quickOpenText);
        _quickOpenSearched = _quickOpenText;
        _quickOpenSelected = 0;
        _quickOpenScroll = {};
    }
    const int count = static_cast<int>(_quickOpenResults.size());
    auto activated = [](int key) { return IsKeyPressed(key) || IsKeyPressedRepeat(key); };
    const int page = std::max(1, static_cast<int>((height - 26) / rowHeight) - 1);
    int move = 0;
    if (activated(KEY_DOWN))
        move = 1;
    else if (activated(KEY_UP))
        move = -1;
    else if (activated(KEY_PAGE_DOWN))
        move = page;
    else if (activated(KEY_PAGE_UP))
        move = -page;
    if (move && count) {
        _quickOpenSelected = std::clamp(_quickOpenSelected + move, 0, count - 1);
        // keep the selected row in view
        const float top = static_cast<float>(_quickOpenSelected) * rowHeight;
        const float view = height - 26 - 6;
        if (top < -_quickOpenScroll.y)
            _quickOpenScroll.y = -top;
        else if (top + rowHeight > -_quickOpenScroll.y + view)
            _quickOpenScroll.y = -(top + rowHeight - view);
    }
    std::optional<std::string> picked;
    if ((IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) && _quickOpenSelected < count)
        picked = _quickOpenResults[_quickOpenSelected];
    if (_quickOpenFocus) {
        SetKeyboardFocus(GetId("search"));
        _quickOpenFocus = false;
    }
    // the search field keeps the focus, but the keys of the list don't move its cursor
    const bool lockField = move != 0 && !GuiIsLocked();
    if (lockField)
        GuiLock();
    TextBox("search", _quickOpenText, 255);
    if (lockField)
        GuiUnlock();
    if (!HasKeyboardFocus(GetId("search")) && !picked)
        _quickOpenFocus = true;  // e.g. after Enter in an empty list, typing goes on in the field
    const auto font = GuiGetFont();
    BeginTableView(-1, "results", {TableColumn::Stretch()}, &_quickOpenScroll);
    for (int row = 0; row < count; ++row) {
        const auto& path = _quickOpenResults[row];
        Color textColor = GetColor(GetStyle(LISTVIEW, TEXT_COLOR_NORMAL));
        if (!TableNextRow(rowHeight, [&](Rectangle rect) {
                if (row == _quickOpenSelected) {
                    DrawRectangleClipped(rect.x, rect.y, rect.width, rect.height, GetColor(GetStyle(LISTVIEW, BASE_COLOR_PRESSED)));
                    textColor = GetColor(GetStyle(LISTVIEW, TEXT_COLOR_PRESSED));
                }
                else if (!GuiIsLocked() && CheckCollisionPointRec(GetMousePosition(), rect)) {
                    DrawRectangleClipped(rect.x, rect.y, rect.width, rect.height, GetColor(GetStyle(LISTVIEW, BASE_COLOR_NORMAL)));
                }
                if (!GuiIsLocked() && CheckCollisionPointRec(GetMousePosition(), rect) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                    picked = path;
            })) {
            continue;
        }
        TableNextColumn([&](Rectangle rect) {
            const auto mark = fileMark(path, false);
            const auto display = displayPath(path);
            const auto slash = display.find_last_of('/');
            const auto name = slash == std::string::npos ? display : display.substr(slash + 1);
            const auto color = mark.color.a && row != _quickOpenSelected ? mark.color : textColor;
            DrawTextClipped(font, name.c_str(), {rect.x + 4, rect.y + 4}, color);
            float x = rect.x + 4 + GetTextSize(name.c_str()).x + 8;
            if (!mark.suffix.empty()) {
                DrawTextClipped(font, mark.suffix.c_str(), {x, rect.y + 4}, ColorAlpha(color, 0.6f));
                x += GetTextSize(mark.suffix.c_str()).x + 8;
            }
            if (slash != std::string::npos)
                DrawTextClipped(font, display.substr(0, slash).c_str(), {x, rect.y + 4}, ColorAlpha(textColor, 0.5f));
        });
    }
    EndTableView();
    EndDrawer();
    if (picked) {
        _quickOpenOpen = false;
        pickFile(*picked);
    }
}

void Editor::pickFile(const std::string& path)
{
    if (isBinaryFile(path) || !looksLikeText(path))
        return;
    if (adoptAsMain(path)) {
        _fileTreeOpen = false;
        return;
    }
    if (openFile(path)) {
        _fileTreeOpen = false;
        setFocus();
    }
}

bool Editor::isMainReplaceable() const
{
    if (_projectFolder.empty() || isInProject(_main->path()))
        return false;
    return _main->diskFile().empty() ? !_main->code().canUndo() : !_main->hasUnsavedChanges();
}

bool Editor::adoptAsMain(const std::string& path)
{
    std::error_code ec;
    const auto normalized = fs::absolute(path, ec).lexically_normal().string();
    if (!_mainFileHandler || lowerExtension(normalized) != ".8o" || !isInProject(normalized) || !isMainReplaceable())
        return false;
    switchMain(normalized);
    return true;
}

bool Editor::switchMain(const std::string& path)
{
    // the app loads the program from disk, so the shown text has to be saved first
    autoSave();
    auto iter = std::ranges::find_if(_files, [&](const auto& file) { return file->path() == path; });
    if (!_mainFileHandler || (iter != _files.end() && (*iter)->hasUnsavedChanges()))
        return false;
    _mainFileHandler(path);
    return true;
}

bool Editor::definesMain(std::string_view text)
{
    for (size_t pos = 0; pos < text.size();) {
        auto end = text.find('\n', pos);
        if (end == std::string_view::npos)
            end = text.size();
        auto line = text.substr(pos, end - pos);
        pos = end + 1;
        const auto start = line.find_first_not_of(" \t");
        if (start == std::string_view::npos || line[start] != ':')
            continue;
        const auto name = line.find_first_not_of(" \t", start + 1);
        if (name == start + 1 || name == std::string_view::npos || line.substr(name, 4) != "main")
            continue;
        if (name + 4 == line.size() || std::string_view(" \t\r#").find(line[name + 4]) != std::string_view::npos)
            return true;
    }
    return false;
}

Editor::Place Editor::currentPlace() const
{
    const auto& code = currentFile().code();
    return {currentFile().path(), code.line(), code.column()};
}

void Editor::rememberPlace(const Place& place)
{
    if (_navigating || place.path.empty())
        return;
    if (!_backPlaces.empty() && _backPlaces.back().path == place.path && _backPlaces.back().line == place.line)
        _backPlaces.back() = place;
    else
        _backPlaces.push_back(place);
    if (_backPlaces.size() > MAX_PLACES)
        _backPlaces.erase(_backPlaces.begin());
    _forwardPlaces.clear();
}

void Editor::trackPlace()
{
    auto place = currentPlace();
    if (_lastPlace && _lastPlace->path != place.path)
        rememberPlace(*_lastPlace);
    _lastPlace = std::move(place);
}

bool Editor::showPlace(const Place& place)
{
    _navigating = true;
    const bool shown = openFile(place.path, place.line, place.column);
    _navigating = false;
    if (shown) {
        _lastPlace = currentPlace();
        setFocus();
    }
    return shown;
}

void Editor::goBack()
{
    const auto current = currentPlace();
    while (!_backPlaces.empty()) {
        const auto place = std::move(_backPlaces.back());
        _backPlaces.pop_back();
        if (place.path == current.path && place.line == current.line)
            continue;
        if (showPlace(place)) {
            if (!current.path.empty())
                _forwardPlaces.push_back(current);
            return;
        }
    }
}

void Editor::goForward()
{
    const auto current = currentPlace();
    while (!_forwardPlaces.empty()) {
        const auto place = std::move(_forwardPlaces.back());
        _forwardPlaces.pop_back();
        if (place.path == current.path && place.line == current.line)
            continue;
        if (showPlace(place)) {
            if (!current.path.empty())
                _backPlaces.push_back(current);
            return;
        }
    }
}

int Editor::navigationRequest() const
{
#ifdef PLATFORM_WEB
    return 0;  // the browser uses these keys and buttons for its own Back and Forward
#else
    if (GuiIsLocked() || gui::PopupMenu::IsOpen() || _newFileOpen || _quickOpenOpen || _fileTreeOpen)
        return 0;
    if (IsMouseButtonPressed(MOUSE_BUTTON_SIDE) || IsMouseButtonPressed(MOUSE_BUTTON_BACK))
        return -1;
    if (IsMouseButtonPressed(MOUSE_BUTTON_EXTRA) || IsMouseButtonPressed(MOUSE_BUTTON_FORWARD))
        return 1;
    if (!hasFocus())
        return 0;
    const bool alt = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);
    const bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    const bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
#ifdef __APPLE__
    const bool keys = gui::IsSysKeyDown() && alt && !ctrl && !shift;
#else
    const bool keys = alt && !ctrl && !shift;
#endif
    if (keys && IsKeyPressed(KEY_LEFT))
        return -1;
    if (keys && IsKeyPressed(KEY_RIGHT))
        return 1;
    return 0;
#endif
}

void Editor::followShownProgram()
{
    auto& file = currentFile();
    if (file.path() == _checkedProgramPath)
        return;
    _checkedProgramPath = file.path();
    if (&file == _main || !_mainFileHandler || file.diskFile().empty() || lowerExtension(file.path()) != ".8o" || !isInProject(file.path()) || !definesMain(file.getText()))
        return;
    if (_main->diskFile().empty() && _main->code().canUndo())
        return;
    switchMain(file.path());
}

std::string Editor::findMainFile(const std::string& folder)
{
    // the .8o files directly in the folder, and the ones they include
    std::error_code ec;
    std::vector<std::string> sources;
    for (const auto& entry : fs::directory_iterator(folder, ec)) {
        const auto name = entry.path().filename().string();
        if (entry.is_regular_file(ec) && lowerExtension(name) == ".8o" && name.front() != '.')
            sources.push_back(fs::absolute(entry.path(), ec).lexically_normal().string());
    }
    std::ranges::sort(sources);
    if (sources.size() <= 1)
        return sources.empty() ? std::string{} : sources.front();
    std::vector<std::string> included;
    for (const auto& source : sources) {
        std::ifstream input(source, std::ios::binary);
        std::string line;
        while (std::getline(input, line)) {
            line = line.substr(0, line.find('#'));
            for (auto pos = line.find(":include"); pos != std::string::npos; pos = line.find(":include", pos + 8)) {
                const auto open = line.find_first_not_of(" \t", pos + 8);
                if (open == std::string::npos || line[open] != '"')
                    continue;
                const auto close = line.find('"', open + 1);
                if (close == std::string::npos)
                    continue;
                const auto file = fs::path(source).parent_path() / fs::path(line.substr(open + 1, close - open - 1));
                included.push_back(fs::absolute(file, ec).lexically_normal().string());
            }
        }
    }
    // the ones no other includes, if that are several, the one named like the folder
    std::vector<std::string> roots;
    std::ranges::copy_if(sources, std::back_inserter(roots), [&](const auto& source) { return std::ranges::find(included, source) == included.end(); });
    if (roots.size() == 1)
        return roots.front();
    auto folderName = fs::absolute(folder, ec).lexically_normal();
    if (!folderName.has_filename())
        folderName = folderName.parent_path();
    const auto& candidates = roots.empty() ? sources : roots;
    auto named = std::ranges::find_if(candidates, [&](const auto& source) { return fs::path(source).stem() == folderName.filename(); });
    return named != candidates.end() ? *named : std::string{};
}

void Editor::startNewFile(const std::string& folder)
{
    _newFileFolder = folder;
    _newFileName.clear();
    _newFileError.clear();
    _newFileOpen = true;
    _newFileFocus = true;
}

std::string Editor::createFile(const std::string& folder, std::string name)
{
    const auto first = name.find_first_not_of(" \t");
    name = first == std::string::npos ? std::string{} : name.substr(first, name.find_last_not_of(" \t") - first + 1);
    if (name.empty())
        return "Enter a file name";
    if (fs::path(name).extension().empty())
        name += ".8o";
    std::error_code ec;
    const auto path = (fs::path(folder) / name).lexically_normal();
    const auto relative = path.lexically_relative(folder);
    if (relative.empty() || *relative.begin() == ".." || !path.has_filename())
        return "The file must be in the folder";
    if (fs::exists(path, ec))
        return "There is already a file " + relative.string();
    fs::create_directories(path.parent_path(), ec);
    {
        std::ofstream output(path.string(), std::ios::binary);
        if (!output)
            return "Couldn't create " + relative.string();
    }
    if (!openFile(path.string()))
        return "Couldn't open " + relative.string();
    _fileTree.refresh();
    _fileTree.reveal(currentFile().path());
    setFocus();
    return {};
}

bool Editor::hasUnsavedChanges() const
{
    return std::ranges::any_of(_files, [](const auto& file) { return file->hasUnsavedChanges(); });
}

bool Editor::autoSave()
{
    bool ok = true;
    for (auto& file : _files)
        ok = file->autoSave() && ok;
    return ok;
}

bool Editor::save()
{
    bool ok = true;
    for (auto& file : _files)
        ok = file->save() && ok;
    return ok;
}

std::string Editor::saveError() const
{
    if (!currentFile().saveError().empty())
        return currentFile().saveError();
    for (const auto& file : _files) {
        if (!file->saveError().empty())
            return file->name() + ": " + file->saveError();
    }
    return {};
}

void Editor::setHistoryDirectory(const std::string& path)
{
    _historyDirectory = path;
    for (auto& file : _files)
        file->setHistoryDirectory(path);
    std::error_code ec;
    if (path.empty() || !fs::is_directory(path, ec))
        return;
    const auto oldest = fs::file_time_type::clock::now() - std::chrono::hours(24 * SourceFile::MAX_HISTORY_DAYS);
    for (const auto& entry : fs::directory_iterator(path, ec)) {
        if (entry.is_regular_file(ec) && entry.last_write_time(ec) < oldest)
            fs::remove(entry.path(), ec);
    }
}

void Editor::checkDiskFile(bool force)
{
#ifndef PLATFORM_WEB
    if (!force) {
        const bool focused = IsWindowFocused();
        const double now = GetTime();
        if (!focused || (_wasFocused && now - _lastDiskCheck < 1.0)) {
            _wasFocused = focused;
            return;
        }
        _wasFocused = true;
        _lastDiskCheck = now;
    }
    for (auto& file : _files)
        file->checkDiskFile();
    // an included file that isn't open but was changed by another program needs a compilation
    for (const auto& [path, time] : _includedFileTimes) {
        std::error_code ec;
        const auto now = fs::last_write_time(path, ec);
        if (now != time && std::ranges::none_of(_files, [&](const auto& file) { return file->path() == path; })) {
            _compiledRevision = contentRevision() - 1;
            _checkedRevision = contentRevision() - 1;
            _compiledSourceSha1 = {};
            _includedFileTimes.clear();
            break;
        }
    }
#endif
}

// A digest of the texts of all open files, a compile result is still valid if it didn't change
Sha1::Digest Editor::sourcesDigest() const
{
    std::vector<const SourceFile*> files;
    for (const auto& file : _files)
        files.push_back(file.get());
    std::ranges::sort(files, [](const auto* a, const auto* b) { return a->path() < b->path(); });
    std::string all = _main->path() + '\0';  // another main file is another build
    for (const auto* file : files)
        all += file->path() + '\0' + file->getText() + '\0';
    return calculateSha1(all);
}

void Editor::update()
{
    trackPlace();
    followShownProgram();
    if(gui::StyleManager::instance().isInvertedTheme() != _isInvertedTheme) {
        _isInvertedTheme = gui::StyleManager::instance().isInvertedTheme();
        updateTextStyle(_textStyle.font);
    }
    const auto revision = contentRevision();
    if(revision != _lastSeenRevision) {
        // text changed since the last frame, restart the inactivity delay
        _lastSeenRevision = revision;
        _inactiveEditTimer = 0;
    }
    else if(revision != _checkedRevision) {
        // text changed earlier, check it once after the inactivity delay
        _inactiveEditTimer += GetFrameTime();
        if(_inactiveEditTimer > INACTIVITY_DELAY) {
            _inactiveEditTimer = 0;
            _checkedRevision = revision;
            _checkedProgramPath.clear();  // an edit may have given the shown file a main label
            autoSave();
            if(_compiledRevision != revision) {
                if(sourcesDigest() == _compiledSourceSha1)
                    _compiledRevision = revision; // e.g. edits undone, compile result still matches
                else
                    compile();
            }
        }
    }
}

// The open files are saved first, the compiler reads included files from disk
void Editor::compile()
{
    autoSave();
    _compiledRevision = contentRevision();
    _compiledSourceSha1 = sourcesDigest();
    try {
        auto start = std::chrono::steady_clock::now();
        _compiler.reset();
        const auto& text = _main->getText();
        if(text.empty() || text.back() != '\n') {
            auto terminated = text + '\n';
            _compiler.compile(_filename, terminated);
        }
        else {
            _compiler.compile(_filename, text);
        }
        auto time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
        TraceLog(LOG_INFO, "Recompilation took %dms", (int)time_ms);
    }
    catch(std::exception& ex)
    {}
    _includedFileTimes.clear();
    for (const auto& path : includedFiles()) {
        std::error_code ec;
        if (const auto time = fs::last_write_time(path, ec); !ec)
            _includedFileTimes[path] = time;
    }
}

void Editor::updateCompilerOptions(int startAddress)
{
    if(_compiler.setStartAddress(startAddress) && isCompiledUpToDate()) {
        // Compile again at the next check instead of right now, the text might be big
        // and the editor might not even be visible.
        _compiledRevision = contentRevision() - 1;
        _checkedRevision = contentRevision() - 1;
        _compiledSourceSha1 = {};
    }
}

void Editor::draw(Font& font, float height)
{
    if(font.texture.id != _textStyle.font.texture.id)
        updateTextStyle(font);
    if (!GuiIsLocked() && hasFocus()) {
        const bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
        if (ctrl && IsKeyPressed(KEY_TAB) && _files.size() > 1)
            switchTo(*_files[1]);
        else if (gui::IsShortcutPressed('W'))
            closeCurrentFile();
    }
#ifndef PLATFORM_WEB
    // the digit is the same key in all layouts, the mouse may be over the drawer, that locks the GUI
    if (!_newFileOpen && !gui::PopupMenu::IsOpen() && gui::IsShortcutPressed('P')) {
        if (_quickOpenOpen) {
            _quickOpenOpen = false;
            setFocus();
        }
        else {
            openQuickOpen();
        }
    }
    else if (!_newFileOpen && !gui::PopupMenu::IsOpen() && gui::IsSysKeyDown() && IsKeyPressed(KEY_ONE)) {
        _quickOpenOpen = false;
        if (_fileTreeOpen) {
            _fileTreeOpen = false;
            setFocus();
        }
        else {
            openFileTree();
        }
    }
#endif
    const float codeHeight = height - (_messageWindowVisible ? LINE_SIZE * 2 + 2 : 0);
    const Rectangle codeArea{gui::GetCurrentPos().x, gui::GetCurrentPos().y, gui::GetContentAvailable().width, codeHeight};
    _pendingNavigation = navigationRequest();  // the code editor ignores the keys then
    currentFile().code().render(codeHeight);
    if (_pendingNavigation < 0)
        goBack();
    else if (_pendingNavigation > 0)
        goForward();
    _pendingNavigation = 0;
    drawMessageArea();
#ifndef PLATFORM_WEB
    drawFileTree(codeArea);
    drawQuickOpen(codeArea);
    drawNewFileDialog();
#endif
}

void Editor::drawFileTree(Rectangle codeArea)
{
    using namespace gui;
    const bool wasOpen = _fileTreeOpen;
    if (BeginDrawer({codeArea.x, codeArea.y, std::min(260.0f, std::floor(codeArea.width / 2)), codeArea.height}, "file-tree", &_fileTreeOpen)) {
        SetRowHeight(16);
        SetSpacing(0);
        const auto root = _fileTree.root();
        const auto title = root.empty() ? std::string("No folder") : (_projectFolder.empty() ? "Folder: " : "Project: ") + fs::path(root).filename().string();
        StatusBar(title.c_str());
        if (root.empty()) {
            Space(4);
            Label(" Open a folder or a source file first.");
            if (_openFolderHandler) {
                SetNextWidth(100);
                if (Button("Open Folder...")) {
                    _fileTreeOpen = false;
                    _openFolderHandler();
                }
            }
        }
        else if (auto file = _fileTree.render()) {
            pickFile(*file);
        }
        EndDrawer();
    }
    else if (wasOpen && IsKeyPressed(KEY_ESCAPE)) {
        setFocus();
    }
}

void Editor::drawNewFileDialog()
{
    using namespace gui;
    if (!_newFileOpen)
        return;
    if (BeginWindowBox({-1, -1, 320, 100}, "New File", &_newFileOpen, WindowBoxFlags(WBF_CLOSABLE | WBF_MODAL))) {
        SetSpacing(2);
        SetRowHeight(18);
        const auto base = treeRoot().empty() ? fs::path(_newFileFolder) : fs::path(_newFileFolder).lexically_relative(fs::path(treeRoot()).parent_path());
        Label(("In " + base.string() + ":").c_str());
        if (_newFileFocus) {
            SetKeyboardFocus(GetId("name"));
            _newFileFocus = false;
        }
        TextBox("name", _newFileName, 255);
        bool create = IsLastWidgetEditFinished() && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER));
        BeginColumns();
        SetSpacing(4);
        SetNextWidth(GetContentAvailable().width - 128);
        Label(_newFileError.c_str());
        SetNextWidth(60);
        create |= Button("Create");
        SetNextWidth(GetContentAvailable().width);
        if (Button("Cancel") || IsKeyPressed(KEY_ESCAPE))
            _newFileOpen = false;
        EndColumns();
        if (create && _newFileOpen) {
            _newFileError = createFile(_newFileFolder, _newFileName);
            if (_newFileError.empty())
                _newFileOpen = false;
        }
        EndWindowBox();
    }
}

static void DrawRectangleX(Rectangle rec, int borderWidth, Color borderColor, Color color)
{
    if (color.a > 0)
    {
        // Draw rectangle filled with color
        DrawRectangle((int)rec.x, (int)rec.y, (int)rec.width, (int)rec.height, color);
    }

    if (borderWidth > 0)
    {
        // Draw rectangle border lines with color
        DrawRectangle((int)rec.x, (int)rec.y, (int)rec.width, borderWidth, borderColor);
        DrawRectangle((int)rec.x, (int)rec.y + borderWidth, borderWidth, (int)rec.height - 2*borderWidth, borderColor);
        DrawRectangle((int)rec.x + (int)rec.width - borderWidth, (int)rec.y + borderWidth, borderWidth, (int)rec.height - 2*borderWidth, borderColor);
        DrawRectangle((int)rec.x, (int)rec.y + (int)rec.height - borderWidth, (int)rec.width, borderWidth, borderColor);
    }
}

void Editor::drawMessageArea()
{
    if(!_messageWindowVisible)
        return;
    using namespace gui;
    auto area = GetContentAvailable();
    DrawRectangleX({area.x - 1, area.y, area.width + 2, area.height + 1}, 1, GetColor(gui::GetStyle(DEFAULT, LINE_COLOR)), {0,0,0,0});
    BeginClipping({area.x, area.y + 1, area.width, area.height - 1});
    const auto& compileResult = _compiler.compileResult();
    const auto saveProblem = saveError();
    if(!saveProblem.empty()) {
        // a source that couldn't be saved is more important than the compile result, which is still shown in short
        DrawTextClipped(GuiGetFont(), ("Not saved: " + saveProblem).c_str(), {area.x + 2, area.y + 4}, StyleManager::mappedColor(ORANGE));
        const auto status = compileResult.resultType == emu::CompileResult::eOK ? std::string("No errors.") : compileResult.errorMessage;
        DrawTextClipped(GuiGetFont(), status.c_str(), {area.x + 2, area.y + 15}, StyleManager::getStyleColor(Style::TEXT_COLOR_NORMAL));
    }
    else if(compileResult.resultType == emu::CompileResult::eOK) {
        DrawTextClipped(GuiGetFont(), "No errors.", {area.x + 2, area.y + 4}, StyleManager::getStyleColor(Style::TEXT_COLOR_NORMAL));
        if (!_programWarning.empty())
            DrawTextClipped(GuiGetFont(), _programWarning.c_str(), {area.x + 2, area.y + 15}, StyleManager::mappedColor(ORANGE));
    }
    else {
        const auto& location = compileResult.locations.front();
        std::error_code ec;
        auto baseDir = fs::path(fs::absolute(_filename, ec)).parent_path();
        auto relFile = fs::relative(location.file, baseDir, ec);
        if(ec) relFile = location.file;
        // a click on the location goes there, also into an included file
        const auto text = fmt::format("{}:{}:{}:", relFile.string(), location.line, location.column);
        const Rectangle textRect{area.x + 2, area.y + 2, GetTextSize(text.c_str()).x, 11};
        const bool hover = !GuiIsLocked() && CheckCollisionPointRec(GetMousePosition(), textRect);
        DrawTextClipped(GuiGetFont(), text.c_str(), {area.x + 2, area.y + 4}, hover ? StyleManager::getStyleColor(Style::TEXT_COLOR_FOCUSED) : StyleManager::getStyleColor(Style::TEXT_COLOR_NORMAL));
        DrawTextClipped(GuiGetFont(), compileResult.errorMessage.c_str(), {area.x + 2, area.y + 15}, StyleManager::mappedColor(ORANGE));
        if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            const auto line = static_cast<uint32_t>(std::max(location.line, 1));
            const auto column = static_cast<uint32_t>(std::max(location.column, 1));
            if (location.file == _filename || fs::absolute(location.file, ec).lexically_normal().string() == _main->path()) {
                switchTo(*_main);
                _main->code().goTo(line, column);
            }
            else {
                openFile(location.file, line, column);
            }
            setFocus();
        }
    }
    EndClipping();
}
