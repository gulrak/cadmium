//---------------------------------------------------------------------------------------
// Automated tests of the rlguipp text widgets (TextBox, TextBoxMulti)
//
// The widgets are drawn in a hidden window, the input is played with raylib's automation
// events (key and mouse button states, mouse position), so modifier keys work like real
// ones. Typed characters can't be played that way, text is entered by pasting from a
// clipboard handler that never touches the system clipboard.
//---------------------------------------------------------------------------------------

#include <rlguipp/rlguipp.hpp>

#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <string>

#if defined(PLATFORM_DESKTOP) || defined(PLATFORM_DESKTOP_GLFW)
extern "C" const char* glfwGetKeyName(int key, int scancode);
#endif

namespace {

// raylib's AutomationEventType values, the enum is only declared in rcore.c
constexpr unsigned int AUTOMATION_KEY_UP = 1;
constexpr unsigned int AUTOMATION_KEY_DOWN = 2;
constexpr unsigned int AUTOMATION_MOUSE_BUTTON_UP = 5;
constexpr unsigned int AUTOMATION_MOUSE_BUTTON_DOWN = 6;
constexpr unsigned int AUTOMATION_MOUSE_POSITION = 7;
constexpr unsigned int AUTOMATION_MOUSE_WHEEL_MOTION = 8;

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
    // the printable keys GLFW knows, the range KEY_APOSTROPHE to KEY_GRAVE has gaps
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

class Harness
{
public:
    std::string single;
    std::string multi;
    char buffer[8]{};
    std::string clipboard;
    bool multiReadOnly{false};
    float multiWidth{0};
    bool singleChanged{false};
    bool singleFinished{false};
    bool multiChanged{false};
    bool multiFinished{false};
    Rectangle singleRect{};
    Rectangle multiRect{};

    Harness()
    {
        gui::SetClipboardHandler([this]() { return clipboard; }, [this](const std::string& text) { clipboard = text; });
    }

    void frame()
    {
        BeginDrawing();
        ClearBackground(GetColor(GuiGetStyle(DEFAULT, BACKGROUND_COLOR)));
        gui::BeginGui();
        gui::SetRowHeight(20);
        gui::SetSpacing(10);
        singleChanged = gui::TextBox("single", single, 64);
        singleFinished = singleFinished || gui::IsLastWidgetEditFinished();
        singleRect = gui::GetLastWidgetRect();
        gui::TextBox("buffer", buffer, sizeof(buffer));
        if (multiWidth > 0)
            gui::SetNextWidth(multiWidth);
        if (multiReadOnly)
            GuiSetStyle(TEXTBOX, TEXT_READONLY, 1);
        multiChanged = gui::TextBoxMulti(64, "multi", multi, 1024);
        multiFinished = multiFinished || gui::IsLastWidgetEditFinished();
        multiRect = gui::GetLastWidgetRect();
        GuiSetStyle(TEXTBOX, TEXT_READONLY, 0);
        gui::EndGui();
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

    // The screen position of the text before byte `index` of the single-line box
    Vector2 singlePos(size_t index) const
    {
        const float x = singleRect.x + (float)GuiGetStyle(TEXTBOX, BORDER_WIDTH) + (float)GuiGetStyle(TEXTBOX, TEXT_PADDING);
        return {x + gui::GetTextSize(single.substr(0, index).c_str()).x + 1, singleRect.y + singleRect.height / 2};
    }

    Vector2 multiTopLeft() const
    {
        return {multiRect.x + (float)GuiGetStyle(TEXTBOX, BORDER_WIDTH) + (float)GuiGetStyle(TEXTBOX, TEXT_PADDING) + 1, multiRect.y + 4};
    }

private:
    Vector2 _lastClick{-100, -100};
};

void testSingleLine(Harness& h)
{
    h.single = "hello world";
    h.frame();
    h.click({h.singleRect.x + h.singleRect.width - 4, h.singlePos(0).y});
    CHECK_EQ(gui::HasKeyboardFocus(gui::GetId("single")), true);

    // insert at the start and at the end
    h.press(KEY_HOME);
    h.paste("X");
    CHECK_TEXT(h.single, "Xhello world");
    h.press(KEY_END);
    h.paste("!");
    CHECK_TEXT(h.single, "Xhello world!");

    // select with shift, copy, cut, undo, redo
    h.press(KEY_LEFT);
    for (int i = 0; i < 5; ++i)
        h.press(KEY_LEFT, {KEY_LEFT_SHIFT});
    h.shortcut('C');
    CHECK_TEXT(h.clipboard, "world");
    h.shortcut('X');
    CHECK_TEXT(h.single, "Xhello !");
    CHECK_TEXT(h.clipboard, "world");
    h.shortcut('Z');
    CHECK_TEXT(h.single, "Xhello world!");
    h.shortcut('Z', true);
    CHECK_TEXT(h.single, "Xhello !");
    h.shortcut('Z');
    CHECK_TEXT(h.single, "Xhello world!");

    // a selection is replaced by the pasted text
    h.paste("there");
    CHECK_TEXT(h.single, "Xhello there!");

    // word movement and word deletion, punctuation is a word of its own
    h.press(KEY_END);
    h.press(KEY_LEFT, {WORD_KEY});
    h.paste("-");
    CHECK_TEXT(h.single, "Xhello there-!");
    h.press(KEY_LEFT, {WORD_KEY});
    h.press(KEY_LEFT, {WORD_KEY});
    h.paste("_");
    CHECK_TEXT(h.single, "Xhello _there-!");
    h.press(KEY_END);
    h.press(KEY_BACKSPACE, {WORD_KEY});
    CHECK_TEXT(h.single, "Xhello _there");
    h.press(KEY_BACKSPACE, {WORD_KEY});
    CHECK_TEXT(h.single, "Xhello ");
    h.press(KEY_HOME);
    h.press(KEY_DELETE);
    CHECK_TEXT(h.single, "hello ");

    // UTF-8 is deleted as whole characters
    h.press(KEY_END);
    h.paste("\xc3\xa4\xc3\xb6");  // "äö"
    h.press(KEY_BACKSPACE);
    CHECK_TEXT(h.single, "hello \xc3\xa4");
    h.press(KEY_LEFT);
    h.press(KEY_DELETE);
    CHECK_TEXT(h.single, "hello ");

    // double click selects a word, triple click all
    h.single = "alpha beta gamma";
    h.frame();
    h.click(h.singlePos(8), 2);
    h.shortcut('C');
    CHECK_TEXT(h.clipboard, "beta");
    h.frame();
    h.click(h.singlePos(8), 3);
    h.shortcut('C');
    CHECK_TEXT(h.clipboard, "alpha beta gamma");

    // a mouse drag selects
    h.frame();
    h.drag(h.singlePos(6), h.singlePos(10));
    h.shortcut('C');
    CHECK_TEXT(h.clipboard, "beta");

    // shift click extends from the cursor
    h.click(h.singlePos(0));
    h.click(h.singlePos(5), 1, {KEY_LEFT_SHIFT});
    h.shortcut('C');
    CHECK_TEXT(h.clipboard, "alpha");

    // select all, line breaks and tabs become spaces, a trailing line break is dropped
    h.shortcut('A');
    h.paste("a\nb\tc\r\n");
    CHECK_TEXT(h.single, "a b c");

    // the maximum length cuts pasted text, also inside UTF-8
    h.shortcut('A');
    h.paste(std::string(63, 'x') + "\xc3\xa4");
    CHECK_EQ(h.single.size(), size_t(63));

    // text changed by the application drops the undo history
    h.single = "set by app";
    h.frame();
    h.shortcut('Z');
    CHECK_TEXT(h.single, "set by app");

    // enter ends the editing, that is reported once
    CHECK_EQ(h.singleFinished, false);
    h.press(KEY_ENTER);
    CHECK_EQ(h.singleFinished, true);
    CHECK_EQ(gui::HasKeyboardFocus(gui::GetId("single")), false);
    h.singleFinished = false;
    h.frame();
    CHECK_EQ(h.singleFinished, false);

    // a click elsewhere ends the editing
    h.click(h.singlePos(0));
    CHECK_EQ(h.singleFinished, false);
    h.click({h.singleRect.x + 10, h.singleRect.y + 200});
    CHECK_EQ(gui::HasKeyboardFocus(gui::GetId("single")), false);
    CHECK_EQ(h.singleFinished, true);
}

void testCharBuffer(Harness& h)
{
    h.frame();
    const auto rect = Rectangle{h.singleRect.x, h.singleRect.y + h.singleRect.height + 10, h.singleRect.width, h.singleRect.height};
    h.click({rect.x + 20, rect.y + rect.height / 2});
    h.paste("123456789");
    CHECK_TEXT(h.buffer, "1234567");
    h.press(KEY_BACKSPACE);
    CHECK_TEXT(h.buffer, "123456");
    h.press(KEY_ENTER);
}

void testMultiLine(Harness& h)
{
    h.multi = "line1\nline2\nline3";
    h.frame();
    h.click(h.multiTopLeft());
    CHECK_EQ(gui::HasKeyboardFocus(gui::GetId("multi")), true);
    h.press(KEY_DOWN);
    h.press(KEY_DOWN);
    h.paste("Z");
    CHECK_TEXT(h.multi, "line1\nline2\nZline3");
    h.press(KEY_UP);
    h.press(KEY_END);
    h.press(KEY_ENTER);
    CHECK_TEXT(h.multi, "line1\nline2\n\nZline3");
    CHECK_EQ(h.multiFinished, false);  // enter adds a line break, it doesn't end the editing
    h.shortcut('Z');
    CHECK_TEXT(h.multi, "line1\nline2\nZline3");

    // pasted line breaks are kept, \r\n and \r become \n
    h.shortcut('A');
    h.paste("a\r\nb\rc");
    CHECK_TEXT(h.multi, "a\nb\nc");

    // a triple click selects the paragraph
    h.multi = "first paragraph\nsecond one";
    h.frame();
    h.click(h.multiTopLeft(), 3);
    h.shortcut('C');
    CHECK_TEXT(h.clipboard, "first paragraph");

    // wrapping: a box for about "aaaa bbbb " wraps "aaaa bbbb cccc" after the second space
    const float border = (float)GuiGetStyle(TEXTBOX, BORDER_WIDTH);
    const float padding = (float)GuiGetStyle(TEXTBOX, TEXT_PADDING);
    h.multiWidth = gui::GetTextSize("aaaa bbbb c").x + 2 * (border + padding);
    h.multi = "aaaa bbbb cccc";
    h.frame();
    h.frame();
    h.click(h.multiTopLeft());
    h.press(KEY_END);
    h.paste("!");
    CHECK_TEXT(h.multi, "aaaa bbbb! cccc");
    // down moves into the wrapped part of the line
    h.press(KEY_HOME);
    h.press(KEY_DOWN);
    h.paste("#");
    CHECK_TEXT(h.multi, "aaaa bbbb! #cccc");
    h.multiWidth = 0;

    // read-only: selecting and copying work, editing doesn't
    h.multi = "read only";
    h.multiReadOnly = true;
    h.frame();
    h.click(h.multiTopLeft(), 2);
    h.shortcut('C');
    CHECK_TEXT(h.clipboard, "read");
    h.paste("changed");
    h.press(KEY_BACKSPACE);
    h.shortcut('X');
    CHECK_TEXT(h.multi, "read only");
    h.multiReadOnly = false;

    // a long text scrolls with the wheel, then a click hits a later line
    h.multi.clear();
    for (int i = 0; i < 20; ++i)
        h.multi += "row" + std::to_string(i) + "\n";
    h.frame();
    play(AUTOMATION_MOUSE_POSITION, (int)h.multiTopLeft().x, (int)h.multiTopLeft().y + 10);
    play(AUTOMATION_MOUSE_WHEEL_MOTION, 0, -2);
    h.frame();
    h.frame();
    h.click(h.multiTopLeft(), 2);
    h.shortcut('C');
    CHECK_EQ(h.clipboard != "row0" && h.clipboard.rfind("row", 0) == 0, true);

    // escape ends the editing
    CHECK_EQ(h.multiFinished, false);
    h.press(KEY_ESCAPE);
    CHECK_EQ(h.multiFinished, true);
    CHECK_EQ(gui::HasKeyboardFocus(gui::GetId("multi")), false);
}

}  // namespace

int main()
{
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(400, 300, "rlguipp text edit test");
    SetTargetFPS(0);
    {
        Harness harness;
        testSingleLine(harness);
        testCharBuffer(harness);
        testMultiLine(harness);
    }
    gui::UnloadGui();
    CloseWindow();
    std::printf("%d of %d checks failed\n", g_failures, g_checks);
    return g_failures ? 1 : 0;
}
