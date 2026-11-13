//---------------------------------------------------------------------------------------
// src/editor.hpp
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
#pragma once

#include <chiplet/octocompiler.hpp>
#include <chiplet/sha1.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <raylib.h>

#include <highlighter.hpp>
#include <sourcefile.hpp>
#include <rlguipp/rlguipp_filetree.hpp>

// The source editor of Cadmium: the main file is the source of the loaded program, further files (like the ones it
// includes) can be opened for editing, each with its own undo history (see SourceFile). Once the files were left unchanged
// for INACTIVITY_DELAY seconds, they are saved and the main file is compiled, the result is shown below the text.
// The files are kept in the order of their use, the panel title shows the current one, clicking it (or Ctrl+Tab for the
// one used before) switches, Sys+W closes it, which makes its edits final.
// In the native builds a file tree (Sys+1) shows the project folder, or the folder of the main file without one. It
// marks the main file, the open ones and the ones in the build, picks files to open and creates new ones. Quick open
// (Sys+P) finds a file of that folder or an open one by some letters of its name.
class Editor
{
public:
    static constexpr float INACTIVITY_DELAY{1.0f};

    Editor();
    // The main file, its name is the one the compiler resolves includes from
    void setFilename(std::string filename) { _filename = std::move(filename); }
    const std::string& getFilename() const { return _filename; }
    bool isEmpty() const { return _main->getText().empty(); }
    const std::string& getText() const { return _main->getText(); }
    // Starts with a new main file with this text, belonging to the disk file if given (see SourceFile::setDiskFile()). The
    // files that were open are saved and closed, but for a disk file in the project folder the open files of the project
    // stay open, an open file that is the disk file becomes the main file.
    void setText(std::string text, const std::string& diskFile = {});
    // Sets text that was generated from the loaded program (a disassembly). It is not compiled
    // until the user edits it, and it can't be undone into the previous program's text.
    void setGeneratedText(std::string text);
    // The file of the main text, see SourceFile::setDiskFile()
    void setDiskFile(const std::string& path) { _main->setDiskFile(path); }
    // A problem of the compiled program the app found (it doesn't fit into the core e.g.), shown below "No errors."
    void setProgramWarning(std::string warning) { _programWarning = std::move(warning); }
    const std::string& diskFile() const { return _main->diskFile(); }

    // The open files, the most recently used first, the current one is shown
    SourceFile& currentFile() { return *_files.front(); }
    const SourceFile& currentFile() const { return *_files.front(); }
    SourceFile& mainFile() { return *_main; }
    const std::vector<std::unique_ptr<SourceFile>>& files() const { return _files; }
    // Opens the file (or switches to it if it is open already), at the line and column if they are given (1 based)
    bool openFile(const std::string& path, uint32_t line = 0, uint32_t column = 0);
    void switchTo(SourceFile& file);
    // Saves and closes the current file, its undo history is gone then, the main file can't be closed
    void closeCurrentFile();

    // Back and Forward: showing another file (a link, an error location, the file list, the tree, quick open) and jumps to
    // an error location remember the place left, goBack() returns there and goForward() undoes that. Places of closed files
    // open them again, places of files that are gone are skipped. Sys+Alt+Left/Right (Alt+Left/Right on Windows and
    // Linux) and the side buttons of the mouse go back and forward, not on the web.
    static constexpr size_t MAX_PLACES = 50;
    bool canGoBack() const { return !_backPlaces.empty(); }
    bool canGoForward() const { return !_forwardPlaces.empty(); }
    void goBack();
    void goForward();
    // Called by update(): notices that another file is shown and remembers the place left in the one shown before
    void trackPlace();
    std::string title() const;
    // The link of a source file name in an :include line at the offset of the file's text (Sys+click and the context
    // menu open it), none for other text and for included images, binaries and sounds
    std::optional<gui::CodeLink> includeLinkAt(SourceFile& file, uint32_t offset);
    // The files the last compilation read through :include (absolute), and if a file is one of them or the main file
    const std::vector<std::string>& includedFiles() const { return _compiler.compileResult().includedFiles; }
    bool isInBuild(const SourceFile& file) const;
    // The list of the open files to switch to, with the close action
    void openFileMenu(Vector2 position);

    // The project folder (absolute, empty for none), the file tree shows it
    void setProjectFolder(const std::string& path);
    const std::string& projectFolder() const { return _projectFolder; }
    bool isInProject(const std::string& path) const;
    // The folder the file tree shows: the project folder, or the folder of the main or current file
    std::string treeRoot() const;
    // The app makes a file the main file by loading it as the program, and opens a folder (both optional)
    void setMainFileHandler(std::function<void(const std::string& path)> handler) { _mainFileHandler = std::move(handler); }
    void setOpenFolderHandler(std::function<void()> handler) { _openFolderHandler = std::move(handler); }
    // Shows the file tree over the left side of the editor with the current file selected, Sys+1 toggles it
    void openFileTree();
    bool isFileTreeOpen() const { return _fileTreeOpen; }
    // How the tree shows a file: "(main)" or "(in build)" after the name, open files highlighted, files that aren't
    // sources dimmed
    gui::FileTreeMark fileMark(const std::string& path, bool isDirectory) const;
    // Creates an empty file in the folder (".8o" is added to a name without extension) and opens it, returns why it
    // couldn't, or an empty string
    std::string createFile(const std::string& folder, std::string name);
    // The program of a folder: its only .8o file, or the only one of them no other one includes, or of those the one named
    // like the folder; empty if that doesn't tell
    static std::string findMainFile(const std::string& folder);
    // If the source defines `: main` (on a line of its own start), it is a program then, not only included
    static bool definesMain(std::string_view text);
    // Opens a file picked in the tree or the quick open: a project .8o file becomes the main file while the main file is
    // from outside of the project and can be closed without loss, the startup text e.g.
    void pickFile(const std::string& path);

    // Quick open: openQuickOpen() collects the text files below the tree root (hidden ones and at most QUICK_OPEN_FILES)
    // and shows the search, quickOpenMatches() are the ones with the letters of the pattern in order (no case), the best
    // first: matches in the file name, at word starts and in runs score higher, open files a bit.
    static constexpr size_t QUICK_OPEN_FILES = 5000;
    static constexpr size_t QUICK_OPEN_MATCHES = 200;
    void openQuickOpen();
    bool isQuickOpenOpen() const { return _quickOpenOpen; }
    std::vector<std::string> quickOpenMatches(const std::string& pattern) const;
    // The score of the path for the pattern, -1 if it doesn't have all letters in order, the empty pattern scores 0
    static int fuzzyScore(std::string_view pattern, std::string_view path);

    uint32_t line() const { return currentFile().code().line(); }
    uint32_t column() const { return currentFile().code().column(); }
    // Changes with every change of the text of any open file
    uint32_t contentRevision() const;
    // True if compiler() holds the result for the current texts
    bool isCompiledUpToDate() const { return _compiledRevision == contentRevision(); }

    void setFocus() { currentFile().code().setFocus(); }
    bool hasFocus() const { return currentFile().code().hasFocus(); }

    bool hasUnsavedChanges() const;
    // Saves the changes of all open files, see SourceFile::autoSave(), false if one couldn't be saved
    bool autoSave();
    // Saves all open files now, as the user asked for it (Sys+S)
    bool save();
    // The first save error of the open files, the current file first
    std::string saveError() const;

    // The history copies of the opened versions go there, copies older than SourceFile::MAX_HISTORY_DAYS are removed
    void setHistoryDirectory(const std::string& path);
    // Looks for changes of the open files by other programs, once a second while Cadmium has the focus and at once when it
    // gets it back (force: now), see SourceFile::checkDiskFile(). A change of an included file that isn't open makes the
    // next update() compile again.
    void checkDiskFile(bool force = false);

    // Saves and compiles once the files were unchanged for INACTIVITY_DELAY seconds, called every frame the editor is shown,
    // and calls followShownProgram()
    void update();
    // A shown project .8o file that defines `: main` becomes the main file (the app loads it as the program), checked once
    // per shown file and again after an edit
    void followShownProgram();

    // Saves the files and compiles the main file now
    void compile();
    void updateCompilerOptions(int startAddress);
    const emu::OctoCompiler& compiler() const { return _compiler; }

    // Shows the editor and the compile result below it at the layout position
    void draw(Font& font, float height);

private:
    static constexpr int LINE_SIZE = 12;
    SourceFile& addFile(std::unique_ptr<SourceFile> file);
    void resetFiles(std::unique_ptr<SourceFile> main);
    Sha1::Digest sourcesDigest() const;
    std::string includeBase(const SourceFile& file) const;
    void updateTextStyle(const Font& font);
    void drawMessageArea();
    void drawFileTree(Rectangle codeArea);
    void drawNewFileDialog();
    void drawQuickOpen(Rectangle codeArea);
    // Picking a project .8o file while the main file is a replaceable one from outside makes it the main file
    bool isMainReplaceable() const;
    bool adoptAsMain(const std::string& path);
    // Saves the open files and lets the app load the file as the program, false if it couldn't be saved
    bool switchMain(const std::string& path);
    struct Place
    {
        std::string path;
        uint32_t line{1};
        uint32_t column{1};
    };
    Place currentPlace() const;
    void rememberPlace(const Place& place);
    bool showPlace(const Place& place);
    int navigationRequest() const;  // -1 back, 1 forward, 0 none
    std::string displayPath(const std::string& path) const;
    void startNewFile(const std::string& folder);
    void fileTreeMenu(gui::PopupMenu::Menu& menu, const std::string& path, bool isDirectory);

    Highlighter _highlighter;
    gui::CodeTextStyle _textStyle;
    std::vector<std::unique_ptr<SourceFile>> _files;  // the most recently used first
    SourceFile* _main{nullptr};
    uint32_t _revisionBase{0};             // the revisions of closed files, so contentRevision() only grows
    std::string _filename;
    std::string _historyDirectory;
    bool _wasFocused{true};
    double _lastDiskCheck{0};
    bool _messageWindowVisible{true};
    std::string _programWarning;
    bool _isInvertedTheme{};
    emu::OctoCompiler _compiler;
    uint32_t _lastSeenRevision{0};
    uint32_t _checkedRevision{0};
    uint32_t _compiledRevision{0};
    Sha1::Digest _compiledSourceSha1;
    std::map<std::string, fs::file_time_type> _includedFileTimes;  // of the included files at the last compilation
    float _inactiveEditTimer{0};
    std::string _projectFolder;
    gui::FileTree _fileTree{"file-tree"};
    bool _fileTreeOpen{false};
    std::function<void(const std::string&)> _mainFileHandler;
    std::string _checkedProgramPath;       // the shown file followShownProgram() looked at last
    std::vector<Place> _backPlaces;
    std::vector<Place> _forwardPlaces;
    std::optional<Place> _lastPlace;       // the place trackPlace() saw last
    bool _navigating{false};               // going back or forward, that isn't remembered as a jump
    int _pendingNavigation{0};             // requested in this frame, done after the editor was rendered
    std::function<void()> _openFolderHandler;
    bool _newFileOpen{false};
    bool _newFileFocus{false};             // the name field gets the focus in the next frame
    std::string _newFileFolder;
    std::string _newFileName;
    std::string _newFileError;
    bool _quickOpenOpen{false};
    bool _quickOpenFocus{false};           // the search field gets the focus in the next frame
    std::string _quickOpenText;
    std::string _quickOpenSearched{"\n"}; // the text the matches are for
    std::vector<std::string> _quickOpenFiles;
    std::vector<std::string> _quickOpenResults;
    int _quickOpenSelected{0};
    Vector2 _quickOpenScroll{};
};
