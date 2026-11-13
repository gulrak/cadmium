//---------------------------------------------------------------------------------------
// src/sourcefile.hpp
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

#include <chiplet/octocompiler.hpp>

#include <cstdint>
#include <memory>
#include <string>

#include <rlguipp/rlguipp_codeeditor.hpp>

// A source file the editor has open: its text in a gui::CodeEditor with its undo history, and the file on disk it
// belongs to. A text from a writable file is saved automatically (see autoSave()), its undo history stays, so saving
// doesn't make edits final. Other texts (a disassembly, a new or a cartridge source) need an explicit Save As.
class SourceFile
{
public:
    // Before the first save of a disk file, its content as it was opened is copied into the history folder (the newest
    // MAX_HISTORY_VERSIONS per file are kept, Editor removes the ones older than MAX_HISTORY_DAYS)
    static constexpr int MAX_HISTORY_VERSIONS = 10;
    static constexpr int MAX_HISTORY_DAYS = 30;

    explicit SourceFile(std::string text = {});  // a text that doesn't belong to a file yet, it can't be undone
    static std::unique_ptr<SourceFile> open(const std::string& path);  // nullptr if the file can't be read

    gui::CodeEditor& code() { return _code; }
    const gui::CodeEditor& code() const { return _code; }
    const std::string& getText() const { return _code.getText(); }
    void setText(std::string text);  // as an undoable edit
    uint32_t contentRevision() const { return _code.contentRevision(); }

    // The file the text belongs to (absolute), empty if none, and the file name or "untitled"
    const std::string& path() const { return _path; }
    std::string name() const;

    // Sets the file the text belongs to, empty for none. It is only taken if the text is its content (line endings aside),
    // its changes are saved automatically if it can be written. The text is in its saved state then.
    void setDiskFile(const std::string& path);
    const std::string& diskFile() const { return _diskFile; }
    bool hasUnsavedChanges() const { return !_diskFile.empty() && contentRevision() != _savedRevision; }
    // Saves the changes to the disk file, a failed attempt is not repeated for the same text and a file that was changed on
    // disk since it was loaded or saved is not overwritten. Returns false if it couldn't save, saveError() tells why.
    bool autoSave();
    // Saves the changes now, as the user asked for it, also over a file changed on disk
    bool save();
    const std::string& saveError() const { return _saveError; }

    void setHistoryDirectory(const std::string& path) { _historyDirectory = path; }
    // Sets the text back to the content the disk file had when it was opened, as an undoable edit
    bool canRevertToOpened() const;
    void revertToOpened();
    // Looks for a change of the disk file by another program: without own unsaved changes the file is reloaded as one
    // undoable edit, with them it is a conflict, saveError() tells about it, reloadFromDisk() or save() solve it
    void checkDiskFile();
    bool isChangedOnDisk() const { return _changedOnDisk; }
    void reloadFromDisk();

private:
    bool writeDiskFile(bool overwriteChanged);
    void writeHistory();
    void replaceText(std::string text);

    gui::CodeEditor _code{"Editor"};       // all files have the same focus id, only one is shown
    std::string _path;
    std::string _diskFile;
    bool _diskFileCrLf{false};             // the file had Windows line endings, they are kept
    uint32_t _savedRevision{0};
    uint32_t _failedSaveRevision{~0u};     // the revision an automatic save failed for
    fs::file_time_type _diskFileTime{};    // the modification time when it was loaded or saved
    std::string _saveError;
    std::string _openedContent;            // the file as it was when it was opened, for the history
    std::string _openedText;               // the same as text, for reverting
    std::string _historyDirectory;
    bool _historyWritten{false};
    bool _changedOnDisk{false};            // changed by another program while there were own unsaved changes
};
