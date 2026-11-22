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
#include <rlguipp/rlguipp_codeeditor.hpp>
#include <rlguipp/rlguipp_detail.hpp>

#include <ghc/utf8.hpp>

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cmath>

namespace gui {

namespace utf8 = ghc::utf8;

static std::pair<std::string::const_iterator, int> findSubstr(bool caseSense, const std::regex* regEx, std::string::const_iterator from, std::string::const_iterator to, std::string::const_iterator patternStart, std::string::const_iterator patternEnd)
{
    if(regEx) {
        auto iter = std::sregex_iterator(from, to, *regEx);
        if(iter ==  std::sregex_iterator())
            return {to, 0};
        return {from + iter->position(0), iter->length(0)};
    }
    else {
        std::string::const_iterator iter;
        if(caseSense)
            iter = std::search(from, to, patternStart, patternEnd);
        else
            iter = std::search(from, to, patternStart, patternEnd, [](char ch1, char ch2) { return std::toupper(ch1) == std::toupper(ch2); });
        if(iter == to)
            return {to, 0};
        return {iter, patternEnd - patternStart};
    }

}

void DrawCodeLine(std::string_view line, Vector2 position, float width, const CodeTextStyle& style, const SyntaxHighlighter* highlighter,
                  SyntaxHighlighter::LineState state, int firstColumn, size_t selectionStart, size_t selectionEnd)
{
    if (const auto lineBreak = line.find('\n'); lineBreak != std::string_view::npos)
        line = line.substr(0, lineBreak);
    static std::vector<uint8_t> styles;
    styles.assign(line.size(), 0);
    if (highlighter)
        highlighter->highlightLine(line, state, styles);
    const Font font = style.font.texture.id ? style.font : GuiGetFont();
    const Color plain = style.palette.empty() ? GetColor(GuiGetStyle(DEFAULT, TEXT_COLOR_NORMAL)) : style.palette.front();
    const float selectionY = position.y - (style.lineHeight - style.fontSize) / 2;
    const bool hasSelection = selectionStart < selectionEnd;
    const char* text = line.data();
    const char* end = text + line.size();
    float x = 0.0f;
    while (text < end && x < width) {
        const auto offset = static_cast<size_t>(text - line.data());
        const auto codepoint = static_cast<int>(utf8::fetchCodepoint(text, end));
        if (firstColumn <= 0) {
            if (hasSelection && offset >= selectionStart && offset < selectionEnd)
                DrawRectangleRec({position.x + x, selectionY, style.cellWidth, style.lineHeight}, style.selection);
            if (codepoint != ' ' && codepoint != '\t') {
                const auto id = styles[offset];
                DrawTextCodepointClipped(font, codepoint, {position.x + x, position.y}, style.fontSize, id < style.palette.size() ? style.palette[id] : plain);
            }
            x += style.cellWidth;
        }
        --firstColumn;
    }
    const auto offset = static_cast<size_t>(text - line.data());
    if (hasSelection && x < width && offset >= selectionStart && offset < selectionEnd)
        DrawRectangleRec({position.x + x, selectionY, width - x, style.lineHeight}, style.selection);
}

CodeEditor::CodeEditor(const char* id)
    : _focusId(GetId(id))
{
    updateLineInfo();
}

void CodeEditor::setText(std::string text)
{
    _selectionStart = 0;
    _selectionEnd = _text.size();
    insert(std::move(text));
    updateLineInfo();
    _losCol = _tosLine = _cursorX = _cursorY = 0;
    _cursorChanged = true;
}

void CodeEditor::clearHistory()
{
    _undoStack = std::stack<EditInfo>();
    _redoStack = std::stack<EditInfo>();
}

void CodeEditor::setHighlighter(const SyntaxHighlighter* highlighter)
{
    _highlighter = highlighter;
    updateHighlighting();
}

void CodeEditor::updateHighlighting()
{
    _lineStates.clear();
    updateLineInfo();
}

void CodeEditor::render(float height)
{
    const auto available = GetContentAvailable();
    _totalArea = {available.x, available.y, available.width, height > 0 ? height : available.height};
    PushId(this);
    ++_editId;
    _cursorChanged = false;
    _matchSelected = false;
    hasFocus();  // keeps the keyboard focus, also while the gui is locked
    bool selectionChange = false;
    if (!GuiIsLocked()) {
        if (IsShortcutPressed('F')) {
            _findOrReplace = _findOrReplace == eFIND ? eNONE : eFIND;
            _findUpdateRevision = ~0u;  // select the current match again when the bar opens
        }
        else if (IsShortcutPressed('R')) {
            _findOrReplace = _findOrReplace == eFIND_REPLACE ? eNONE : eFIND_REPLACE;
            _findUpdateRevision = ~0u;
        }
        else if (IsKeyPressed(KEY_ESCAPE)) {
            _selectionStart = _selectionEnd = 0;
            _findOrReplace = eNONE;
            _toolArea = {0, 0, 0, 0};
            selectionChange = true;
        }
    }
    _toolArea = drawToolArea();
    _textArea = {_totalArea.x, _totalArea.y + _toolArea.height, _totalArea.width, _totalArea.height - _toolArea.height};
    _visibleLines = uint32_t((_textArea.height - 6) / _textStyle.lineHeight);
    _visibleCols = uint32_t((_textArea.width - _lineNumberWidth - 6) / _textStyle.cellWidth);
    updateHoveredLink();
    if (!GuiIsLocked())
        selectionChange |= handleInput();
    if (_cursorChanged) {
        _blinkStart = GetTime();
        if (!selectionChange && !_matchSelected)
            _selectionStart = _selectionEnd = 0;
    }
    if (_cursorChanged || _scrollToCursor)
        ensureCursorVisibility();
    _scrollToCursor = false;
    if (_contentRevision != _lastSeenRevision) {
        _lastSeenRevision = _contentRevision;
        _blinkStart = GetTime();
    }
    _lineNumberCols = _lines.empty() ? 5 : std::max(3, int(std::log10(_lines.size())) + 3);
    _lineNumberWidth = _lineNumberCols * _textStyle.cellWidth;
    drawText();
    PopId();
    if (_linkToOpen) {
        // last, the link may lead to another editor
        auto open = std::move(_linkToOpen);
        _linkToOpen = nullptr;
        open();
    }
}

// The link under the mouse while Sys is held, the mouse cursor shows a pointing hand over it
void CodeEditor::updateHoveredLink()
{
    _hoveredLink.reset();
    if (_linkHandler && !GuiIsLocked() && IsSysKeyDown()) {
        if (const auto offset = characterAt(GetMousePosition()))
            _hoveredLink = linkAt(*offset);
    }
    if (_hoveredLink.has_value() != _linkCursor) {
        _linkCursor = _hoveredLink.has_value();
        SetMouseCursor(_linkCursor ? MOUSE_CURSOR_POINTING_HAND : MOUSE_CURSOR_DEFAULT);
    }
}

// The link the handler finds at the offset, if the offset is inside of it
std::optional<CodeLink> CodeEditor::linkAt(uint32_t offset) const
{
    if (!_linkHandler)
        return std::nullopt;
    auto link = _linkHandler(contextAt(offset));
    if (link && link->start <= offset && offset < link->end)
        return link;
    return std::nullopt;
}

// The mouse and keyboard input of the text area, returns true if it changed the selection. It works like in TextBoxMulti.
bool CodeEditor::handleInput()
{
    const bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    bool selectionChange = false;

    // Mouse: a click places the cursor (with shift it extends the selection), a double click selects a word, a triple click the
    // line, dragging after a single click selects, also beyond the text area, which scrolls it
    const auto mouse = GetMousePosition();
    const bool inText = CheckCollisionPointRec(mouse, {_textArea.x + 1, _textArea.y + 1, _textArea.width - 6, _textArea.height - 6});
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && inText) {
        // a right-click into the selection keeps it, elsewhere the cursor goes there first
        setFocus();
        const auto offset = offsetAt(mouse);
        const auto [start, end] = selection();
        if (start == end || offset < start || offset > end) {
            _typingRun = false;
            setCursor(offset);
            _selectionStart = _selectionEnd = 0;
        }
        else {
            selectionChange = true;
        }
        openContextMenu(mouse, characterAt(mouse).value_or(offset));
        return selectionChange;
    }
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && inText && _hoveredLink && _hoveredLink->open) {
        setFocus();
        _linkToOpen = _hoveredLink->open;
        return selectionChange;
    }
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && inText) {
        const bool wasFocused = hasFocus();
        setFocus();
        const double now = GetTime();
        if (now - _lastClickTime < detail::TEXT_MULTI_CLICK_TIME && std::abs(mouse.x - _lastClickPos.x) < 4 && std::abs(mouse.y - _lastClickPos.y) < 4)
            ++_clickCount;
        else
            _clickCount = 1;
        _lastClickTime = now;
        _lastClickPos = mouse;
        const auto anchor = _selectionStart != _selectionEnd ? _selectionStart : offsetFromCursor();
        setCursorFromMouse(mouse);
        const auto offset = offsetFromCursor();
        if (_clickCount == 2) {
            selectWord(offset);
        }
        else if (_clickCount >= 3) {
            selectLine(offset);
        }
        else {
            _selectionStart = wasFocused && shift ? anchor : offset;
            _selectionEnd = offset;
        }
        _mouseSelecting = _clickCount == 1;
        _typingRun = false;
        selectionChange = true;
    }
    else if (_mouseSelecting && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        const int oldX = _cursorX, oldY = _cursorY;
        setCursorFromMouse(mouse);
        _cursorChanged = _cursorX != oldX || _cursorY != oldY;
        _selectionEnd = offsetFromCursor();
        selectionChange = true;
    }
    if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT))
        _mouseSelecting = false;

    if (!hasFocus()) {
        _typingRun = false;
        return selectionChange;
    }
    if (_shortcutHandler && _shortcutHandler()) {
        _typingRun = false;
        return selectionChange;
    }
    // Keyboard, like TextBoxMulti: word movement with Alt (macOS) or Ctrl, line and text start/end with Cmd (macOS) or Home/End and Ctrl
    const bool mac = detail::isMacLike();
    const bool sys = IsSysKeyDown();
    const bool alt = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);
    const bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    const bool wordKey = mac ? alt : ctrl;
    const bool hasSelection = _selectionStart != _selectionEnd;
    const uint32_t cursor = offsetFromCursor();
    const auto textSize = static_cast<uint32_t>(_text.size());
    if (IsKeyPressed(KEY_KB_MENU) || (shift && IsKeyPressed(KEY_F10))) {
        openContextMenu();
        return true;
    }
    auto activated = [](int key) { return IsKeyPressed(key) || IsKeyPressedRepeat(key); };
    auto move = [&](auto&& f) { selectionChange |= cursorWrapper(f); };
    auto copySelection = [this]() {
        const auto [start, end] = selection();
        CopyToClipboard(_text.substr(start, end - start));
    };

    if (IsShortcutPressed('Z')) {
        if (shift)
            redo();
        else
            undo();
    }
    else if (!mac && IsShortcutPressed('Y')) {
        redo();
    }
    else if (IsShortcutPressed('C')) {
        if (hasSelection)
            copySelection();
    }
    else if (IsShortcutPressed('X')) {
        if (hasSelection) {
            copySelection();
            deleteSelectedText();
        }
    }
    else if (IsShortcutPressed('A')) {
        _typingRun = false;
        setCursor(textSize);
        _selectionStart = 0;
        _selectionEnd = textSize;
        selectionChange = true;
    }
    else if (activated(KEY_LEFT)) {
        if (hasSelection && !shift) {
            _typingRun = false;
            setCursor(selection().first);
        }
        else {
            move([&]() {
                if (mac && sys)
                    cursorHome();
                else if (wordKey)
                    setCursor(wordLeft(cursor));
                else
                    cursorLeft();
            });
        }
    }
    else if (activated(KEY_RIGHT)) {
        if (hasSelection && !shift) {
            _typingRun = false;
            setCursor(selection().second);
        }
        else {
            move([&]() {
                if (mac && sys)
                    cursorEnd();
                else if (wordKey)
                    setCursor(wordRight(cursor));
                else
                    cursorRight();
            });
        }
    }
    else if (activated(KEY_UP)) {
        move([&]() { mac && sys ? setCursor(0) : cursorUp(); });
    }
    else if (activated(KEY_DOWN)) {
        move([&]() { mac && sys ? setCursor(textSize) : cursorDown(); });
    }
    else if (activated(KEY_PAGE_UP)) {
        move([&]() { cursorUp(_visibleLines); });
    }
    else if (activated(KEY_PAGE_DOWN)) {
        move([&]() { cursorDown(_visibleLines); });
    }
    else if (IsKeyPressed(KEY_HOME)) {
        // Ctrl+Home/End went to the text start/end also on macOS, there it is Cmd+Up/Down like in the text widgets
        move([&]() { ctrl && !mac ? setCursor(0) : cursorHome(); });
    }
    else if (IsKeyPressed(KEY_END)) {
        move([&]() { ctrl && !mac ? setCursor(textSize) : cursorEnd(); });
    }
    else if (activated(KEY_BACKSPACE)) {
        if (mac && sys)
            deleteTo(_lines[_cursorY]);
        else
            deleteTo(wordKey ? wordLeft(cursor) : static_cast<uint32_t>(detail::prevOffset(_text, cursor)));
    }
    else if (activated(KEY_DELETE)) {
        deleteTo(wordKey ? wordRight(cursor) : static_cast<uint32_t>(detail::nextOffset(_text, cursor)));
    }
    else if (activated(KEY_TAB) && !ctrl) {  // Ctrl+Tab is for the application, e.g. to switch files
        auto numSpace = ((_cursorX / 4) + 1) * 4 - _cursorX;
        insert(std::string(numSpace, ' '));
    }
    else if (activated(KEY_ENTER) || activated(KEY_KP_ENTER)) {
        insert("\n");
    }
    if (IsPasteRequested())
        insert(PasteFromClipboard());
    // all characters typed in this frame, raylib drops the ones not read
    while (auto codepoint = GetCharPressed()) {
        if (codepoint >= 32 && codepoint != 127 && !(mac && sys))
            typeCharacter(codepoint);
    }
    return selectionChange;
}

// Typing is undone word by word: a typed character continues the undo step of the ones typed before it, unless it starts a word
// after a space, the typing paused for more than TYPING_PAUSE seconds, the cursor was moved or something is selected. Unlike
// in the text widgets, where all characters typed one after another are one undo step.
void CodeEditor::typeCharacter(int codepoint)
{
    const double now = GetTime();
    const bool wordStart = codepoint != ' ' && codepoint != '\t' && classBefore(offsetFromCursor()) == 1;
    if (!_typingRun || _selectionStart != _selectionEnd || wordStart || now - _lastTypingTime > TYPING_PAUSE)
        ++_undoStep;
    _typingRun = true;
    _lastTypingTime = now;
    std::string text;
    utf8::append(text, codepoint);
    insertText(std::move(text));
}

// Every edit but typing is an undo step of its own
void CodeEditor::startUndoStep()
{
    ++_undoStep;
    _typingRun = false;
}

void CodeEditor::setCursor(uint32_t offset)
{
    auto [cx, cy] = cursorFromOffset(std::min<uint32_t>(offset, _text.size()));
    _cursorX = _cursorVirtX = cx;
    _cursorY = cy;
    _cursorChanged = true;
}

// Moves the cursor to the character boundary nearest to the mouse, left of the text to the line start, below it to its end
void CodeEditor::setCursorFromMouse(Vector2 mouse)
{
    setCursor(offsetAt(mouse));
}

// The offset of the character boundary nearest to the mouse, left of the text the line start, below it the end of the text
uint32_t CodeEditor::offsetAt(Vector2 mouse) const
{
    const float column = (mouse.x - _textArea.x - _lineNumberWidth) / _textStyle.cellWidth;
    const int row = int(std::floor((mouse.y - _textArea.y - 2 * textPadding()) / _textStyle.lineHeight)) + _tosLine;
    if (row >= int(_lines.size()))
        return static_cast<uint32_t>(_text.size());
    const int line = std::max(row, 0);
    return offsetOf(line, std::clamp(int(std::floor(column + 0.5f)) + _losCol, 0, lineLength(line)));
}

// The offset of the character under the mouse, none beside the text of a line
std::optional<uint32_t> CodeEditor::characterAt(Vector2 mouse) const
{
    if (!CheckCollisionPointRec(mouse, _textArea))
        return std::nullopt;
    const float column = std::floor((mouse.x - _textArea.x - _lineNumberWidth) / _textStyle.cellWidth) + float(_losCol);
    const int row = int(std::floor((mouse.y - _textArea.y - 2 * textPadding()) / _textStyle.lineHeight)) + _tosLine;
    if (column < 0 || row < 0 || row >= int(_lines.size()) || column >= float(lineLength(row)))
        return std::nullopt;
    return offsetOf(row, int(column));
}

// The offset of a column (in code points, 0 based) of a line (0 based)
uint32_t CodeEditor::offsetOf(int line, int column) const
{
    const char* text = _text.data() + _lines[line];
    const char* end = _text.data() + _text.size();
    for (int i = 0; text < end && i < column && *text != '\n'; ++i)
        utf8::fetchCodepoint(text, end);
    return static_cast<uint32_t>(text - _text.data());
}

void CodeEditor::openContextMenu()
{
    const float lineHeight = _textStyle.lineHeight;
    const float lineTop = _textArea.y + float(_cursorY - _tosLine) * lineHeight + lineHeight - 3 * textPadding();
    const float x = _textArea.x + _lineNumberWidth + float(_cursorX - _losCol) * _textStyle.cellWidth;
    openContextMenu({std::clamp(x, _textArea.x, _textArea.x + _textArea.width), std::clamp(lineTop + lineHeight, _textArea.y, _textArea.y + _textArea.height)}, offsetFromCursor());
}

void CodeEditor::openContextMenu(Vector2 position, uint32_t offset)
{
    auto keys = [](const char* key, bool shift = false) { return ShortcutText(key, shift ? SHORTCUT_SYS | SHORTCUT_SHIFT : SHORTCUT_SYS); };
    auto enabledIf = [](PopupMenu::Item item, bool enabled) { return enabled ? std::move(item) : PopupMenu::Disabled(std::move(item)); };
    const bool hasSelection = _selectionStart != _selectionEnd;
    PopupMenu::Menu menu{
        enabledIf(PopupMenu::Action("Undo", [this]() { undo(); _scrollToCursor = true; }, keys("Z")), canUndo()),
        enabledIf(PopupMenu::Action("Redo", [this]() { redo(); _scrollToCursor = true; }, keys("Z", true)), !_redoStack.empty()),
        PopupMenu::Separator(),
        enabledIf(PopupMenu::Action("Cut", [this]() { CopyToClipboard(selectedText()); deleteSelectedText(); _scrollToCursor = true; }, keys("X")), hasSelection),
        enabledIf(PopupMenu::Action("Copy", [this]() { CopyToClipboard(selectedText()); }, keys("C")), hasSelection),
#ifndef __EMSCRIPTEN__
        // in the browser the clipboard text only arrives with the paste event of the shortcut
        PopupMenu::Action("Paste", [this]() { insert(PasteFromClipboard()); _scrollToCursor = true; }, keys("V")),
#endif
        PopupMenu::Action("Select All", [this]() { select(0, static_cast<uint32_t>(_text.size())); }, keys("A")),
        PopupMenu::Separator(),
        PopupMenu::Action("Find...", [this]() { openFindBar(eFIND); }, keys("F")),
        PopupMenu::Action("Replace...", [this]() { openFindBar(eFIND_REPLACE); }, keys("R")),
    };
    if (auto link = linkAt(offset); link && link->open && !link->label.empty()) {
        menu.insert(menu.begin(), {PopupMenu::Action(link->label, link->open), PopupMenu::Separator()});
    }
    if (_contextMenuHandler)
        _contextMenuHandler(menu, contextAt(offset));
    if (!menu.empty())
        PopupMenu::Open(position, menu);
}

void CodeEditor::openFindBar(FindReplaceMode mode)
{
    _findOrReplace = mode;
    _findUpdateRevision = ~0u;  // select the current match again when the bar opens
}

int CodeEditor::classAt(uint32_t offset) const
{
    if (offset >= _text.size())
        return -1;
    int size = 0;
    return detail::charClass(GetCodepointNext(_text.c_str() + offset, &size));
}

int CodeEditor::classBefore(uint32_t offset) const
{
    return offset == 0 ? -1 : classAt(static_cast<uint32_t>(detail::prevOffset(_text, offset)));
}

// The start of the word before the offset, spaces before it are skipped, a line break is a word of its own
uint32_t CodeEditor::wordLeft(uint32_t offset) const
{
    while (offset > 0 && classBefore(offset) == 1)
        offset = static_cast<uint32_t>(detail::prevOffset(_text, offset));
    const int wordClass = classBefore(offset);
    if (wordClass == 0)
        return static_cast<uint32_t>(detail::prevOffset(_text, offset));
    while (offset > 0 && classBefore(offset) == wordClass)
        offset = static_cast<uint32_t>(detail::prevOffset(_text, offset));
    return offset;
}

// The start of the next word, the spaces behind a word are skipped
uint32_t CodeEditor::wordRight(uint32_t offset) const
{
    const int wordClass = classAt(offset);
    if (wordClass == 0)
        return static_cast<uint32_t>(detail::nextOffset(_text, offset));
    if (wordClass > 1) {
        while (offset < _text.size() && classAt(offset) == wordClass)
            offset = static_cast<uint32_t>(detail::nextOffset(_text, offset));
    }
    while (offset < _text.size() && classAt(offset) == 1)
        offset = static_cast<uint32_t>(detail::nextOffset(_text, offset));
    return offset;
}

// The word at the offset, or if there is none, the one ending there (both by the character classes of the word movement),
// equal offsets if there is no word
std::pair<uint32_t, uint32_t> CodeEditor::wordRange(uint32_t offset) const
{
    int wordClass = classAt(offset);
    if (wordClass <= 0) {
        wordClass = classBefore(offset);
        if (wordClass <= 0)
            return {offset, offset};
        offset = static_cast<uint32_t>(detail::prevOffset(_text, offset));
    }
    uint32_t start = offset;
    uint32_t end = offset;
    while (start > 0 && classBefore(start) == wordClass)
        start = static_cast<uint32_t>(detail::prevOffset(_text, start));
    while (end < _text.size() && classAt(end) == wordClass)
        end = static_cast<uint32_t>(detail::nextOffset(_text, end));
    return {start, end};
}

void CodeEditor::selectWord(uint32_t offset)
{
    const auto [start, end] = wordRange(offset);
    if (start == end) {
        _selectionStart = _selectionEnd = offset;
        return;
    }
    setCursor(end);
    _selectionStart = start;
    _selectionEnd = end;
}

uint32_t CodeEditor::toCodepointStart(uint32_t offset) const
{
    offset = std::min<uint32_t>(offset, _text.size());
    while (offset > 0 && offset < _text.size() && (static_cast<unsigned char>(_text[offset]) & 0xC0) == 0x80)
        --offset;
    return offset;
}

std::string CodeEditor::selectedText() const
{
    const auto [start, end] = selection();
    return _text.substr(start, end - start);
}

void CodeEditor::select(uint32_t start, uint32_t end)
{
    start = toCodepointStart(start);
    end = toCodepointStart(end);
    _typingRun = false;
    setCursor(end);
    _selectionStart = start;
    _selectionEnd = end;
    _scrollToCursor = true;
}

void CodeEditor::goTo(uint32_t line, uint32_t column)
{
    _typingRun = false;
    _cursorY = std::clamp(int(line) - 1, 0, int(_lines.size()) - 1);
    _cursorX = _cursorVirtX = std::clamp(int(column) - 1, 0, lineLength(_cursorY));
    _selectionStart = _selectionEnd = 0;
    _cursorChanged = _scrollToCursor = true;
}

void CodeEditor::replaceRange(uint32_t start, uint32_t end, std::string text)
{
    start = toCodepointStart(start);
    end = toCodepointStart(end);
    if (end < start)
        std::swap(start, end);
    setCursor(start);
    _selectionStart = start;
    _selectionEnd = end;
    insert(std::move(text));
    _scrollToCursor = true;
}

CodeContext CodeEditor::contextAt(uint32_t offset) const
{
    CodeContext context;
    context.offset = toCodepointStart(offset);
    const auto [column, line] = cursorFromOffset(context.offset);
    context.line = line + 1;
    context.column = column + 1;
    context.selection = selection();
    context.word = wordRange(context.offset);
    const uint32_t lineStart = _lines[line];
    const uint32_t lineEnd = line + 1 < _lines.size() ? _lines[line + 1] - 1 : static_cast<uint32_t>(_text.size());
    context.lineText = std::string_view(_text).substr(lineStart, lineEnd - lineStart);
    if (_highlighter && context.offset < lineEnd) {
        std::vector<uint8_t> styles(context.lineText.size(), 0);
        _highlighter->highlightLine(context.lineText, _lineStates[line], styles);
        context.style = styles[context.offset - lineStart];
    }
    return context;
}

// The line of the offset without its line break
void CodeEditor::selectLine(uint32_t offset)
{
    const auto line = cursorFromOffset(offset).second;
    const auto start = _lines[line];
    const auto end = static_cast<uint32_t>(lineEnd(line) - _text.data()) - (line + 1 < _lines.size() ? 1 : 0);
    setCursor(end);
    _selectionStart = start;
    _selectionEnd = end;
}

// Deletes the selection, or if there is none, the text between the cursor and the offset
void CodeEditor::deleteTo(uint32_t offset)
{
    startUndoStep();
    if (_selectionStart != _selectionEnd) {
        eraseSelection();
    }
    else {
        const uint32_t cursor = offsetFromCursor();
        const uint32_t target = std::min<uint32_t>(offset, _text.size());
        const uint32_t start = std::min(cursor, target);
        deleteText(start, std::max(cursor, target) - start);
    }
    _cursorChanged = true;
}

void CodeEditor::drawText()
{
    int lineNumber = int(_tosLine) - 1;
    const float lineHeight = _textStyle.lineHeight;
    const float cellWidth = _textStyle.cellWidth;
    const Font font = _textStyle.font.texture.id ? _textStyle.font : GuiGetFont();
    float ypos = _textArea.y - 2 * textPadding();
    _scrollPos = {-(float)_losCol * cellWidth, -(float)_tosLine * lineHeight};
    const int borderWidth = GetStyle(DEFAULT, BORDER_WIDTH);
    SetStyle(DEFAULT, BORDER_WIDTH, 0);
    BeginScrollPanel(_textArea.height, {0,0,std::max(_textArea.width, (float)(_longestLineSize+8) * cellWidth), std::max(_textArea.height, float(_lines.size()+1) * lineHeight)}, &_scrollPos);
    SetStyle(DEFAULT, BORDER_WIDTH, borderWidth);
    DrawRectangleClipped(_textArea.x + _lineNumberWidth - cellWidth / 2, _textArea.y, 1, _textArea.height, GetColor(GetStyle(DEFAULT,BORDER_COLOR_NORMAL)));
    const auto textColor = GetColor(GetStyle(DEFAULT, TEXT_COLOR_NORMAL));
    const auto [selStart, selEnd] = selection();
    while(lineNumber < int(_lines.size()) && ypos < _textArea.y + _textArea.height) {
        if(lineNumber >= 0) {
            DrawTextClipped(font, TextFormat("%*d", int(_lineNumberCols - 1), lineNumber + 1), {_textArea.x, ypos}, textColor);
            const auto lineOffset = _lines[lineNumber];
            const std::string_view line(lineStart(lineNumber), lineEnd(lineNumber) - lineStart(lineNumber));
            DrawCodeLine(line, {_textArea.x + _lineNumberWidth, ypos}, _textArea.width - _lineNumberWidth, _textStyle, _highlighter, _lineStates[lineNumber], _losCol,
                         selStart > lineOffset ? selStart - lineOffset : 0, selEnd > lineOffset ? selEnd - lineOffset : 0);
            if (_hoveredLink && _hoveredLink->start < lineOffset + line.size() && _hoveredLink->end > lineOffset) {
                // the part of the link in this line, underlined
                const auto [startColumn, startLine] = cursorFromOffset(std::max<uint32_t>(_hoveredLink->start, lineOffset));
                const auto [endColumn, endLine] = cursorFromOffset(std::min<uint32_t>(_hoveredLink->end, static_cast<uint32_t>(lineOffset + line.size())));
                const float x1 = std::max(0.0f, float(startColumn - _losCol)) * cellWidth;
                const float x2 = float(endColumn - _losCol) * cellWidth;
                if (x2 > x1)
                    DrawRectangleClipped(_textArea.x + _lineNumberWidth + x1, ypos + _textStyle.fontSize, x2 - x1, 1, GetColor(GetStyle(DEFAULT, TEXT_COLOR_FOCUSED)));
            }
        }
        ++lineNumber;
        ypos += lineHeight;
    }
    if(hasFocus() && std::fmod(GetTime() - _blinkStart, 2 * detail::TEXT_CURSOR_BLINK_TIME) < detail::TEXT_CURSOR_BLINK_TIME) {
        auto cx = (_cursorX - _losCol) * cellWidth;
        auto cy = (_cursorY - _tosLine) * lineHeight + lineHeight - 2 * textPadding();
        if(cx >= 0 && cx < _textArea.width - _lineNumberWidth - 3 && cy >= 0 && cy + _textStyle.fontSize < _textArea.height) {
            DrawRectangleClipped(_textArea.x + _lineNumberWidth + cx, _textArea.y + cy - textPadding(), 2, lineHeight, GetColor(GetStyle(DEFAULT, TEXT_COLOR_FOCUSED)));
        }
    }
    EndScrollPanel();
    _tosLine = -_scrollPos.y / lineHeight;
    _losCol = -_scrollPos.x / cellWidth;
}

void CodeEditor::fixLinefeed(std::string& text)
{
    size_t pos = 0;
    while ((pos = text.find("\r\n", pos)) != std::string::npos) {
        text.replace(pos, 2, "\n");
        pos += 1;
    }
}

void CodeEditor::setFocus()
{
    gui::SetKeyboardFocus(_focusId);
}

bool CodeEditor::hasFocus() const
{
    return gui::HasKeyboardFocus(_focusId);
}

void CodeEditor::cursorLeft(int steps)
{
    for (int i = 0; i < steps; ++i) {
        if (_cursorX > 0)
            --_cursorX;
        else if (_cursorY > 0)
            --_cursorY, _cursorX = lineLength(_cursorY);
    }
    _cursorVirtX = _cursorX;
    _cursorChanged = true;
}

void CodeEditor::cursorRight(int steps)
{
    for (int i = 0; i < steps; ++i) {
        if (_cursorX < lineLength(_cursorY))
            ++_cursorX;
        else if (_cursorY + 1 < _lines.size())
            ++_cursorY, _cursorX = 0;
    }
    _cursorVirtX = _cursorX;
    _cursorChanged = true;
}

void CodeEditor::cursorUp(int steps)
{
    for (int i = 0; i < steps; ++i) {
        if (_cursorY > 0)
            --_cursorY;
    }
    _cursorX = _cursorVirtX;
    if (_cursorX > lineLength(_cursorY))
        _cursorX = lineLength(_cursorY);
    _cursorChanged = true;
}

void CodeEditor::cursorDown(int steps)
{
    for (int i = 0; i < steps; ++i) {
        if (_cursorY + 1 < _lines.size())
            ++_cursorY;
    }
    _cursorX = _cursorVirtX;
    if (_cursorX > lineLength(_cursorY))
        _cursorX = lineLength(_cursorY);
    _cursorChanged = true;
}

void CodeEditor::cursorHome()
{
    _cursorX = _cursorVirtX = 0;
    _cursorChanged = true;
}

void CodeEditor::cursorEnd()
{
    _cursorX = _cursorVirtX = lineLength(_cursorY);
    _cursorChanged = true;
}

uint32_t CodeEditor::offsetFromCursor() const
{
    const char* text = _text.data() + _lines[_cursorY];
    const char* end = _text.data() + _text.size();
    unsigned i = 0;
    while(text < end && i < _cursorX && *text != '\n')
        utf8::fetchCodepoint(text, end), ++i;
    return text - _text.data();
}

std::pair<int, int> CodeEditor::cursorFromOffset(uint32_t offset) const
{
    int cx = 0, cy = 0;
    auto iter=std::upper_bound(_lines.begin(), _lines.end(), offset);
    if (iter!=_lines.begin()) {
        --iter;
    }
    cy = int(iter - _lines.begin());
    offset -= *iter;
    const char* text = _text.data() + *iter;
    const char* end = text + offset;
    while(text < end && *text != '\n')
        utf8::fetchCodepoint(text, end), ++cx;
    return {cx, cy};
}

int CodeEditor::lineLength(uint32_t line) const
{
    if(line >= _lines.size()) return 0;
    const char* text = _text.data() + _lines[line];
    const char* end = _text.data() + _text.size();
    int len = 0;
    while(text < end && *text != '\n')
        utf8::fetchCodepoint(text, end), ++len;
    return len;
}

void CodeEditor::updateLineInfo(uint32_t fromLine)
{
    if (fromLine >= _lines.size())
        fromLine = 0;
    if (fromLine == 0)
        _longestLineSize = 0;
    const char* str = _text.data() + (fromLine ? _lines[fromLine] : 0);
    const char* end = _text.data() + _text.size();
    auto state = fromLine < _lineStates.size() ? _lineStates[fromLine] : gui::SyntaxHighlighter::LineState{};
    _lines.erase(_lines.begin() + fromLine, _lines.end());
    _lineStates.resize(fromLine);
    auto length = str - _text.data();
    auto lastOffset = length;
    _lines.push_back(length);
    _lineStates.push_back(state);
    while (str < end) {
        if (*str++ == '\n') {
            if (_highlighter)
                state = _highlighter->nextLineState(std::string_view(_text.data() + lastOffset, str - 1 - (_text.data() + lastOffset)), state);
            _lines.push_back(str - _text.data());
            _lineStates.push_back(state);
            length = _lines.back() - lastOffset;
            if(length > _longestLineSize)
                _longestLineSize = length;
            lastOffset = _lines.back();
        }
    }
}

void CodeEditor::ensureCursorVisibility()
{
    if (_visibleLines && _cursorY >= _tosLine + _visibleLines - 1)
        _tosLine = _cursorY - _visibleLines + 2;
    else if (_visibleLines && _cursorY < _tosLine)
        _tosLine = _cursorY;
    if (_visibleCols && _cursorX >= _losCol + _visibleCols)
        _losCol = _cursorX - _visibleCols + 1;
    else if (_visibleCols && _cursorX < _losCol)
        _losCol = _cursorX;
}

void CodeEditor::safeInsert(uint32_t offset, const std::string& text)
{
    assert(offset <= _text.size() && "Text offset is actually in text");
    if(offset > _text.size()) {
        TraceLog(LOG_ERROR, "Trying to insert after end at offset: %d, (size: %d)", offset, (uint32_t)_text.size());
    }
    else {
        _text.insert(offset, text);
        ++_contentRevision;
    }
}

void CodeEditor::safeErase(uint32_t offset, uint32_t length)
{
    assert(offset <= _text.size() && "Text offset is actually in text");
    if(offset > _text.size()) {
        TraceLog(LOG_ERROR, "Trying to erase after end at offset: %d, (size: %d)", offset, (uint32_t)_text.size());
        return;
    }
    else if(offset + length > _text.size()) {
        TraceLog(LOG_WARNING, "Trying to erase until after end at offset: %d, length: %d, (size: %d)", offset, length, (uint32_t)_text.size());
    }
    _text.erase(offset, length);
    ++_contentRevision;
}

void CodeEditor::deleteSelectedText()
{
    startUndoStep();
    eraseSelection();
}

void CodeEditor::eraseSelection()
{
    if(_selectionStart != _selectionEnd) {
        uint32_t selStart = _selectionStart > _selectionEnd ? _selectionEnd : _selectionStart;
        uint32_t selEnd = _selectionStart > _selectionEnd ? _selectionStart : _selectionEnd;
        deleteText(selStart, selEnd - selStart);
    }
}

void CodeEditor::deleteText(uint32_t offset, uint32_t length)
{
    if(length) {
        auto [cx, cy] = cursorFromOffset(offset);
        _undoStack.push({eDELETE, _undoStep, offsetFromCursor(), offset, offset + length, std::string(_text.begin() + offset, _text.begin() + offset + length)});
        safeErase(offset, length);
        _cursorX = _cursorVirtX = cx;
        _cursorY = cy;
        _selectionStart = _selectionEnd = 0;
        updateLineInfo(cy);
        _redoStack = std::stack<EditInfo>();
    }
}

uint32_t CodeEditor::insert(std::string text)
{
    startUndoStep();
    return insertText(std::move(text));
}

uint32_t CodeEditor::insertText(std::string text)
{
    fixLinefeed(text);
    eraseSelection();
    auto offset = offsetFromCursor();
    _undoStack.push({eINSERT, _undoStep, offsetFromCursor(), offset, offset, text});
    safeInsert(offset, text);
    updateLineInfo(_cursorY);
    cursorRight(utf8::length(text));
    _redoStack = std::stack<EditInfo>();
    return text.size();
}

void CodeEditor::undo()
{
    _typingRun = false;
    _selectionStart = _selectionEnd = 0;
    if(_undoStack.empty()) return;
    auto id = _undoStack.top().id;
    while(!_undoStack.empty() && _undoStack.top().id == id) {
        auto op = _undoStack.top().operation;
        if(op == eDELETE) {
            safeInsert(_undoStack.top().startOffset, _undoStack.top().text);
            auto startLine = cursorFromOffset(_undoStack.top().startOffset).second;
            auto [cx, cy] = cursorFromOffset(_undoStack.top().cursorPos);
            _cursorX = _cursorVirtX = cx; _cursorY = cy;
            _cursorChanged = true;
            updateLineInfo(startLine);
        }
        else if(op == eINSERT) {
            safeErase(_undoStack.top().startOffset, _undoStack.top().text.length());
            auto [cx, cy] = cursorFromOffset(_undoStack.top().cursorPos);
            _cursorX = _cursorVirtX = cx; _cursorY = cy;
            _cursorChanged = true;
            updateLineInfo(_cursorY);
        }
        _redoStack.push(_undoStack.top());
        _undoStack.pop();
    }
}

void CodeEditor::redo()
{
    _typingRun = false;
    _selectionStart = _selectionEnd = 0;
    if(_redoStack.empty()) return;
    auto id = _redoStack.top().id;
    while(!_redoStack.empty() && _redoStack.top().id == id) {
        auto op = _redoStack.top().operation;
        if(op == eDELETE) {
            safeErase(_redoStack.top().startOffset, _redoStack.top().endOffset - _redoStack.top().startOffset);
            auto [cx, cy] = cursorFromOffset(_redoStack.top().startOffset);
            _cursorX = _cursorVirtX = cx; _cursorY = cy;
            _cursorChanged = true;
            updateLineInfo(_cursorY);
        }
        else if(op == eINSERT) {
            safeInsert(_redoStack.top().startOffset, _redoStack.top().text);
            auto startLine = cursorFromOffset(_redoStack.top().startOffset).second;
            auto [cx, cy] = cursorFromOffset(_redoStack.top().startOffset + _redoStack.top().text.length());
            _cursorX = _cursorVirtX = cx; _cursorY = cy;
            _cursorChanged = true;
            updateLineInfo(startLine);
        }
        _undoStack.push(_redoStack.top());
        _redoStack.pop();
    }
}

Rectangle CodeEditor::drawToolArea()
{
    Rectangle toolArea{};
    bool toolOpened = false;
    switch (_findOrReplace) {
        case eNONE:
            toolArea = {0,0,0,0};
            break;
        case eFIND:
            toolOpened = _toolArea.height == 0;
            toolArea = {_totalArea.x, _totalArea.y, _totalArea.width, 18};
            break;
        case eFIND_REPLACE:
            toolOpened = _toolArea.height == 0;
            toolArea = {_totalArea.x, _totalArea.y, _totalArea.width, 36};
            break;
    }
    if(_findOrReplace != eNONE) {
        SetRowHeight(18);
        BeginColumns();
        SetSpacing(0);
        SetNextWidth(18);
        Button(GuiIconText(ICON_LENS_BIG,""));
        SetNextWidth(toolArea.width - 18*5);
        {
            const auto textColorPressed = GetStyle(DEFAULT, TEXT_COLOR_PRESSED);
            if (_findRegex && !_findRegexValid)
                SetStyle(DEFAULT, TEXT_COLOR_PRESSED, ColorToInt(RED));
            if (TextBox("find", _findString, 4096))
                updateFindResults();
            if (toolOpened)
                SetKeyboardFocus(GetId("find"));
            SetStyle(DEFAULT, TEXT_COLOR_PRESSED, textColorPressed);
        }
        SetNextWidth(18);
        if (Toggle("Aa", &_findCaseSensitive))
            updateFindResults();
        SetNextWidth(18);
        if (Toggle(".*", &_findRegex))
            updateFindResults();
        SetNextWidth(18);
        if(!_findResults || _findCurrentResult == _findResults) GuiDisable();
        if(Button(GuiIconText(ICON_ARROW_DOWN,"")) && _findCurrentResult < _findResults) {
            ++_findCurrentResult;
            updateFindResults();
        }
        GuiEnable();
        SetNextWidth(18);
        if(!_findResults || _findCurrentResult == 1) GuiDisable();
        if(Button(GuiIconText(ICON_ARROW_UP,"")) && _findCurrentResult > 1) {
            --_findCurrentResult;
            updateFindResults();
        }
        GuiEnable();
        EndColumns();
        if(_findUpdateRevision != _contentRevision)
            updateFindResults();
    }
    if(_findOrReplace == eFIND_REPLACE) {
        SetRowHeight(18);
        BeginColumns();
        SetSpacing(0);
        SetNextWidth(18);
        Button("R");
        SetNextWidth(toolArea.width - 96 - 18);
        if(TextBox("replace", _replaceString, 4096))
            updateFindResults();
        SetNextWidth(48);
        if(!_findResults || _findCurrentOffset != _selectionStart || _selectionEnd - _selectionStart != _findCurrentLength)
            GuiDisable();
        if(Button("Replace") && _findCurrentOffset == _selectionStart && _selectionEnd - _selectionStart == _findCurrentLength) {
            insert(_replaceString);
            updateFindResults();
        }
        GuiEnable();
        SetNextWidth(48);
        if(!_findResults)
            GuiDisable();
        if(Button("Rep.all") && _findResults)
            replaceAll();
        GuiEnable();
        EndColumns();
        if(IsKeyPressed(KEY_TAB)) {
            if(HasKeyboardFocus(GetId("find")))
                SetKeyboardFocus(GetId("replace"));
            else if(HasKeyboardFocus(GetId("replace")))
                SetKeyboardFocus(GetId("find"));
        }
    }
    return toolArea;
}

void CodeEditor::updateFindResults()
{
    _findResults = 0;
    _findCurrentLength = 0;
    _findCurrentOffset = 0;
    _selectionStart = _selectionEnd = 0;
    _findUpdateRevision = _contentRevision;
    if(_findString.empty())
        return;
    if(_findRegex) {
        if(_findString != _findRegExString || _findRegExCaseSensitive != _findCaseSensitive) {
            _findRegExString = _findString;
            _findRegExCaseSensitive = _findCaseSensitive;
            try {
                if (_findCaseSensitive)
                    _findRegEx = std::regex(_findString);
                else
                    _findRegEx = std::regex(_findString, std::regex::icase);
                _findRegexValid = true;
            }
            catch (...)
            {
                _findRegexValid = false;
            }
        }
    }
    for (const auto& [offset, length] : findMatches()) {
        ++_findResults;
        if (_findCurrentResult <= 0)
            _findCurrentResult = _findResults;
        if (_findCurrentResult == _findResults) {
            _selectionStart = _findCurrentOffset = offset;
            _findCurrentLength = length;
            _selectionEnd = _selectionStart + _findCurrentLength;
            _matchSelected = true;
            auto [cx, cy] = cursorFromOffset(_selectionStart);
            _cursorX = _cursorVirtX = cx; _cursorY = cy;
            ensureCursorVisibility();
        }
    }
    if(_findCurrentResult > _findResults)
        _findCurrentResult = _findResults;
}

// The offsets and lengths of all matches of the find text, updateFindResults() prepares the regular expression
std::vector<std::pair<uint32_t, uint32_t>> CodeEditor::findMatches() const
{
    std::vector<std::pair<uint32_t, uint32_t>> matches;
    if (_findString.empty())
        return matches;
    auto iter = _text.cbegin();
    int len = 0;
    while (true) {
        std::tie(iter, len) = findSubstr(_findCaseSensitive, _findRegex && _findRegexValid ? &_findRegEx : nullptr, iter, _text.cend(), _findString.cbegin(), _findString.cend());
        if (!len)
            break;
        matches.emplace_back(static_cast<uint32_t>(iter - _text.cbegin()), static_cast<uint32_t>(len));
        iter += len;
    }
    return matches;
}

// Replaces all matches as one undo step, the last one first, so the offsets of the others stay valid
void CodeEditor::replaceAll()
{
    const auto matches = findMatches();
    if (matches.empty())
        return;
    startUndoStep();
    for (auto iter = matches.rbegin(); iter != matches.rend(); ++iter) {
        _selectionStart = iter->first;
        _selectionEnd = iter->first + iter->second;
        insertText(_replaceString);
    }
    _cursorChanged = true;
    updateFindResults();
}

}  // namespace gui
