//---------------------------------------------------------------------------------------
// src/sourcefile.cpp
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
#include <sourcefile.hpp>
#include <chiplet/utility.hpp>

#include <algorithm>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iterator>
#include <vector>

static bool readSource(const std::string& path, std::string& content, std::string& text, bool& crLf)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
        return false;
    content.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    crLf = content.find("\r\n") != std::string::npos;
    text.clear();
    text.reserve(content.size());
    for (size_t i = 0; i < content.size(); ++i) {
        if (!crLf || content[i] != '\r' || i + 1 >= content.size() || content[i + 1] != '\n')
            text += content[i];
    }
    return true;
}

SourceFile::SourceFile(std::string text)
{
    _code.setText(std::move(text));
    _code.clearHistory();
    _code.setContextMenuHandler([this](gui::PopupMenu::Menu& menu, const gui::CodeContext&) {
        if (_diskFile.empty())
            return;
        menu.push_back(gui::PopupMenu::Separator());
        if (_changedOnDisk)
            menu.push_back(gui::PopupMenu::Action("Reload from Disk", [this]() { reloadFromDisk(); }));
        auto revert = gui::PopupMenu::Action("Revert to Opened Version", [this]() { revertToOpened(); });
        menu.push_back(canRevertToOpened() ? revert : gui::PopupMenu::Disabled(revert));
    });
}

std::unique_ptr<SourceFile> SourceFile::open(const std::string& path)
{
    std::string content, text;
    bool crLf = false;
    if (!readSource(path, content, text, crLf))
        return nullptr;
    auto file = std::make_unique<SourceFile>(std::move(text));
    file->setDiskFile(path);
    return file;
}

std::string SourceFile::name() const
{
    return _path.empty() ? std::string("untitled") : fs::path(_path).filename().string();
}

void SourceFile::setText(std::string text)
{
    _code.setText(std::move(text));
}

void SourceFile::setDiskFile(const std::string& path)
{
    _path.clear();
    _diskFile.clear();
    _saveError.clear();
    _savedRevision = contentRevision();
    _failedSaveRevision = ~0u;
    _historyWritten = _changedOnDisk = false;
#ifndef PLATFORM_WEB
    if (path.empty())
        return;
    std::error_code ec;
    if (!fs::is_regular_file(path, ec))
        return;
    // only a text that is the content of the file belongs to it, a source from a cartridge next to a source file of the
    // same name could be another version
    std::string content, text;
    if (!readSource(path, content, text, _diskFileCrLf) || text != _code.getText())
        return;
    _path = fs::absolute(path, ec).lexically_normal().string();
    {
        // a file in a place that can't be written (like inside an application bundle) needs a Save As
        std::ofstream test(path, std::ios::binary | std::ios::app);
        if (!test)
            return;
    }
    _openedContent = std::move(content);
    _openedText = std::move(text);
    _diskFileTime = fs::last_write_time(path, ec);
    _diskFile = path;
#endif
}

bool SourceFile::autoSave()
{
    if (!hasUnsavedChanges() || contentRevision() == _failedSaveRevision)
        return _saveError.empty();
    if (!writeDiskFile(false)) {
        _failedSaveRevision = contentRevision();
        return false;
    }
    return true;
}

bool SourceFile::save()
{
    return !hasUnsavedChanges() || writeDiskFile(true);
}

// Writes the text to a file next to the disk file and renames it over the disk file, so a crash never leaves a half
// written source, the permissions of the file are kept
bool SourceFile::writeDiskFile(bool overwriteChanged)
{
    std::error_code ec;
    const fs::path path(_diskFile);
    if (const auto time = fs::last_write_time(path, ec); !overwriteChanged && !ec && time != _diskFileTime) {
        _saveError = "The file was changed on disk, it is not saved over.";
        return false;
    }
    if (!_historyWritten)
        writeHistory();
    auto text = _code.getText();
    if (_diskFileCrLf) {
        std::string converted;
        converted.reserve(text.size() + text.size() / 16);
        for (const char c : text) {
            if (c == '\n')
                converted += '\r';
            converted += c;
        }
        text = std::move(converted);
    }
    const auto temporary = fs::path(path).concat(".cadmium-saving");
    {
        std::ofstream output(temporary.string(), std::ios::binary | std::ios::trunc);
        if (!output || !output.write(text.data(), static_cast<std::streamsize>(text.size()))) {
            fs::remove(temporary, ec);
            _saveError = "Couldn't write " + temporary.filename().string() + ".";
            return false;
        }
    }
    if (const auto status = fs::status(path, ec); !ec)
        fs::permissions(temporary, status.permissions(), ec);
    fs::rename(temporary, path, ec);
    if (ec) {
        fs::remove(temporary, ec);
        _saveError = "Couldn't replace " + path.filename().string() + ".";
        return false;
    }
    _savedRevision = contentRevision();
    _failedSaveRevision = ~0u;
    _diskFileTime = fs::last_write_time(path, ec);
    _saveError.clear();
    _changedOnDisk = false;
    TraceLog(LOG_INFO, "Saved source to %s", _diskFile.c_str());
    return true;
}

// The history copies of a file are named <stem>-<hash of its path>-<date>-<time>-<milliseconds><extension>, so they sort by age
static std::string historyPrefix(const fs::path& file)
{
    std::error_code ec;
    return file.stem().string() + "-" + calculateSha1(fs::absolute(file, ec).lexically_normal().string()).to_hex().substr(0, 8) + "-";
}

// Copies the content the disk file had when it was opened into the history folder, before it is saved over the first time
void SourceFile::writeHistory()
{
    _historyWritten = true;
    if (_historyDirectory.empty())
        return;
    std::error_code ec;
    const fs::path file(_diskFile);
    const fs::path directory(_historyDirectory);
    fs::create_directories(directory, ec);
    const auto prefix = historyPrefix(file);
    const auto now = std::chrono::system_clock::now();
    const auto seconds = std::chrono::system_clock::to_time_t(now);
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;
    char stamp[32];
    std::strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", std::localtime(&seconds));
    const auto target = directory / fmt::format("{}{}-{:03}{}", prefix, stamp, milliseconds, file.extension().string());
    {
        std::ofstream output(target.string(), std::ios::binary | std::ios::trunc);
        if (!output || !output.write(_openedContent.data(), static_cast<std::streamsize>(_openedContent.size()))) {
            TraceLog(LOG_WARNING, "Couldn't write the history copy %s", target.string().c_str());
            return;
        }
    }
    // only the newest copies of this file are kept
    std::vector<fs::path> copies;
    for (const auto& entry : fs::directory_iterator(directory, ec)) {
        const auto name = entry.path().filename().string();
        if (name.starts_with(prefix) && entry.path().extension() == file.extension())
            copies.push_back(entry.path());
    }
    std::ranges::sort(copies);
    for (size_t i = 0; i + MAX_HISTORY_VERSIONS < copies.size(); ++i)
        fs::remove(copies[i], ec);
}

bool SourceFile::canRevertToOpened() const
{
    return !_diskFile.empty() && _code.getText() != _openedText;
}

void SourceFile::revertToOpened()
{
    if (canRevertToOpened())
        replaceText(_openedText);
}

// Replaces the whole text as one undoable edit, the cursor stays in its line and column as far as possible
void SourceFile::replaceText(std::string text)
{
    const auto line = _code.line();
    const auto column = _code.column();
    _code.setText(std::move(text));
    _code.goTo(line, column);
}

void SourceFile::checkDiskFile()
{
#ifndef PLATFORM_WEB
    if (_diskFile.empty())
        return;
    std::error_code ec;
    const auto time = fs::last_write_time(_diskFile, ec);
    if (ec || time == _diskFileTime)
        return;
    std::string content, text;
    bool crLf = false;
    if (!readSource(_diskFile, content, text, crLf))
        return;
    if (text == _code.getText()) {
        // the same text (e.g. only touched), nothing to do
        _diskFileTime = time;
        _diskFileCrLf = crLf;
        return;
    }
    if (hasUnsavedChanges() || contentRevision() == _failedSaveRevision) {
        _changedOnDisk = true;
        _saveError = "The file was changed on disk, reload it (context menu) or keep yours (Sys+S).";
        return;
    }
    reloadFromDisk();
#endif
}

void SourceFile::reloadFromDisk()
{
    std::string content, text;
    bool crLf = false;
    if (_diskFile.empty() || !readSource(_diskFile, content, text, crLf))
        return;
    std::error_code ec;
    replaceText(std::move(text));
    _diskFileCrLf = crLf;
    _diskFileTime = fs::last_write_time(_diskFile, ec);
    _savedRevision = contentRevision();
    _failedSaveRevision = ~0u;
    _changedOnDisk = false;
    _saveError.clear();
    TraceLog(LOG_INFO, "Reloaded %s, it was changed on disk", _diskFile.c_str());
}
