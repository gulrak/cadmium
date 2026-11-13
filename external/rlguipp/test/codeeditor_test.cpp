//---------------------------------------------------------------------------------------
// Automated tests of the rlguipp code editor (CodeEditor, DrawCodeLine, SyntaxHighlighter)
//
// Like the text widget tests, the editor is drawn in a hidden window and the input is played
// with raylib's automation events, text is entered by pasting from a clipboard handler that
// never touches the system clipboard.
//---------------------------------------------------------------------------------------

#include <rlguipp/rlguipp_codeeditor.hpp>
#include <rlgl.h>

#include <cctype>
#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <map>
#include <string>

#if defined(PLATFORM_DESKTOP) || defined(PLATFORM_DESKTOP_GLFW)
extern "C" const char* glfwGetKeyName(int key, int scancode);
// to type characters through the char callback raylib registered, automation events can't type
struct GLFWwindow;
using GLFWcharfun = void (*)(GLFWwindow*, unsigned int);
extern "C" GLFWwindow* glfwGetCurrentContext();
extern "C" GLFWcharfun glfwSetCharCallback(GLFWwindow* window, GLFWcharfun callback);
#endif

namespace {

// raylib's AutomationEventType values, the enum is only declared in rcore.c
constexpr unsigned int AUTOMATION_KEY_UP = 1;
constexpr unsigned int AUTOMATION_KEY_DOWN = 2;
constexpr unsigned int AUTOMATION_MOUSE_BUTTON_UP = 5;
constexpr unsigned int AUTOMATION_MOUSE_BUTTON_DOWN = 6;
constexpr unsigned int AUTOMATION_MOUSE_POSITION = 7;

#ifdef __APPLE__
constexpr int SYS_KEY = KEY_LEFT_SUPER;
constexpr int WORD_KEY = KEY_LEFT_ALT;
#else
constexpr int SYS_KEY = KEY_LEFT_CONTROL;
constexpr int WORD_KEY = KEY_LEFT_CONTROL;
#endif

int g_failures = 0;
int g_checks = 0;

#define CHECK_EQ(actual, expected)                                                                                         \
    do {                                                                                                                   \
        ++g_checks;                                                                                                        \
        const auto& actualValue = (actual);                                                                                \
        const auto& expectedValue = (expected);                                                                            \
        if (!(actualValue == expectedValue)) {                                                                             \
            ++g_failures;                                                                                                  \
            std::printf("FAILED line %d: %s\n", __LINE__, #actual);                                                        \
        }                                                                                                                  \
    } while (false)

#define CHECK_TEXT(actual, expected)                                                                                       \
    do {                                                                                                                   \
        ++g_checks;                                                                                                        \
        const std::string actualText = (actual);                                                                           \
        const std::string expectedText = (expected);                                                                       \
        if (actualText != expectedText) {                                                                                  \
            ++g_failures;                                                                                                  \
            std::printf("FAILED line %d: %s is \"%s\", expected \"%s\"\n", __LINE__, #actual, actualText.c_str(), expectedText.c_str()); \
        }                                                                                                                  \
    } while (false)

// The raylib key that shows the letter in the current keyboard layout, the same lookup rlguipp does
int keyFor(char letter)
{
#if defined(PLATFORM_DESKTOP) || defined(PLATFORM_DESKTOP_GLFW)
    for (int key = KEY_APOSTROPHE; key <= KEY_GRAVE; ++key) {
        if ((key > KEY_APOSTROPHE && key < KEY_COMMA) || key == 58 || key == 60 || (key > KEY_EQUAL && key < KEY_A) || key == 94 || key == 95)
            continue;
        const char* name = glfwGetKeyName(key, 0);
        if (name && name[0] && !name[1] && (name[0] == letter || name[0] == letter - 'A' + 'a'))
            return key;
    }
#endif
    return KEY_A + (letter - 'A');
}

void play(unsigned int type, int param0 = 0, int param1 = 0)
{
    AutomationEvent event{};
    event.type = type;
    event.params[0] = param0;
    event.params[1] = param1;
    PlayAutomationEvent(event);
}

// Words in the list are keywords (style 1), '#' starts a comment (style 2) and "/*" ... "*/" is a block
// comment (style 3) that goes over lines, the line state is 1 inside of it. It remembers the state each
// line was highlighted with, to check what the editor passes.
class TestHighlighter : public gui::SyntaxHighlighter
{
public:
    mutable std::map<std::string, LineState> highlightedWith;

    LineState nextLineState(std::string_view line, LineState state) const override
    {
        for (size_t i = 0; i + 1 < line.size(); ++i) {
            if (!state && line.substr(i, 2) == "/*")
                state = 1, ++i;
            else if (state && line.substr(i, 2) == "*/")
                state = 0, ++i;
        }
        return state;
    }

    void highlightLine(std::string_view line, LineState state, std::span<uint8_t> styles) const override
    {
        highlightedWith[std::string(line)] = state;
        size_t i = 0;
        while (i < line.size()) {
            if (state) {
                const auto end = line.find("*/", i);
                const auto stop = end == std::string_view::npos ? line.size() : end + 2;
                std::fill(styles.begin() + i, styles.begin() + stop, 3);
                i = stop;
                state = 0;
            }
            else if (line.substr(i, 2) == "/*") {
                state = 1;
            }
            else if (line[i] == '#') {
                std::fill(styles.begin() + i, styles.end(), 2);
                break;
            }
            else if (line[i] != ' ') {
                const auto end = std::min(line.find(' ', i), line.size());
                const auto word = line.substr(i, end - i);
                if (word == "if" || word == "then")
                    std::fill(styles.begin() + i, styles.begin() + end, 1);
                i = end;
            }
            else {
                ++i;
            }
        }
    }
};

class Harness
{
public:
    gui::CodeEditor editor{"code"};
    TestHighlighter highlighter;
    std::string clipboard;
    bool locked{false};
    bool captureScreen{false};  // keep the drawn frame in `screen`
    Image screen{};
    Vector2 origin{};
    float width{0};

    Harness()
    {
        gui::SetClipboardHandler([this]() { return clipboard; }, [this](const std::string& text) { clipboard = text; });
        editor.setHighlighter(&highlighter);
    }

    ~Harness() { UnloadImage(screen); }

    void frame()
    {
        BeginDrawing();
        ClearBackground(GetColor(GuiGetStyle(DEFAULT, BACKGROUND_COLOR)));
        gui::BeginGui();
        gui::SetRowHeight(20);
        gui::SetSpacing(0);
        origin = gui::GetCurrentPos();
        width = gui::GetContentAvailable().width;
        if (locked)
            GuiLock();
        editor.render(200);
        if (locked)
            GuiUnlock();
        gui::EndGui();
        if (captureScreen) {
            rlDrawRenderBatchActive();  // raylib draws the batch only in EndDrawing()
            UnloadImage(screen);
            screen = LoadImageFromScreen();
        }
        EndDrawing();
    }

    void press(int key, std::initializer_list<int> modifiers = {})
    {
        for (auto modifier : modifiers)
            play(AUTOMATION_KEY_DOWN, modifier);
        play(AUTOMATION_KEY_DOWN, key);
        frame();
        play(AUTOMATION_KEY_UP, key);
        for (auto modifier : modifiers)
            play(AUTOMATION_KEY_UP, modifier);
        frame();
    }

    void shortcut(char letter, bool shift = false)
    {
        if (shift)
            press(keyFor(letter), {SYS_KEY, KEY_LEFT_SHIFT});
        else
            press(keyFor(letter), {SYS_KEY});
    }

    void paste(const std::string& text)
    {
        clipboard = text;
        shortcut('V');
    }

    // The screen position just right of the boundary before a column (0 based) in a line (0 based), a click there puts the
    // cursor before the column, the line numbers take three columns for less than ten lines
    Vector2 textPos(int line, int column) const
    {
        const auto& style = editor.textStyle();
        const float lineNumberWidth = 3 * style.cellWidth;
        const float padding = (style.lineHeight - style.fontSize) / 2;
        return {origin.x + lineNumberWidth + column * style.cellWidth + 1, origin.y + 2 * padding + line * style.lineHeight + style.lineHeight / 2};
    }

    void click(Vector2 pos, int count = 1, std::initializer_list<int> modifiers = {})
    {
        // a click near the last one would continue its click count
        if (std::abs(pos.x - _lastClick.x) < 8 && std::abs(pos.y - _lastClick.y) < 8)
            WaitTime(0.5);
        _lastClick = pos;
        for (int i = 0; i < count; ++i) {
            for (auto modifier : modifiers)
                play(AUTOMATION_KEY_DOWN, modifier);
            play(AUTOMATION_MOUSE_POSITION, (int)pos.x, (int)pos.y);
            play(AUTOMATION_MOUSE_BUTTON_DOWN, MOUSE_BUTTON_LEFT);
            frame();
            play(AUTOMATION_MOUSE_BUTTON_UP, MOUSE_BUTTON_LEFT);
            for (auto modifier : modifiers)
                play(AUTOMATION_KEY_UP, modifier);
            frame();
        }
    }

    // Types the characters one per frame, returns false where that isn't possible
    bool type(const std::string& text)
    {
#if defined(PLATFORM_DESKTOP) || defined(PLATFORM_DESKTOP_GLFW)
        auto* window = glfwGetCurrentContext();
        const auto callback = glfwSetCharCallback(window, nullptr);
        glfwSetCharCallback(window, callback);
        if (!callback)
            return false;
        for (const char c : text) {
            callback(window, static_cast<unsigned char>(c));
            frame();
        }
        return true;
#else
        (void)text;
        return false;
#endif
    }

    void rightClick(Vector2 pos)
    {
        play(AUTOMATION_MOUSE_POSITION, (int)pos.x, (int)pos.y);
        play(AUTOMATION_MOUSE_BUTTON_DOWN, MOUSE_BUTTON_RIGHT);
        frame();
        play(AUTOMATION_MOUSE_BUTTON_UP, MOUSE_BUTTON_RIGHT);
        frame();
    }

    void drag(Vector2 from, Vector2 to)
    {
        play(AUTOMATION_MOUSE_POSITION, (int)from.x, (int)from.y);
        play(AUTOMATION_MOUSE_BUTTON_DOWN, MOUSE_BUTTON_LEFT);
        frame();
        play(AUTOMATION_MOUSE_POSITION, (int)to.x, (int)to.y);
        frame();
        play(AUTOMATION_MOUSE_BUTTON_UP, MOUSE_BUTTON_LEFT);
        frame();
    }

    // The buttons of the replace row, the second row of the find bar: R, the replace text, Replace, Rep.all
    Vector2 replaceButton() const { return {origin.x + width - 72, origin.y + 27}; }
    Vector2 replaceAllButton() const { return {origin.x + width - 24, origin.y + 27}; }

    std::string selectedText() const
    {
        const auto [start, end] = editor.selection();
        return editor.getText().substr(start, end - start);
    }

private:
    Vector2 _lastClick{-100, -100};
};

void testEditing(Harness& h)
{
    h.editor.setText("first line\nsecond line\nthird");
    h.frame();
    CHECK_EQ(h.editor.hasFocus(), false);
    h.click(h.textPos(1, 7));
    CHECK_EQ(h.editor.hasFocus(), true);
    CHECK_EQ(h.editor.line(), 2u);
    CHECK_EQ(h.editor.column(), 8u);

    // the cursor keys, insertion at the cursor
    h.press(KEY_UP);
    h.press(KEY_HOME);
    h.paste("> ");
    CHECK_TEXT(h.editor.getText(), "> first line\nsecond line\nthird");
    h.press(KEY_DOWN);
    h.press(KEY_DOWN);
    h.press(KEY_END);
    h.paste("!");
    CHECK_TEXT(h.editor.getText(), "> first line\nsecond line\nthird!");

    // enter and backspace, tab inserts spaces up to the next multiple of four columns
    h.press(KEY_ENTER);
    h.press(KEY_TAB);
    h.paste("x");
    CHECK_TEXT(h.editor.getText(), "> first line\nsecond line\nthird!\n    x");
    h.press(KEY_LEFT);
    h.press(KEY_TAB);
    CHECK_TEXT(h.editor.getText(), "> first line\nsecond line\nthird!\n        x");
    h.press(KEY_END);
    h.press(KEY_BACKSPACE);
    h.press(KEY_BACKSPACE);
    CHECK_TEXT(h.editor.getText(), "> first line\nsecond line\nthird!\n       ");

    // select with shift, copy, cut, undo, redo
    h.editor.setText("alpha beta gamma");
    h.frame();
    h.press(KEY_END);
    for (int i = 0; i < 5; ++i)
        h.press(KEY_LEFT, {KEY_LEFT_SHIFT});
    CHECK_TEXT(h.selectedText(), "gamma");
    h.shortcut('C');
    CHECK_TEXT(h.clipboard, "gamma");
    h.shortcut('X');
    CHECK_TEXT(h.editor.getText(), "alpha beta ");
    h.shortcut('Z');
    CHECK_TEXT(h.editor.getText(), "alpha beta gamma");
    h.shortcut('Z', true);
    CHECK_TEXT(h.editor.getText(), "alpha beta ");
    h.shortcut('Z');

    // select all, a paste replaces the selection, multi-byte characters are one column
    h.shortcut('A');
    CHECK_TEXT(h.selectedText(), "alpha beta gamma");
    h.paste("\xc3\xa4x");
    CHECK_TEXT(h.editor.getText(), "\xc3\xa4x");
    CHECK_EQ(h.editor.column(), 3u);
    h.press(KEY_LEFT);
    h.press(KEY_LEFT);
    h.paste("<");
    CHECK_TEXT(h.editor.getText(), "<\xc3\xa4x");

    // a mouse drag selects, also backwards
    h.editor.setText("one two three");
    h.frame();
    h.drag(h.textPos(0, 8), h.textPos(0, 4));
    CHECK_TEXT(h.selectedText(), "two ");

    // setText() can be undone, clearHistory() forgets it
    const auto revision = h.editor.contentRevision();
    h.editor.setText("replaced");
    CHECK_EQ(h.editor.contentRevision() != revision, true);
    h.frame();
    h.shortcut('Z');
    CHECK_TEXT(h.editor.getText(), "one two three");
    h.editor.setText("replaced");
    h.editor.clearHistory();
    CHECK_EQ(h.editor.canUndo(), false);
    h.shortcut('Z');
    CHECK_TEXT(h.editor.getText(), "replaced");
}

void testFind(Harness& h)
{
    h.editor.setText("find the needle\nin the haystack, the needle");
    h.frame();
    h.click(h.textPos(0, 0));
    h.shortcut('F');
    CHECK_EQ(gui::HasKeyboardFocus(gui::GetId("find")), false);  // the find box is in the id scope of the editor
    // the paste goes to the find box, it has the keyboard focus now
    h.paste("needle");
    CHECK_TEXT(h.editor.getText(), "find the needle\nin the haystack, the needle");
    CHECK_EQ(h.editor.selection().first, 9u);
    CHECK_TEXT(h.selectedText(), "needle");
    h.press(KEY_ESCAPE);
    h.press(KEY_ESCAPE);
    h.frame();
    CHECK_EQ(h.editor.selection().first, h.editor.selection().second);
    // the text area moves back up when the bar is closed
    h.click(h.textPos(1, 0));
    CHECK_EQ(h.editor.line(), 2u);
}

void testReplace(Harness& h)
{
    h.editor.setText("a needle, a needle\nand a needle");
    h.editor.clearHistory();
    h.frame();
    h.click(h.textPos(0, 0));
    h.shortcut('R');
    h.shortcut('A');  // the find text is kept from before
    h.paste("needle");
    CHECK_EQ(h.editor.selection().first, 2u);
    h.press(KEY_TAB);  // from the find text to the replace text
    h.shortcut('A');
    h.paste("pin");
    CHECK_TEXT(h.editor.getText(), "a needle, a needle\nand a needle");

    // all matches at once, as one undo step
    h.click(h.replaceAllButton());
    CHECK_TEXT(h.editor.getText(), "a pin, a pin\nand a pin");
    h.editor.undo();
    CHECK_TEXT(h.editor.getText(), "a needle, a needle\nand a needle");
    h.frame();

    // Replace replaces the selected match and selects the next one, also when clicked again
    h.click(h.textPos(0, 0));
    h.shortcut('R');
    h.shortcut('R');
    CHECK_TEXT(h.selectedText(), "needle");
    h.click(h.replaceButton());
    CHECK_TEXT(h.editor.getText(), "a pin, a needle\nand a needle");
    CHECK_EQ(h.editor.selection().first, 9u);
    h.click(h.replaceButton());
    CHECK_TEXT(h.editor.getText(), "a pin, a pin\nand a needle");
    CHECK_TEXT(h.selectedText(), "needle");
    h.click(h.replaceAllButton());
    CHECK_TEXT(h.editor.getText(), "a pin, a pin\nand a pin");
    h.press(KEY_ESCAPE);
    h.frame();
}

void testHighlighting(Harness& h)
{
    h.highlighter.highlightedWith.clear();
    h.editor.setText("if a then b\n/* block\ncomment */ x\nafter # note");
    h.frame();
    CHECK_EQ(h.highlighter.highlightedWith.at("if a then b"), 0u);
    CHECK_EQ(h.highlighter.highlightedWith.at("/* block"), 0u);
    CHECK_EQ(h.highlighter.highlightedWith.at("comment */ x"), 1u);
    CHECK_EQ(h.highlighter.highlightedWith.at("after # note"), 0u);

    // an edit updates the states of the following lines, also its undo
    h.click(h.textPos(1, 2));
    h.press(KEY_BACKSPACE);
    h.press(KEY_BACKSPACE);
    CHECK_TEXT(h.editor.getText(), "if a then b\n block\ncomment */ x\nafter # note");
    h.highlighter.highlightedWith.clear();
    h.frame();
    CHECK_EQ(h.highlighter.highlightedWith.at("comment */ x"), 0u);
    h.shortcut('Z');
    h.shortcut('Z');
    CHECK_TEXT(h.editor.getText(), "if a then b\n/* block\ncomment */ x\nafter # note");
    h.highlighter.highlightedWith.clear();
    h.frame();
    CHECK_EQ(h.highlighter.highlightedWith.at("comment */ x"), 1u);

    // a new highlighter starts from state 0
    h.editor.setText("plain\nnew");
    h.highlighter.highlightedWith.clear();
    h.frame();
    CHECK_EQ(h.highlighter.highlightedWith.at("new"), 0u);
}

// The editing works like in the text widgets
void testLikeTextBox(Harness& h)
{
    // typed characters are undone as one run, a cursor move starts a new one
    h.editor.setText("");
    h.editor.clearHistory();
    h.frame();
    h.click(h.textPos(0, 0));
    if (h.type("abc")) {
        CHECK_TEXT(h.editor.getText(), "abc");
        h.press(KEY_LEFT);
        h.type("x");
        h.press(KEY_END);
        h.type("de");
        CHECK_TEXT(h.editor.getText(), "abxcde");
        h.shortcut('Z');
        CHECK_TEXT(h.editor.getText(), "abxc");
        h.shortcut('Z');
        CHECK_TEXT(h.editor.getText(), "abc");
        h.shortcut('Z');
        CHECK_TEXT(h.editor.getText(), "");
        h.shortcut('Z', true);
        CHECK_TEXT(h.editor.getText(), "abc");
        // a selection is replaced by typing, as an undo step of its own
        h.shortcut('A');
        h.type("q");
        CHECK_TEXT(h.editor.getText(), "q");
        h.shortcut('Z');
        CHECK_TEXT(h.editor.getText(), "abc");

        // typing is undone word by word, a word after a space starts a new step
        h.editor.setText("");
        h.frame();
        h.type("one two  three");
        h.shortcut('Z');
        CHECK_TEXT(h.editor.getText(), "one two  ");
        h.shortcut('Z');
        CHECK_TEXT(h.editor.getText(), "one ");
        h.shortcut('Z');
        CHECK_TEXT(h.editor.getText(), "");

        // a pause in the typing starts a new step
        h.type("ab");
        WaitTime(1.2);
        h.type("cd");
        h.shortcut('Z');
        CHECK_TEXT(h.editor.getText(), "ab");
    }
    else {
        std::printf("typing not possible on this platform, skipped the typing tests\n");
    }

    // Delete removes the character after the cursor, every Backspace and Delete is an undo step of its own
    h.editor.setText("12345");
    h.frame();
    h.press(KEY_HOME);
    h.press(KEY_DELETE);
    CHECK_TEXT(h.editor.getText(), "2345");
    h.press(KEY_END);
    h.press(KEY_BACKSPACE);
    h.press(KEY_BACKSPACE);
    CHECK_TEXT(h.editor.getText(), "23");
    h.shortcut('Z');
    CHECK_TEXT(h.editor.getText(), "234");
    h.shortcut('Z');
    CHECK_TEXT(h.editor.getText(), "2345");
    h.shortcut('Z');
    CHECK_TEXT(h.editor.getText(), "12345");

    // word movement and deletion, punctuation and a line break are words of their own
    h.editor.setText("alpha beta(gamma)\n  delta");
    h.frame();
    h.press(KEY_RIGHT, {WORD_KEY});
    CHECK_EQ(h.editor.column(), 7u);
    h.press(KEY_RIGHT, {WORD_KEY});
    CHECK_EQ(h.editor.column(), 11u);
    h.press(KEY_RIGHT, {WORD_KEY});
    CHECK_EQ(h.editor.column(), 12u);
    h.press(KEY_DELETE, {WORD_KEY});
    CHECK_TEXT(h.editor.getText(), "alpha beta()\n  delta");
    h.press(KEY_END);
    h.press(KEY_RIGHT, {WORD_KEY});
    CHECK_EQ(h.editor.line(), 2u);
    CHECK_EQ(h.editor.column(), 1u);
    h.press(KEY_RIGHT, {WORD_KEY});
    CHECK_EQ(h.editor.column(), 3u);
    h.press(KEY_BACKSPACE, {WORD_KEY});
    CHECK_TEXT(h.editor.getText(), "alpha beta()delta");
    h.press(KEY_LEFT, {WORD_KEY});
    h.press(KEY_LEFT, {WORD_KEY});
    CHECK_EQ(h.editor.column(), 7u);

    // without shift, a selection collapses to its start or end
    h.press(KEY_HOME);
    for (int i = 0; i < 3; ++i)
        h.press(KEY_RIGHT, {KEY_LEFT_SHIFT});
    CHECK_TEXT(h.selectedText(), "alp");
    h.press(KEY_RIGHT);
    CHECK_EQ(h.editor.column(), 4u);
    CHECK_TEXT(h.selectedText(), "");
    h.press(KEY_LEFT, {KEY_LEFT_SHIFT});
    h.press(KEY_LEFT, {KEY_LEFT_SHIFT});
    h.press(KEY_LEFT);
    CHECK_EQ(h.editor.column(), 2u);

    // line and text start and end
    h.editor.setText("first\nsecond line\nlast");
    h.frame();
    h.click(h.textPos(1, 3));
#ifdef __APPLE__
    h.press(KEY_RIGHT, {SYS_KEY});
    CHECK_EQ(h.editor.column(), 12u);
    h.press(KEY_LEFT, {SYS_KEY});
    CHECK_EQ(h.editor.column(), 1u);
    h.press(KEY_DOWN, {SYS_KEY});
    CHECK_EQ(h.editor.line(), 3u);
    CHECK_EQ(h.editor.column(), 5u);
    h.press(KEY_UP, {SYS_KEY});
    CHECK_EQ(h.editor.line(), 1u);
    h.click(h.textPos(1, 6));
    h.press(KEY_BACKSPACE, {SYS_KEY});
    CHECK_TEXT(h.editor.getText(), "first\n line\nlast");
#else
    h.press(KEY_END);
    CHECK_EQ(h.editor.column(), 12u);
    h.press(KEY_HOME);
    CHECK_EQ(h.editor.column(), 1u);
    h.press(KEY_END, {KEY_LEFT_CONTROL});
    CHECK_EQ(h.editor.line(), 3u);
    CHECK_EQ(h.editor.column(), 5u);
    h.press(KEY_HOME, {KEY_LEFT_CONTROL});
    CHECK_EQ(h.editor.line(), 1u);
    h.shortcut('Z');
    h.shortcut('Y');
#endif

    // a double click selects a word, a triple click the line, shift+click extends the selection
    h.editor.setText("one two three\nfour");
    h.frame();
    h.click(h.textPos(0, 5), 2);
    CHECK_TEXT(h.selectedText(), "two");
    h.click(h.textPos(0, 5), 3);
    CHECK_TEXT(h.selectedText(), "one two three");
    h.click(h.textPos(1, 1));
    h.click(h.textPos(0, 4), 1, {KEY_LEFT_SHIFT});
    CHECK_TEXT(h.selectedText(), "two three\nf");
    // a click below the last line goes to the end of the text
    h.click({h.textPos(1, 1).x, h.textPos(1, 1).y + 100});
    CHECK_EQ(h.editor.line(), 2u);
    CHECK_EQ(h.editor.column(), 5u);
}

// The API for applications, e.g. for context menu actions
void testApi(Harness& h)
{
    const std::string text = "first line\nsecond w\xc3\xb6rd here\nif x then";
    h.editor.setText(text);
    h.editor.clearHistory();
    h.frame();

    // select, read and replace a range, as one undo step, the cursor goes behind the new text
    h.editor.select(6, 10);
    h.frame();
    CHECK_TEXT(h.selectedText(), "line");
    CHECK_TEXT(h.editor.selectedText(), "line");
    h.editor.replaceRange(6, 10, "LINE");
    CHECK_TEXT(h.editor.getText(), "first LINE\nsecond w\xc3\xb6rd here\nif x then");
    CHECK_EQ(h.editor.cursorOffset(), 10u);
    h.editor.undo();
    CHECK_TEXT(h.editor.getText(), text);
    // an empty range inserts, an offset inside a UTF-8 sequence moves to its start
    h.editor.replaceRange(0, 0, "> ");
    CHECK_TEXT(h.editor.getText().substr(0, 4), "> fi");
    h.editor.undo();
    h.editor.replaceRange(20, 21, "o");
    CHECK_TEXT(h.editor.getText(), "first line\nsecond word here\nif x then");
    h.editor.undo();
    CHECK_TEXT(h.editor.getText(), text);

    // goTo is limited to the text and removes the selection
    h.editor.select(0, 5);
    h.editor.goTo(2, 8);
    h.frame();
    CHECK_EQ(h.editor.line(), 2u);
    CHECK_EQ(h.editor.column(), 8u);
    CHECK_TEXT(h.editor.selectedText(), "");
    h.editor.goTo(99, 99);
    CHECK_EQ(h.editor.line(), 3u);
    CHECK_EQ(h.editor.column(), 10u);

    // the context of an offset: line, column in code points, the word by the double click rule, the style of the highlighter
    h.editor.select(0, 5);
    auto context = h.editor.contextAt(21);  // the 'r' of "w\xc3\xb6rd"
    CHECK_EQ(context.line, 2u);
    CHECK_EQ(context.column, 10u);
    CHECK_EQ(context.word.first, 18u);
    CHECK_EQ(context.word.second, 23u);
    CHECK_TEXT(std::string(context.lineText), "second w\xc3\xb6rd here");
    CHECK_EQ(context.selection.second, 5u);
    CHECK_EQ(context.style, 0);
    context = h.editor.contextAt(36);  // the "then" of the last line
    CHECK_EQ(context.style, 1);
    CHECK_TEXT(std::string(context.lineText), "if x then");
    context = h.editor.contextAt(static_cast<uint32_t>(text.size()));
    CHECK_EQ(context.line, 3u);
    CHECK_EQ(context.column, 10u);

    // a line far down is scrolled into view by the next render
    std::string longText;
    for (int i = 1; i <= 100; ++i)
        longText += "line " + std::to_string(i) + "\n";
    h.editor.setText(longText);
    h.frame();
    h.editor.goTo(80, 1);
    h.frame();
    CHECK_EQ(h.editor.topOfScreen() > 60 && h.editor.topOfScreen() < 80, true);
    h.editor.select(10, 12);
    h.frame();
    CHECK_EQ(h.editor.topOfScreen(), 1u);
    CHECK_TEXT(h.editor.selectedText(), "e ");
}

const gui::PopupMenu::Item* findItem(const gui::PopupMenu::Menu& menu, const std::string& label)
{
    for (const auto& item : menu) {
        if (item.label == label)
            return &item;
    }
    std::printf("no menu item '%s'\n", label.c_str());
    return nullptr;
}

void runItem(const gui::PopupMenu::Menu& menu, const std::string& label)
{
    if (const auto* item = findItem(menu, label); item && item->enabled && item->action)
        item->action();
}

bool isEnabled(const gui::PopupMenu::Menu& menu, const std::string& label)
{
    const auto* item = findItem(menu, label);
    return item && item->enabled;
}

void testContextMenu(Harness& h)
{
    gui::PopupMenu::Menu lastMenu;
    gui::CodeContext lastContext;
    std::string lastLine;
    int opened = 0;
    // the application adds an item and sees the context of the click
    h.editor.setContextMenuHandler([&](gui::PopupMenu::Menu& menu, const gui::CodeContext& context) {
        menu.push_back(gui::PopupMenu::Separator());
        menu.push_back(gui::PopupMenu::Action("To Uppercase", [&h]() {
            const auto [start, end] = h.editor.selection();
            auto text = h.editor.selectedText();
            for (auto& c : text)
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            h.editor.replaceRange(start, end, text);
        }));
        lastMenu = menu;
        lastContext = context;
        lastLine = std::string(context.lineText);
        ++opened;
    });
    h.editor.setText("alpha beta\ngamma");
    h.editor.clearHistory();
    h.frame();

    // a right-click outside the selection moves the cursor there first
    h.editor.select(0, 5);
    h.frame();
    h.rightClick(h.textPos(1, 2));
    CHECK_EQ(opened, 1);
    CHECK_EQ(gui::PopupMenu::IsOpen(), true);
    CHECK_EQ(h.editor.line(), 2u);
    CHECK_EQ(h.editor.column(), 3u);
    CHECK_TEXT(h.editor.selectedText(), "");
    CHECK_EQ(lastContext.line, 2u);
    CHECK_EQ(lastContext.column, 3u);
    CHECK_TEXT(lastLine, "gamma");
    CHECK_EQ(lastContext.word.first, 11u);
    CHECK_EQ(lastContext.word.second, 16u);
    CHECK_EQ(isEnabled(lastMenu, "Undo"), false);
    CHECK_EQ(isEnabled(lastMenu, "Cut"), false);
    CHECK_EQ(isEnabled(lastMenu, "Copy"), false);
    CHECK_EQ(isEnabled(lastMenu, "Select All"), true);
    CHECK_EQ(isEnabled(lastMenu, "Find..."), true);
    // while the menu is open the editor takes no input
    h.paste("x");
    CHECK_TEXT(h.editor.getText(), "alpha beta\ngamma");
    gui::PopupMenu::Close();
    h.frame();

    // a right-click into the selection keeps it, the actions work on it
    h.editor.select(6, 10);
    h.frame();
    h.rightClick(h.textPos(0, 8));
    CHECK_TEXT(h.editor.selectedText(), "beta");
    CHECK_EQ(lastContext.selection.first, 6u);
    CHECK_EQ(isEnabled(lastMenu, "Cut"), true);
    runItem(lastMenu, "Copy");
    CHECK_TEXT(h.clipboard, "beta");
    runItem(lastMenu, "To Uppercase");
    CHECK_TEXT(h.editor.getText(), "alpha BETA\ngamma");
    gui::PopupMenu::Close();
    h.frame();
    h.rightClick(h.textPos(1, 0));
    CHECK_EQ(isEnabled(lastMenu, "Undo"), true);
    runItem(lastMenu, "Undo");
    CHECK_TEXT(h.editor.getText(), "alpha beta\ngamma");
    runItem(lastMenu, "Select All");
    CHECK_TEXT(h.editor.selectedText(), "alpha beta\ngamma");
    runItem(lastMenu, "Replace...");
    gui::PopupMenu::Close();
    h.frame();
    CHECK_EQ(gui::HasKeyboardFocus(gui::GetId("code")), false);  // the opened find bar has the focus
    h.press(KEY_ESCAPE);
    h.frame();

    // Shift+F10 opens it at the cursor
    h.click(h.textPos(0, 3));
    h.press(KEY_F10, {KEY_LEFT_SHIFT});
    CHECK_EQ(gui::PopupMenu::IsOpen(), true);
    CHECK_EQ(lastContext.offset, 3u);
    gui::PopupMenu::Close();
    h.frame();
    h.editor.setContextMenuHandler({});
}

// True if one of the pixels of the screen row y between x1 and x2 (screen coordinates) has the color
bool rowHasColor(const Image& screen, float x1, float x2, float y, Color color)
{
    const float scale = static_cast<float>(screen.width) / static_cast<float>(GetScreenWidth());
    for (float x = x1; x < x2; x += 1.0f) {
        const auto pixel = GetImageColor(screen, static_cast<int>(x * scale), static_cast<int>(y * scale));
        if (pixel.r == color.r && pixel.g == color.g && pixel.b == color.b)
            return true;
    }
    return false;
}

void testLinks(Harness& h)
{
    // words starting with '@' are links
    std::string opened;
    h.editor.setLinkHandler([&opened](const gui::CodeContext& context) -> std::optional<gui::CodeLink> {
        const auto word = context.word;
        if (word.first == word.second || context.lineText.empty())
            return std::nullopt;
        const auto lineStart = context.offset - (context.column - 1);  // ASCII only in this test
        const auto start = word.first > lineStart && context.lineText[word.first - lineStart - 1] == '@' ? word.first - 1 : word.first;
        if (context.lineText[start - lineStart] != '@')
            return std::nullopt;
        const auto name = std::string(context.lineText.substr(start - lineStart, word.second - start));
        return gui::CodeLink{start, word.second, "Open " + name, [&opened, name]() { opened = name; }};
    });
    h.editor.setText("call @target now\nplain line");
    h.editor.clearHistory();
    h.frame();
    h.click(h.textPos(1, 0));

    // with Sys held over the link it is underlined
    const auto linkPos = h.textPos(0, 7);
    const auto& style = h.editor.textStyle();
    const float underlineY = h.origin.y - (style.lineHeight - style.fontSize) + style.lineHeight + style.fontSize;
    const Color linkColor = GetColor(GuiGetStyle(DEFAULT, TEXT_COLOR_FOCUSED));
    h.captureScreen = true;
    play(AUTOMATION_MOUSE_POSITION, (int)linkPos.x, (int)linkPos.y);
    h.frame();
    CHECK_EQ(rowHasColor(h.screen, h.textPos(0, 5).x, h.textPos(0, 12).x, underlineY, linkColor), false);
    play(AUTOMATION_KEY_DOWN, SYS_KEY);
    h.frame();
    CHECK_EQ(rowHasColor(h.screen, h.textPos(0, 5).x, h.textPos(0, 12).x, underlineY, linkColor), true);
    CHECK_EQ(rowHasColor(h.screen, h.textPos(0, 0).x, h.textPos(0, 4).x, underlineY, linkColor), false);

    // Sys+click opens it, the cursor stays
    play(AUTOMATION_MOUSE_BUTTON_DOWN, MOUSE_BUTTON_LEFT);
    h.frame();
    play(AUTOMATION_MOUSE_BUTTON_UP, MOUSE_BUTTON_LEFT);
    h.frame();
    CHECK_TEXT(opened, "@target");
    CHECK_EQ(h.editor.line(), 2u);
    // Sys+click beside a link is a normal click
    const auto plainPos = h.textPos(1, 3);
    play(AUTOMATION_MOUSE_POSITION, (int)plainPos.x, (int)plainPos.y);
    play(AUTOMATION_MOUSE_BUTTON_DOWN, MOUSE_BUTTON_LEFT);
    h.frame();
    play(AUTOMATION_MOUSE_BUTTON_UP, MOUSE_BUTTON_LEFT);
    play(AUTOMATION_KEY_UP, SYS_KEY);
    h.frame();
    CHECK_EQ(h.editor.column(), 4u);
    h.captureScreen = false;

    // the context menu shows the link first
    opened.clear();
    gui::PopupMenu::Menu lastMenu;
    h.editor.setContextMenuHandler([&lastMenu](gui::PopupMenu::Menu& menu, const gui::CodeContext&) { lastMenu = menu; });
    h.rightClick(h.textPos(0, 10));
    CHECK_EQ(lastMenu.empty(), false);
    CHECK_TEXT(lastMenu.empty() ? "" : lastMenu.front().label, "Open @target");
    runItem(lastMenu, "Open @target");
    CHECK_TEXT(opened, "@target");
    gui::PopupMenu::Close();
    h.frame();
    h.rightClick(h.textPos(1, 3));
    CHECK_TEXT(lastMenu.empty() ? "" : lastMenu.front().label, "Undo");
    gui::PopupMenu::Close();
    h.frame();
    h.editor.setContextMenuHandler({});
    h.editor.setLinkHandler({});
}

void testLocked(Harness& h)
{
    h.editor.setText("locked");
    h.frame();
    h.click(h.textPos(0, 6));
    CHECK_EQ(h.editor.hasFocus(), true);
    h.locked = true;
    h.paste("x");
    h.press(KEY_LEFT);
    h.click(h.textPos(0, 0));
    CHECK_TEXT(h.editor.getText(), "locked");
    CHECK_EQ(h.editor.column(), 7u);
    CHECK_EQ(h.editor.hasFocus(), true);  // the focus stays while the gui is locked
    h.locked = false;
    h.paste("!");
    CHECK_TEXT(h.editor.getText(), "locked!");
}

void testShortcutText()
{
    // the context menu and the app menus show the shortcuts like the platform does
    if (gui::IsMacLike()) {
        CHECK_TEXT(gui::ShortcutText("Z"), "⌘Z");
        CHECK_TEXT(gui::ShortcutText("Z", gui::SHORTCUT_SHIFT | gui::SHORTCUT_SYS), "⇧⌘Z");
        CHECK_TEXT(gui::ShortcutText("←", gui::SHORTCUT_SYS | gui::SHORTCUT_ALT), "⌥⌘←");
        CHECK_TEXT(gui::ShortcutText("Tab", gui::SHORTCUT_CTRL), "⌃Tab");
        CHECK_TEXT(gui::ShortcutText("F5", gui::SHORTCUT_SHIFT), "⇧F5");
    }
    else {
        CHECK_TEXT(gui::ShortcutText("Z"), "Ctrl+Z");
        CHECK_TEXT(gui::ShortcutText("Z", gui::SHORTCUT_SHIFT | gui::SHORTCUT_SYS), "Ctrl+Shift+Z");
        CHECK_TEXT(gui::ShortcutText("Left", gui::SHORTCUT_ALT), "Alt+Left");
        CHECK_TEXT(gui::ShortcutText("Tab", gui::SHORTCUT_CTRL | gui::SHORTCUT_SYS), "Ctrl+Tab");
        CHECK_TEXT(gui::ShortcutText("F5", gui::SHORTCUT_SHIFT), "Shift+F5");
    }
}

void testShortcutHandler(Harness& h)
{
    h.editor.setText("shortcut");
    h.frame();
    h.click(h.textPos(0, 4));
    int asked = 0;
    h.editor.setShortcutHandler([&]() {
        ++asked;
        return IsKeyDown(KEY_LEFT_ALT);
    });
    // a key the app takes doesn't move the cursor, other keys still work
    h.press(KEY_LEFT, {KEY_LEFT_ALT, SYS_KEY});
    CHECK_EQ(h.editor.column(), 5u);
    h.press(KEY_LEFT);
    CHECK_EQ(h.editor.column(), 4u);
    CHECK_EQ(asked > 0, true);
    h.editor.setShortcutHandler({});
}

}  // namespace

int main()
{
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(400, 300, "rlguipp code editor test");
    SetTargetFPS(0);
    {
        Harness harness;
        testEditing(harness);
        testFind(harness);
        testReplace(harness);
        testHighlighting(harness);
        testLikeTextBox(harness);
        testApi(harness);
        testContextMenu(harness);
        testLinks(harness);
        testLocked(harness);
        testShortcutHandler(harness);
        testShortcutText();
    }
    gui::UnloadGui();
    CloseWindow();
    std::printf("%d of %d checks failed\n", g_failures, g_checks);
    return g_failures ? 1 : 0;
}
