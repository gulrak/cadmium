//---------------------------------------------------------------------------------------
// Automated tests of the rlguipp file tree (FileTree) and the drawer (BeginDrawer)
//
// Like the other widget tests, the tree is drawn in a hidden window and the input is
// played with raylib's automation events, the files are in a temporary folder.
//---------------------------------------------------------------------------------------

#include <rlguipp/rlguipp_filetree.hpp>
#include <ghc/fs_fwd.hpp>

#include <cstdio>
#include <fstream>
#include <initializer_list>
#include <set>
#include <string>
#include <vector>

namespace fs = ghc::filesystem;

namespace {

// raylib's AutomationEventType values, the enum is only declared in rcore.c
constexpr unsigned int AUTOMATION_KEY_UP = 1;
constexpr unsigned int AUTOMATION_KEY_DOWN = 2;
constexpr unsigned int AUTOMATION_MOUSE_BUTTON_UP = 5;
constexpr unsigned int AUTOMATION_MOUSE_BUTTON_DOWN = 6;
constexpr unsigned int AUTOMATION_MOUSE_POSITION = 7;

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

void play(unsigned int type, int param0 = 0, int param1 = 0)
{
    AutomationEvent event{};
    event.type = type;
    event.params[0] = param0;
    event.params[1] = param1;
    PlayAutomationEvent(event);
}

void writeFile(const fs::path& path)
{
    fs::create_directories(path.parent_path());
    std::ofstream(path.string()) << "test\n";
}

// The test folder:
//   a-folder/          (empty)
//   b-folder/deep/x.8o
//   b-folder/inner.8o
//   long/file00.8o ... long/file39.8o
//   .hidden
//   alpha.txt
//   main.8o
//   Zeta.8O
fs::path createFixture()
{
    const auto dir = fs::temp_directory_path() / "rlguipp-filetree-test";
    fs::remove_all(dir);
    fs::create_directories(dir / "a-folder");
    writeFile(dir / "b-folder" / "deep" / "x.8o");
    writeFile(dir / "b-folder" / "inner.8o");
    for (int i = 0; i < 40; ++i)
        writeFile(dir / "long" / (std::string("file") + char('0' + i / 10) + char('0' + i % 10) + ".8o"));
    writeFile(dir / ".hidden");
    writeFile(dir / "alpha.txt");
    writeFile(dir / "main.8o");
    writeFile(dir / "Zeta.8O");
    return dir;
}

class Harness
{
public:
    gui::FileTree tree{"tree"};
    std::string root;
    std::optional<std::string> picked;  // what render() returned in the last frames
    bool inDrawer{false};
    bool drawerOpen{false};
    static constexpr Rectangle DRAWER{200, 20, 180, 260};
    Vector2 origin{};

    void frame()
    {
        BeginDrawing();
        ClearBackground(GetColor(GuiGetStyle(DEFAULT, BACKGROUND_COLOR)));
        gui::BeginGui();
        gui::SetRowHeight(20);
        gui::SetSpacing(0);
        if (inDrawer) {
            if (gui::BeginDrawer(DRAWER, "drawer", &drawerOpen)) {
                origin = gui::GetCurrentPos();
                if (auto file = tree.render(-1))
                    picked = file;
                gui::EndDrawer();
            }
        }
        else {
            gui::SetNextWidth(180);
            origin = gui::GetCurrentPos();
            if (auto file = tree.render(200))
                picked = file;
        }
        gui::EndGui();
        EndDrawing();
    }

    // The middle of a row (0 based) of the tree, in screen coordinates
    Vector2 rowPos(int row) const
    {
        const Vector2 offset = inDrawer ? Vector2{DRAWER.x, DRAWER.y} : Vector2{0, 0};
        return {offset.x + origin.x + 60, offset.y + origin.y + 3 + gui::FileTree::ROW_HEIGHT * (float(row) + 0.5f)};
    }

    void press(int key)
    {
        play(AUTOMATION_KEY_DOWN, key);
        frame();
        play(AUTOMATION_KEY_UP, key);
        frame();
    }

    void click(Vector2 pos, MouseButton button = MOUSE_BUTTON_LEFT)
    {
        WaitTime(0.5);  // no double clicks
        play(AUTOMATION_MOUSE_POSITION, (int)pos.x, (int)pos.y);
        play(AUTOMATION_MOUSE_BUTTON_DOWN, button);
        frame();
        play(AUTOMATION_MOUSE_BUTTON_UP, button);
        frame();
    }

    std::string path(const std::string& relative) const { return (fs::path(root) / fs::path(relative)).lexically_normal().string(); }

    // The shown entries relative to the root, found by walking the selection with the cursor keys
    std::vector<std::string> shownEntries()
    {
        std::vector<std::string> result;
        tree.setFocus();
        press(KEY_HOME);
        std::string last;
        while (!tree.selected().empty() && tree.selected() != last) {
            last = tree.selected();
            result.push_back(fs::path(last).lexically_relative(root).generic_string());
            press(KEY_DOWN);
        }
        return result;
    }
};

std::string joined(const std::vector<std::string>& items)
{
    std::string result;
    for (const auto& item : items)
        result += (result.empty() ? "" : " ") + item;
    return result;
}

void testListing(Harness& h)
{
    h.tree.setRoot(h.root + "/");
    CHECK_TEXT(h.tree.root(), h.root);
    h.frame();
    // folders first, names without case, no hidden files
    CHECK_TEXT(joined(h.shownEntries()), "a-folder b-folder long alpha.txt main.8o Zeta.8O");

    // a click on a folder opens and closes it, a click on a file picks it
    h.click(h.rowPos(1));
    CHECK_EQ(h.tree.isExpanded(h.path("b-folder")), true);
    CHECK_TEXT(h.tree.selected(), h.path("b-folder"));
    CHECK_TEXT(joined(h.shownEntries()), "a-folder b-folder b-folder/deep b-folder/inner.8o long alpha.txt main.8o Zeta.8O");
    h.picked.reset();
    h.click(h.rowPos(3));
    CHECK_TEXT(h.picked.value_or("none"), h.path("b-folder/inner.8o"));
    CHECK_TEXT(h.tree.selected(), h.path("b-folder/inner.8o"));
    h.click(h.rowPos(1));
    CHECK_EQ(h.tree.isExpanded(h.path("b-folder")), false);
    CHECK_TEXT(joined(h.shownEntries()), "a-folder b-folder long alpha.txt main.8o Zeta.8O");
    // an opened folder stays open below a closed one
    h.tree.setExpanded(h.path("b-folder/deep"), true);
    h.tree.setExpanded(h.path("b-folder"), true);
    CHECK_TEXT(joined(h.shownEntries()), "a-folder b-folder b-folder/deep b-folder/deep/x.8o b-folder/inner.8o long alpha.txt main.8o Zeta.8O");
    h.tree.setExpanded(h.path("b-folder"), false);

    // the filter keeps all folders, the extensions have no case
    h.tree.setFilter({".8O"});
    CHECK_TEXT(joined(h.shownEntries()), "a-folder b-folder long main.8o Zeta.8O");
    h.tree.setFilter({});
    CHECK_TEXT(joined(h.shownEntries()), "a-folder b-folder long alpha.txt main.8o Zeta.8O");

    // a new root starts with closed folders
    h.tree.setRoot(h.path("b-folder"));
    CHECK_TEXT(joined(h.shownEntries()), "b-folder/deep b-folder/inner.8o");
    h.tree.setRoot(h.root);
    CHECK_EQ(h.tree.isExpanded(h.path("b-folder/deep")), false);
}

void testKeyboard(Harness& h)
{
    h.tree.setRoot(h.root);
    h.click(h.rowPos(1));  // b-folder opens
    CHECK_EQ(h.tree.hasFocus(), true);
    h.press(KEY_LEFT);
    CHECK_EQ(h.tree.isExpanded(h.path("b-folder")), false);
    h.press(KEY_RIGHT);
    CHECK_EQ(h.tree.isExpanded(h.path("b-folder")), true);
    CHECK_TEXT(h.tree.selected(), h.path("b-folder"));
    // Right on an open folder goes to its first entry, Left on an entry to its folder
    h.press(KEY_RIGHT);
    CHECK_TEXT(h.tree.selected(), h.path("b-folder/deep"));
    h.press(KEY_DOWN);
    CHECK_TEXT(h.tree.selected(), h.path("b-folder/inner.8o"));
    h.picked.reset();
    h.press(KEY_ENTER);
    CHECK_TEXT(h.picked.value_or("none"), h.path("b-folder/inner.8o"));
    h.press(KEY_LEFT);
    CHECK_TEXT(h.tree.selected(), h.path("b-folder"));
    // Enter on a folder opens or closes it
    h.press(KEY_ENTER);
    CHECK_EQ(h.tree.isExpanded(h.path("b-folder")), false);
    h.press(KEY_END);
    CHECK_TEXT(h.tree.selected(), h.path("Zeta.8O"));
    h.press(KEY_UP);
    CHECK_TEXT(h.tree.selected(), h.path("main.8o"));

    // a click elsewhere takes the focus, keys do nothing then
    h.click({390, 290});
    CHECK_EQ(h.tree.hasFocus(), false);
    h.press(KEY_UP);
    CHECK_TEXT(h.tree.selected(), h.path("main.8o"));
}

void testRevealAndScroll(Harness& h)
{
    h.tree.setRoot(h.root);
    h.tree.reveal(h.path("b-folder/deep/x.8o"));
    CHECK_EQ(h.tree.isExpanded(h.path("b-folder")), true);
    CHECK_EQ(h.tree.isExpanded(h.path("b-folder/deep")), true);
    CHECK_TEXT(h.tree.selected(), h.path("b-folder/deep/x.8o"));
    h.frame();
    h.picked.reset();
    h.click(h.rowPos(3));
    CHECK_TEXT(h.picked.value_or("none"), h.path("b-folder/deep/x.8o"));

    // the revealed entry is scrolled into view, the tree shows twelve rows
    h.tree.setExpanded(h.path("b-folder"), false);
    h.tree.reveal(h.path("long/file39.8o"));
    h.frame();
    h.frame();
    h.picked.reset();
    h.click(h.rowPos(11));
    CHECK_TEXT(h.picked.value_or("none"), h.path("long/file39.8o"));
    // the cursor keys scroll too
    h.tree.setFocus();
    h.press(KEY_HOME);
    h.press(KEY_DOWN);
    h.picked.reset();
    h.click(h.rowPos(2));
    CHECK_TEXT(h.tree.selected(), h.path("long"));
    CHECK_EQ(h.tree.isExpanded(h.path("long")), false);
    h.tree.setExpanded(h.path("long"), false);
    h.tree.setExpanded(h.path("b-folder/deep"), false);
}

void testRefresh(Harness& h)
{
    h.tree.setRoot(h.root);
    h.tree.setExpanded(h.path("a-folder"), true);
    CHECK_TEXT(joined(h.shownEntries()), "a-folder b-folder long alpha.txt main.8o Zeta.8O");
    writeFile(fs::path(h.root) / "a-folder" / "new.8o");
    writeFile(fs::path(h.root) / "beta.txt");
    CHECK_TEXT(joined(h.shownEntries()), "a-folder b-folder long alpha.txt main.8o Zeta.8O");
    h.tree.refresh();
    CHECK_TEXT(joined(h.shownEntries()), "a-folder a-folder/new.8o b-folder long alpha.txt beta.txt main.8o Zeta.8O");
    fs::remove_all(fs::path(h.root) / "a-folder");
    fs::remove(fs::path(h.root) / "beta.txt");
    h.tree.refresh();
    CHECK_TEXT(joined(h.shownEntries()), "b-folder long alpha.txt main.8o Zeta.8O");
    fs::create_directories(fs::path(h.root) / "a-folder");
    h.tree.refresh();
    CHECK_EQ(h.tree.isExpanded(h.path("a-folder")), false);
}

void testMarks(Harness& h)
{
    h.tree.setRoot(h.root);
    std::set<std::string> asked;
    h.tree.setMarkHandler([&asked](const std::string& path, bool isDirectory) {
        asked.insert(path + (isDirectory ? "/" : ""));
        return gui::FileTreeMark{ICON_STAR, RED, "(main)"};
    });
    h.frame();
    CHECK_EQ(asked.size(), 6u);
    CHECK_EQ(asked.count(h.path("long") + "/"), 1u);
    CHECK_EQ(asked.count(h.path("main.8o")), 1u);
    h.tree.setMarkHandler({});
}

void testContextMenu(Harness& h)
{
    h.tree.setRoot(h.root);
    std::string menuPath;
    bool menuIsDirectory = false;
    std::string action;
    h.tree.setContextMenuHandler([&](gui::PopupMenu::Menu& menu, const std::string& path, bool isDirectory) {
        menuPath = path;
        menuIsDirectory = isDirectory;
        if (!isDirectory)
            menu.push_back(gui::PopupMenu::Action("Use", [&action, path]() { action = path; }));
    });
    h.click(h.rowPos(4), MOUSE_BUTTON_RIGHT);
    CHECK_TEXT(menuPath, h.path("main.8o"));
    CHECK_EQ(menuIsDirectory, false);
    CHECK_TEXT(h.tree.selected(), h.path("main.8o"));
    CHECK_EQ(gui::PopupMenu::IsOpen(), true);
    // the first item is right below the click
    h.click({h.rowPos(4).x + 20, h.rowPos(4).y + 10});
    CHECK_TEXT(action, h.path("main.8o"));
    CHECK_EQ(gui::PopupMenu::IsOpen(), false);
    // without items there is no menu, a click below the entries is for the root folder
    h.click(h.rowPos(1), MOUSE_BUTTON_RIGHT);
    CHECK_TEXT(menuPath, h.path("b-folder"));
    CHECK_EQ(menuIsDirectory, true);
    CHECK_EQ(gui::PopupMenu::IsOpen(), false);
    h.click(h.rowPos(10), MOUSE_BUTTON_RIGHT);
    CHECK_TEXT(menuPath, h.root);
    h.tree.setContextMenuHandler({});
}

void testDrawer(Harness& h)
{
    h.tree.setRoot(h.root);
    h.inDrawer = true;
    h.drawerOpen = true;
    h.frame();
    h.frame();
    // a click inside picks the file and keeps it open
    h.picked.reset();
    h.click(h.rowPos(5));
    CHECK_TEXT(h.picked.value_or("none"), h.path("Zeta.8O"));
    CHECK_EQ(h.drawerOpen, true);
    // a context menu in the drawer opens at the click
    std::string action;
    h.tree.setContextMenuHandler([&action](gui::PopupMenu::Menu& menu, const std::string& path, bool) {
        menu.push_back(gui::PopupMenu::Action("Use", [&action, path]() { action = path; }));
    });
    h.click(h.rowPos(4), MOUSE_BUTTON_RIGHT);
    CHECK_EQ(gui::PopupMenu::IsOpen(), true);
    h.click({h.rowPos(4).x + 20, h.rowPos(4).y + 10});
    CHECK_TEXT(action, h.path("main.8o"));
    CHECK_EQ(h.drawerOpen, true);
    // a click outside of an open menu only closes the menu
    h.click(h.rowPos(4), MOUSE_BUTTON_RIGHT);
    h.click({20, 150});
    CHECK_EQ(gui::PopupMenu::IsOpen(), false);
    CHECK_EQ(h.drawerOpen, true);
    h.tree.setContextMenuHandler({});
    // a click outside closes it, also when it ends inside
    h.click({20, 150});
    CHECK_EQ(h.drawerOpen, false);
    h.drawerOpen = true;
    h.frame();
    h.frame();
    play(AUTOMATION_MOUSE_POSITION, 20, 150);
    play(AUTOMATION_MOUSE_BUTTON_DOWN, MOUSE_BUTTON_LEFT);
    h.frame();
    play(AUTOMATION_MOUSE_POSITION, (int)h.rowPos(1).x, (int)h.rowPos(1).y);
    play(AUTOMATION_MOUSE_BUTTON_UP, MOUSE_BUTTON_LEFT);
    h.frame();
    CHECK_EQ(h.drawerOpen, true);
    // a drag from inside to outside keeps it open, Escape closes it
    play(AUTOMATION_MOUSE_BUTTON_DOWN, MOUSE_BUTTON_LEFT);
    h.frame();
    play(AUTOMATION_MOUSE_POSITION, 20, 150);
    play(AUTOMATION_MOUSE_BUTTON_UP, MOUSE_BUTTON_LEFT);
    h.frame();
    CHECK_EQ(h.drawerOpen, true);
    h.press(KEY_ESCAPE);
    CHECK_EQ(h.drawerOpen, false);
    h.inDrawer = false;
}

}  // namespace

int main()
{
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(400, 300, "rlguipp file tree test");
    SetExitKey(KEY_NULL);
    SetTargetFPS(0);
    const auto dir = createFixture();
    {
        Harness harness;
        harness.root = dir.lexically_normal().string();
        testListing(harness);
        testKeyboard(harness);
        testRevealAndScroll(harness);
        testRefresh(harness);
        testMarks(harness);
        testContextMenu(harness);
        testDrawer(harness);
    }
    fs::remove_all(dir);
    gui::UnloadGui();
    CloseWindow();
    std::printf("%d of %d checks failed\n", g_failures, g_checks);
    return g_failures ? 1 : 0;
}
