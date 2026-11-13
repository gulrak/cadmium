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
// Syntax highlighted code
//---------------------------------------------------------------------------------------
#pragma once

#include <rlguipp/rlguipp.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <regex>
#include <span>
#include <stack>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace gui {

// Gives each byte of a line of code a style id, the palette of the CodeTextStyle turns it into a color (0 is plain text).
// Lines are given without their line break. Only the style of the first byte of a UTF-8 sequence is used.
class SyntaxHighlighter
{
public:
    // What the highlighting carries from one line to the next (like being inside a block comment), 0 at the start of the text
    using LineState = uint32_t;

    virtual ~SyntaxHighlighter() = default;

    // The state at the start of the line after this one. An editor calls it for every line behind an edit, so it should be cheap.
    virtual LineState nextLineState(std::string_view line, LineState state) const
    {
        (void)line;
        return state;
    }

    // Sets the style ids of the bytes of the line that starts in state, styles has one entry per byte, all 0 at the call
    virtual void highlightLine(std::string_view line, LineState state, std::span<uint8_t> styles) const = 0;
};

// How code is drawn. The font must be monospaced, every codepoint advances by cellWidth.
struct CodeTextStyle
{
    Font font{};                         // texture.id == 0 uses the gui font
    float fontSize{8};
    float cellWidth{6};
    float lineHeight{12};                // the selection background is this high, centered on the text
    std::vector<Color> palette;          // indexed by style id, an id without an entry uses the first one, an empty palette the gui text color
    Color selection{100, 100, 120, 255};
};

// Draws a line of code (a line break ends it) at position, highlighted by highlighter from the state of the line (nullptr draws
// plain text). The first firstColumn codepoints are skipped and nothing is drawn beyond width. The bytes from selectionStart to
// selectionEnd (offsets into the line) get the selection color behind them, a selection that goes past the end of the line also
// fills the rest of the width.
RLGUIPP_API void DrawCodeLine(std::string_view line, Vector2 position, float width, const CodeTextStyle& style,
                              const SyntaxHighlighter* highlighter = nullptr, SyntaxHighlighter::LineState state = 0,
                              int firstColumn = 0, size_t selectionStart = 0, size_t selectionEnd = 0);

// Where something is in the text of a CodeEditor, as CodeEditor::contextAt() describes it
struct CodeContext
{
    uint32_t offset{0};                         // byte offset into the text
    uint32_t line{1};                           // 1 based, like CodeEditor::line()
    uint32_t column{1};                         // 1 based, counted in code points
    std::pair<uint32_t, uint32_t> selection{};  // the selection of the editor (byte offsets, start <= end, equal if nothing is selected)
    std::pair<uint32_t, uint32_t> word{};       // the word at the offset by the double click rule, equal offsets if there is none
    std::string_view lineText;                  // the line without its line break, only valid until the text changes
    uint8_t style{0};                           // the style id the highlighter gives the character at the offset
};

// A part of the text that leads somewhere, as a link handler of a CodeEditor finds it (e.g. the file name of an include)
struct CodeLink
{
    uint32_t start{0};           // the byte range of the link, it is underlined while the mouse is over it with Sys held
    uint32_t end{0};
    std::string label;           // the entry of the context menu, empty for none
    std::function<void()> open;  // what Sys+click and the menu entry do
};

// A code editor with line numbers, syntax highlighting and a find (Sys+F) and replace (Sys+R) bar. The editing works like in
// TextBoxMulti (keys, word movement, double and triple click, the clipboard of the text widgets), but typing is undone word by
// word, lines are not wrapped, and Tab inserts spaces to the next multiple of four columns. The undo history keeps the edits, not
// copies of the text. The editor keeps the text and its editing state, render() shows it and handles the input.
class CodeEditor
{
public:
    enum FindReplaceMode { eNONE, eFIND, eFIND_REPLACE };

    explicit CodeEditor(const char* id = "CodeEditor");  // id identifies the keyboard focus, see setFocus()

    // The text
    const std::string& getText() const { return _text; }
    bool isEmpty() const { return _text.empty(); }
    void setText(std::string text);     // replaces the whole text, this can be undone like an edit, the cursor goes to the start
    void clearHistory();                // forgets what can be undone and redone, e.g. after loading a file
    bool canUndo() const { return !_undoStack.empty(); }
    // Changes only when the text itself changes (edits, undo/redo, setText), unlike editId() which counts rendered frames
    uint32_t contentRevision() const { return _contentRevision; }
    uint32_t editId() const { return _editId; }

    // Editing like the user does it, each call is an undo step of its own. Offsets are byte offsets, an offset inside a UTF-8
    // sequence is moved to its start, one beyond the text to its end.
    uint32_t insert(std::string text);  // at the cursor, replaces the selection, returns the number of bytes inserted
    void replaceRange(uint32_t start, uint32_t end, std::string text);  // the cursor goes behind the new text
    void deleteSelectedText();
    void undo();
    void redo();

    // The cursor and the selection
    uint32_t line() const { return _cursorY + 1; }
    uint32_t column() const { return _cursorX + 1; }
    uint32_t cursorOffset() const { return offsetFromCursor(); }
    uint32_t topOfScreen() const { return _tosLine; }
    uint32_t totalLines() const { return _lines.size(); }
    std::pair<uint32_t, uint32_t> selection() const  // byte offsets, start <= end
    {
        return {_selectionStart > _selectionEnd ? _selectionEnd : _selectionStart, _selectionStart > _selectionEnd ? _selectionStart : _selectionEnd};
    }
    std::string selectedText() const;
    // These move the cursor, the next render() scrolls it into view
    void select(uint32_t start, uint32_t end);  // selects from start to end, the cursor goes to end
    void goTo(uint32_t line, uint32_t column);  // 1 based like line() and column(), limited to the text, removes the selection
    // The line, column, word, highlighting style and the selection at an offset, e.g. for a context menu
    CodeContext contextAt(uint32_t offset) const;

    // The look
    void setHighlighter(const SyntaxHighlighter* highlighter);  // not owned, it must outlive its use, nullptr shows plain text
    void updateHighlighting();                                  // scans the text again, after the rules of the highlighter changed
    void setTextStyle(const CodeTextStyle& style) { _textStyle = style; }
    const CodeTextStyle& textStyle() const { return _textStyle; }

    void setFocus();
    bool hasFocus() const;

    // The context menu: a right-click into the text (or the menu key, Shift+F10 at the cursor) opens it with the edit actions.
    // A right-click outside the selection first moves the cursor there. The handler can add, insert or remove items before the
    // menu opens, it gets the context of the click. The actions run later, while the menu is shown the text can't change, but
    // the CodeContext must not be kept. The editor must outlive its open menu.
    using ContextMenuHandler = std::function<void(PopupMenu::Menu& menu, const CodeContext& context)>;
    void setContextMenuHandler(ContextMenuHandler handler) { _contextMenuHandler = std::move(handler); }
    void openContextMenu();  // at the cursor, like the menu key

    // Links: the handler gets the context of the character under the mouse while Sys is held (or of a right-click) and returns
    // the link there, if any. Sys+click opens it, the context menu shows its label first. open() is called at the end of
    // render() (it may switch to another editor, but must not destroy this one) or from the menu.
    using LinkHandler = std::function<std::optional<CodeLink>(const CodeContext& context)>;
    void setLinkHandler(LinkHandler handler) { _linkHandler = std::move(handler); }

    // Shortcuts of the app: while the editor has the keyboard focus, the handler is asked before the keys of a frame are
    // handled; if it returns true, the app takes them (a Back key that would move the cursor otherwise e.g.) and the editor
    // ignores them in this frame
    using ShortcutHandler = std::function<bool()>;
    void setShortcutHandler(ShortcutHandler handler) { _shortcutHandler = std::move(handler); }

    // Shows the editor at the current layout position with the remaining width and the given height (<= 0 uses the remaining
    // height), and handles the input. Keyboard input needs the focus, a click into the text gives it.
    void render(float height = 0);

private:
    enum Operation { eINSERT, eDELETE };
    struct EditInfo
    {
        Operation operation;
        uint32_t id;
        uint32_t cursorPos;
        uint32_t startOffset;
        uint32_t endOffset;
        std::string text;
    };
    bool handleInput();
    void drawText();
    Rectangle drawToolArea();
    void updateLineInfo(uint32_t fromLine = 0);
    void ensureCursorVisibility();
    void updateFindResults();
    std::vector<std::pair<uint32_t, uint32_t>> findMatches() const;
    void replaceAll();
    void cursorLeft(int steps = 1);
    void cursorRight(int steps = 1);
    void cursorUp(int steps = 1);
    void cursorDown(int steps = 1);
    void cursorHome();
    void cursorEnd();
    void setCursor(uint32_t offset);
    void setCursorFromMouse(Vector2 mouse);
    uint32_t offsetAt(Vector2 mouse) const;
    std::optional<uint32_t> characterAt(Vector2 mouse) const;
    std::optional<CodeLink> linkAt(uint32_t offset) const;
    void updateHoveredLink();
    uint32_t offsetOf(int line, int column) const;
    void openContextMenu(Vector2 position, uint32_t offset);
    void openFindBar(FindReplaceMode mode);
    std::pair<uint32_t, uint32_t> wordRange(uint32_t offset) const;
    uint32_t toCodepointStart(uint32_t offset) const;
    void selectWord(uint32_t offset);
    void selectLine(uint32_t offset);
    int classAt(uint32_t offset) const;
    int classBefore(uint32_t offset) const;
    uint32_t wordLeft(uint32_t offset) const;
    uint32_t wordRight(uint32_t offset) const;
    void startUndoStep();
    void typeCharacter(int codepoint);
    uint32_t insertText(std::string text);
    void eraseSelection();
    void deleteTo(uint32_t offset);
    void deleteText(uint32_t offset, uint32_t length);
    void safeInsert(uint32_t offset, const std::string& text);
    void safeErase(uint32_t offset, uint32_t length);
    uint32_t offsetFromCursor() const;
    std::pair<int, int> cursorFromOffset(uint32_t offset) const;
    int lineLength(uint32_t line) const;
    char* lineStart(uint32_t line) { return _text.data() + (line < _lines.size() ? _lines[line] : 0); }
    char* lineEnd(uint32_t line) { return _text.data() + (line + 1 < _lines.size() ? _lines[line + 1] : _text.size()); }
    template <typename F>
    bool cursorWrapper(F&& f)
    {
        _typingRun = false;
        bool shiftPressed = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        if (shiftPressed && _selectionStart == _selectionEnd)
            _selectionStart = offsetFromCursor();
        f();
        if (shiftPressed)
            _selectionEnd = offsetFromCursor();
        return shiftPressed;
    }
    // the space between the top of the selection and the top of the text, the selection is lineHeight high
    float textPadding() const { return (_textStyle.lineHeight - _textStyle.fontSize) / 2; }
    static void fixLinefeed(std::string& text);

    Id _focusId;
    std::string _text;
    std::vector<uint32_t> _lines;
    std::vector<SyntaxHighlighter::LineState> _lineStates;
    const SyntaxHighlighter* _highlighter{nullptr};
    ContextMenuHandler _contextMenuHandler;
    LinkHandler _linkHandler;
    ShortcutHandler _shortcutHandler;
    std::optional<CodeLink> _hoveredLink;   // the link under the mouse while Sys is held
    std::function<void()> _linkToOpen;      // a Sys+click on a link, opened at the end of render()
    bool _linkCursor{false};                // the mouse cursor was set to the pointing hand
    CodeTextStyle _textStyle;
    uint32_t _selectionStart{0};
    uint32_t _selectionEnd{0};
    std::stack<EditInfo> _undoStack;
    std::stack<EditInfo> _redoStack;
    int _tosLine{0};
    int _losCol{0};
    int _cursorX{0};
    int _cursorVirtX{0};
    int _cursorY{0};
    uint32_t _visibleLines{0};
    uint32_t _visibleCols{0};
    float _lineNumberWidth{36};
    uint32_t _lineNumberCols{6};
    uint32_t _editId{0};
    uint32_t _undoStep{0};     // the id of the undo step the next edits belong to
    bool _typingRun{false};    // the last edit was typing, further typing may continue its undo step
    double _lastTypingTime{0};
    static constexpr double TYPING_PAUSE = 1.0;  // seconds without typing that end an undo step
    uint32_t _longestLineSize{0};
    uint32_t _contentRevision{0};
    uint32_t _lastSeenRevision{0};
    Rectangle _totalArea{};
    Rectangle _textArea{};
    Rectangle _toolArea{};
    FindReplaceMode _findOrReplace{eNONE};
    bool _findCaseSensitive{false};
    bool _findRegex{false};
    bool _findRegexValid{false};
    std::string _findString;
    std::string _replaceString;
    std::regex _findRegEx;
    std::string _findRegExString;
    bool _findRegExCaseSensitive{false};
    uint32_t _findUpdateRevision{0xFFFFFFFF};
    int _findResults{0};
    int _findCurrentResult{-1};
    uint32_t _findCurrentOffset{0};
    uint32_t _findCurrentLength{0};
    Vector2 _scrollPos{};
    double _blinkStart{0};
    bool _cursorChanged{false};
    bool _scrollToCursor{false};  // the cursor was moved through the API, the next render() scrolls it into view
    bool _matchSelected{false};  // the find bar selected a match in this frame, a cursor change must not clear it
    bool _mouseSelecting{false};
    int _clickCount{0};
    double _lastClickTime{-1};
    Vector2 _lastClickPos{};
};

}  // namespace gui
