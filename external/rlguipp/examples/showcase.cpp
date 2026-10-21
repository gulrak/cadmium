//---------------------------------------------------------------------------------------
//
// Copyright (c) 2024, Steffen Schümann <s.schuemann@pobox.com>
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
// rlguipp Showcase - demonstrates the layout and widget features of rlguipp.
//
// Structure mirrors real-world Cadmium usage:
//   - StatusBar at bottom (called first so layout reserves the space)
//   - Burger-menu toolbar at top
//   - Tab-based main content area
//   - Floating PopupMenu, modal WindowBox, and inline Popup
//
// Golden image tests:
//   rlguipp_showcase --golden <approved-dir> --out <output-dir> [--update]
// renders a fixed set of tabs, menus and dialogs in a hidden window and compares them
// pixel by pixel with the approved PNGs, see README.md "Tests".
//---------------------------------------------------------------------------------------

#include <rlguipp/rlguiapp.hpp>
#include <rlguipp/rlguipp.hpp>
#include <rlguipp/rlguipp_filedialog.hpp>
#include <rlguipp/rlguipp_leds.hpp>
#include <stylemanager.hpp>
#include <fmt/format.h>
#include <ghc/fs_fwd.hpp>
#include <cmath>
#include <ctime>
#include <cstdio>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace fs = ghc::filesystem;

using namespace gui;

class ShowcaseApp : public gui::Application {
public:
    // In golden mode the window is hidden, the mouse is moved out of the gui and time stands still
    explicit ShowcaseApp(bool goldenMode = false)
        : gui::Application("rlGuipp Showcase", goldenMode ? Config{800, 600, 1, 60, FLAG_WINDOW_HIDDEN} : Config{800, 600, 2})
        , _goldenMode(goldenMode)
    {
        if (_goldenMode)
            SetMousePosition(-10000, -10000);
        _styleManager.addTheme("Warm",  30.0f, 90, false);
        _styleManager.addTheme("Light", 200.0f, 90, true);
        _styleManager.setTheme(0);
        _fileDlg.path = fs::current_path().string();
        buildMenu();
    }

private:
    // Icon-only button helper: inverts colors when active to show pressed state.
    static bool iconButton(int iconId, bool active = false)
    {
        StyleManager::Scope guard;
        if (active) {
            int fg = guard.getStyle(Style::TEXT_COLOR_NORMAL);
            int bg = guard.getStyle(Style::BASE_COLOR_NORMAL);
            guard.setStyle(Style::BASE_COLOR_NORMAL, fg);
            guard.setStyle(Style::TEXT_COLOR_NORMAL, bg);
        }
        SetNextWidth(20);
        return Button(GuiIconText(iconId, ""));
    }

    void buildMenu()
    {
        _themeNames = {"Default", "Warm", "Light"};

        PopupMenu::Menu themeMenu;
        for (int i = 0; i < (int)_themeNames.size(); ++i) {
            themeMenu.push_back(PopupMenu::Action(_themeNames[i], [this, i]() {
                _themeIdx = i;
                _styleManager.setTheme(i);
            }));
        }

        _mainMenu = {
            PopupMenu::Action("About...", [this]() { _aboutOpen = true; _aboutScroll = {}; }),
            PopupMenu::Separator(),
            PopupMenu::Submenu("Theme", themeMenu),
            PopupMenu::Separator(),
            PopupMenu::Action("Quit", [this]() { requestClose(); }, "⌘Q"),
        };

        PopupMenu::Menu tabMenu;
        const char* tabNames[] = {"Layout", "Controls", "Input", "Sliders", "Lists", "Windows", "Dialogs", "Widgets"};
        for (int i = 0; i < 8; ++i) {
            tabMenu.push_back(PopupMenu::Action(tabNames[i], [this, i]() { _activeTab = i; }));
        }
        auto openFileDialog = [this](FileDialogInfo::DialogType type, const char* title) {
            return [this, type, title]() {
                _fileDlg = {type, title, _fileDlg.path};
                _fileDlgOpen = true;
            };
        };
        _menuBar = {
            {"File", {
                PopupMenu::Action("Open File...", openFileDialog(FileDialogInfo::OpenFile, "Open File")),
                PopupMenu::Action("Save File As...", openFileDialog(FileDialogInfo::SaveFile, "Save File As...")),
                PopupMenu::Separator(),
                PopupMenu::Action("Quit", [this]() { requestClose(); }, "⌘Q"),
            }},
            {"View", {
                PopupMenu::Submenu("Theme", themeMenu),
                PopupMenu::Submenu("Go to Tab", std::move(tabMenu)),
                PopupMenu::Separator(),
                PopupMenu::Toggle("Status Bar", &_showStatusBar),
            }},
            {"Help", {
                PopupMenu::Action("About rlGuipp Showcase...", [this]() { _aboutOpen = true; _aboutScroll = {}; }),
            }},
        };
    }

    // -----------------------------------------------------------------------
    // Main draw - called every frame inside an active BeginGui/EndGui pair.
    // -----------------------------------------------------------------------

    void drawGui() override
    {
        SetStyle(STATUSBAR, TEXT_PADDING, 4);
        SetStyle(LISTVIEW, SCROLLBAR_WIDTH, 6);
        SetStyle(DROPDOWNBOX, DROPDOWN_ITEMS_SPACING, 0);
        SetStyle(SPINNER, TEXT_PADDING, 4);

        // Status bar - called first; rlguipp reserves space at the bottom.
        SetRowHeight(16);
        SetSpacing(0);
        if (_showStatusBar) {
            StatusBar({
                {0.5f,  "rlGuipp Showcase v" RLGUIPP_VERSION_STRING},
                {0.25f, fmt::format("{:.0f} FPS", _goldenMode ? 60.0f : (float)GetFPS()).c_str()},
                {0.25f, fmt::format("Theme: {}", _themeNames[_themeIdx]).c_str()},
            });
        }

        // Toolbar - burger menu left, menu bar fills the remaining space.
        SetRowHeight(20);
        SetSpacing(0);
        BeginColumns();
        {
            if (iconButton(ICON_BURGER_MENU, PopupMenu::IsMenuOpen(_mainMenu)) && !PopupMenu::IsOpen())
                PopupMenu::Open({1.0f, GetCurrentPos().y + 20}, _mainMenu);
            SetTooltip("Main menu  (About is in the menu)");
            MenuBar("menubar", _menuBar);
            _menuBarRect = GetLastWidgetRect();
        }
        EndColumns();

        // Main content panel with tab view.
        //Begin();
        BeginPanel("rlGuipp Feature Showcase", {4, 4});
        {
            SetSpacing(0);
            BeginTabView(&_activeTab);
            if (BeginTab("Layout",   {4, 4})) { drawLayoutTab();   EndTab(); }
            if (BeginTab("Controls", {4, 4})) { drawControlsTab(); EndTab(); }
            if (BeginTab("Input",    {4, 4})) { drawInputTab();    EndTab(); }
            if (BeginTab("Sliders",  {4, 4})) { drawSlidersTab();  EndTab(); }
            if (BeginTab("Lists",    {4, 4})) { drawListsTab();    EndTab(); }
            if (BeginTab("Windows",  {4, 4})) { drawWindowsTab();  EndTab(); }
            if (BeginTab("Dialogs",  {4, 4})) { drawDialogsTab();  EndTab(); }
            if (BeginTab("Widgets",  {4, 4})) { drawWidgetsTab();  EndTab(); }
            EndTabView();
        }
        EndPanel();
        //End();

        drawAboutModal();
        drawDemoModal();
        drawFileDialog();
    }

    // -----------------------------------------------------------------------
    // Tab: Layout - containers, spacing, indenting
    // -----------------------------------------------------------------------

    void drawLayoutTab()
    {
        SetSpacing(4);

        Label("BeginColumns / EndColumns  (side-by-side panels):");
        BeginColumns();
        {
            SetSpacing(4);
            SetNextRelativeWidth(0.5f);
            BeginPanel("Left Panel", {4, 4});
            SetSpacing(3);
            Label("Content inside BeginPanel");
            Label("Second label row");
            EndPanel();

            BeginPanel("Right Panel", {4, 4});
            SetSpacing(3);
            Label("Content inside BeginPanel");
            Space(8);
            Label("With an 8px gap above");
            EndPanel();
        }
        EndColumns();

        Space(6);

        BeginGroupBox("BeginGroupBox  (label in border gap)");
        {
            SetSpacing(3);
            Label("Content inside a GroupBox");
            SetIndent(12);
            Label("Indented via SetIndent(12)");
            Label("A second indented row");
            SetIndent(0);
            Label("Back to zero indent");
        }
        EndGroupBox();

        Space(6);

        auto pos   = GetCurrentPos();
        auto avail = GetContentAvailable();
        Label(fmt::format("GetCurrentPos()        → ({:.0f}, {:.0f})", pos.x, pos.y).c_str());
        Label(fmt::format("GetContentAvailable()  → x={:.0f}  y={:.0f}  w={:.0f}  h={:.0f}", avail.x, avail.y, avail.width, avail.height).c_str());

        Space(6);
        Separator();
        Space(4);
        Label("↑ Separator() with Space() above and below");
    }

    // -----------------------------------------------------------------------
    // Tab: Controls - buttons, toggles, checkboxes
    // -----------------------------------------------------------------------

    void drawControlsTab()
    {
        SetSpacing(4);
        BeginColumns();
        {
            SetSpacing(4);

            // Left column
            SetNextRelativeWidth(0.5f);
            Begin();
            {
                SetSpacing(4);
                Label("Button  /  LabelButton:");
                if (Button("Regular Button")) {}
                SetTooltip("Tooltip set via SetTooltip()");
                if (LabelButton("Label Button (clickable text)")) {}

                Space(4);
                Label("Toggle:");
                Toggle("Toggle Me", &_toggleState);

                Space(4);
                Label("ToggleGroup:");
                SetNextWidth(int(GetContentAvailable().width * 0.33f));
                ToggleGroup("One;Two;Three", &_toggleGroupState);

                Space(4);
                Label("CheckBox:");
                CheckBox("Option A", &_checkbox1);
                CheckBox("Option B (starts on)", &_checkbox2);
            }
            End();

            // Right column
            Begin();
            {
                SetSpacing(4);
                Label("ButtonMulti (detects any mouse button):");
                auto clicked = ButtonMulti("Left / Right / Middle Click");
                if (clicked) {
                    switch (*clicked) {
                        case MOUSE_BUTTON_LEFT:   _lastClickedButton = "Left";   break;
                        case MOUSE_BUTTON_RIGHT:  _lastClickedButton = "Right";  break;
                        case MOUSE_BUTTON_MIDDLE: _lastClickedButton = "Middle"; break;
                        default:                  _lastClickedButton = "Other";  break;
                    }
                }
                Label(fmt::format("Last clicked: {}", _lastClickedButton.empty() ? "none" : _lastClickedButton).c_str());

                Space(4);
                Label("GuiDisable() / GuiEnable():");
                GuiDisable();
                Button("Disabled Button");
                bool alwaysOn = true;
                CheckBox("Disabled CheckBox", &alwaysOn);
                Toggle("Disabled Toggle", &alwaysOn);
                GuiEnable();

                Space(4);
                Label("Icon buttons via iconButton() helper:");
                BeginColumns();
                SetSpacing(2);
                iconButton(ICON_FILE_OPEN);  SetTooltip("Open");
                iconButton(ICON_FILE_SAVE);  SetTooltip("Save");
                iconButton(ICON_UNDO);       SetTooltip("Undo");
                iconButton(ICON_REDO);       SetTooltip("Redo");
                iconButton(ICON_GEAR);       SetTooltip("Settings");
                EndColumns();
            }
            End();
        }
        EndColumns();
    }

    // -----------------------------------------------------------------------
    // Tab: Input - text boxes, spinners, dropdowns
    // -----------------------------------------------------------------------

    void drawInputTab()
    {
        SetSpacing(4);

        Label("TextBox (char* variant):");
        TextBox("charBuf", _charBuf, sizeof(_charBuf));
        SetTooltip("Click to edit - char array");

        Label("TextBox (std::string variant):");
        TextBox("stringBuf", _stringBuf, 256);
        SetTooltip("Click to edit - std::string");

        Space(4);
        BeginColumns();
        {
            SetSpacing(4);

            SetNextRelativeWidth(0.5f);
            Begin();
            {
                SetSpacing(4);
                Label("Spinner:");
                Spinner("##spinner", &_spinnerVal, 0, 100);

                Space(4);
                Label("ValueBox:");
                ValueBox("##valueBox", &_valueBoxVal, 0, 999);

                Space(4);
                Label("ComboBox:");
                ComboBox("Alpha;Beta;Gamma;Delta", &_comboActive);
            }
            End();

            Begin();
            {
                SetSpacing(4);
                Label("DropdownBox (opens down):");
                DropdownBox("Red;Green;Blue;Yellow", &_dropdownActive);

                // Leave vertical room so the open dropdown does not clip.
                Space(44);

                Label("DropdownBox (directionUp = true):");
                DropdownBox("Cat;Dog;Fish;Bird", &_dropdownUpActive, true);
            }
            End();
        }
        EndColumns();
    }

    // -----------------------------------------------------------------------
    // Tab: Sliders - Slider, SliderBar, ProgressBar, ColorPicker
    // -----------------------------------------------------------------------

    void drawSlidersTab()
    {
        SetSpacing(4);

        SetReserve(50);

        Label("Slider (left + right label):");
        Slider("Min", "Max", &_sliderA, 0.0f, 1.0f);

        Label("Slider (auto right label from format):");
        Slider("Val", &_sliderB, 0.0f, 100.0f);

        Space(4);
        Label("SliderBar (left + right label):");
        SliderBar("0", "1", &_sliderBar, 0.0f, 1.0f);

        Label("SliderBar (auto right label):");
        SliderBar("Pos", &_sliderBar2, -50.0f, 50.0f);

        Space(4);
        Label("ProgressBar (animated, auto-advances each frame):");
        _progress = std::fmod(_progress + frameTime() * 0.25f, 1.0f);
        ProgressBar("0%", "100%", _progress, 0.0f, 1.0f);

        SetReserve(0);

        Space(6);
        Separator();
        Space(4);

        BeginColumns();
        {
            SetSpacing(4);
            SetNextRelativeWidth(0.5f);
            Begin();
            {
                Label("ColorPicker:");
                SetNextWidth(120.0f);
                ColorPicker(&_pickedColor);
            }
            End();
            Begin();
            {
                SetSpacing(4);
                Label("ColorPanel:");
                SetNextWidth(40.0f);
                ColorPanel("", &_pickedColor);
                Space(4);
                Label("ColorBarAlpha:");
                ColorBarAlpha("", &_colorAlpha);
                Label(fmt::format("Alpha: {:.2f}  →  #{:02X}{:02X}{:02X}{:02X}",
                    _colorAlpha,
                    _pickedColor.r, _pickedColor.g, _pickedColor.b,
                    (int)(_colorAlpha * 255)).c_str());
            }
            End();
        }
        EndColumns();
    }

    // -----------------------------------------------------------------------
    // Tab: Lists - ListView, SimpleListView, BeginTableView
    // -----------------------------------------------------------------------

    void drawListsTab()
    {
        SetSpacing(4);
        auto area = GetContentAvailable();
        float halfH = (area.height - 20) / 2.5f - 6;

        BeginColumns();
        {
            SetSpacing(4);

            // Left: classic raygui-style list views
            SetNextRelativeWidth(0.5f);
            Begin();
            {
                SetSpacing(4);
                Label("ListView (semicolon-separated string):");
                ListView(halfH,
                    "Apple;Banana;Cherry;Date;Elderberry;Fig;Grape;Honeydew;Kiwi;Lemon",
                    &_listScrollIdx, &_listActive);

                Space(4);
                Label("SimpleListView (std::vector<std::string>):");
                static std::vector<std::string> items = {
                    "Item 1 - first",  "Item 2 - second", "Item 3 - third",
                    "Item 4 - fourth", "Item 5 - fifth",  "Item 6 - sixth",
                    "Item 7 - seventh","Item 8 - eighth",
                };
                SimpleListView(halfH, "simpleList", items, &_simpleListScroll, &_simpleListActive);
            }
            End();

            // Right: table view with header + data rows
            Begin();
            {
                SetSpacing(4);
                Label("BeginTableView (header + 20 data rows, scrollable):");
                BeginTableView(area.height - 32, "table", {TableColumn::Fixed(120), TableColumn::Fixed(80), TableColumn::Fixed(80)}, &_tableScroll);

                // Header row with tinted background
                Color headerBg = GetColor(GetStyle(DEFAULT, BASE_COLOR_FOCUSED));
                if (TableNextRow(16, headerBg)) {
                    if (TableNextColumn()) Label("Name");
                    if (TableNextColumn())  Label("Value");
                    if (TableNextColumn())  Label("Status");
                }

                // Data rows
                static const char* statuses[] = {"OK", "Warn", "Error"};
                for (int i = 0; i < 20; ++i) {
                    if (!TableNextRow(16)) continue;
                    if (TableNextColumn()) Label(fmt::format("Row {:02d}", i + 1).c_str());
                    if (TableNextColumn())  Label(fmt::format("{}", i * 7 + 3).c_str());
                    if (TableNextColumn())  Label(statuses[i % 3]);
                }
                EndTableView();
            }
            End();
        }
        EndColumns();
    }

    // -----------------------------------------------------------------------
    // Tab: Windows - BeginWindowBox, BeginPopup
    // -----------------------------------------------------------------------

    void drawWindowsTab()
    {
        SetSpacing(4);

        Label("BeginWindowBox  (draggable modal):");
        if (Button("Open Demo Modal"))
            _demoModalOpen = true;

        Space(8);
        Label("BeginPopup  (inline popup anchored to a button):");
        if (Button("Open Inline Popup"))
            _inlinePopupOpen = true;

        auto anchor = GetLastWidgetRect();
        if (BeginPopup({anchor.x, anchor.y + anchor.height + 2, 120, 56}, "inlinePopup", &_inlinePopupOpen)) {
            SetSpacing(0);
            SetRowHeight(14);
            if (Button("Action A")) { _inlinePopupResult = "Action A"; _inlinePopupOpen = false; }
            if (Button("Action B")) { _inlinePopupResult = "Action B"; _inlinePopupOpen = false; }
            if (Button("Close"))    { _inlinePopupOpen = false; }
            EndPopup();
        }
        if (!_inlinePopupResult.empty())
            Label(fmt::format("Popup selected: {}", _inlinePopupResult).c_str());

        Space(8);
        Label("BeginScrollPanel  (shown inside the Demo Modal above).");
        Label("Open the modal to see a scrollable content area.");

        Space(8);
        Label("Use BeginWindowBox + widgets to build custom dialogs.");
    }

    // -----------------------------------------------------------------------
    // Tab: Widgets - SevenSegmentDisplay
    // -----------------------------------------------------------------------

    void drawWidgetsTab()
    {
        _segTimer += frameTime();
        if (_segTimer >= 0.25f) {
            _segTimer = 0.0f;
            ++_segValue;
        }

        SetSpacing(6);
        BeginColumns();
        SetNextRelativeWidth(0.5f);
        Begin();
        Label("SevenSegmentDisplay  (auto-incrementing)");
        Space(2);

        Label("Green LED \xe2\x80\x94 decimal:");
        {
            StyleManager::Scope led;
            led.setStyle(Style::TEXT_COLOR_NORMAL,   Color{0x22, 0xff, 0x22, 0xff});
            led.setStyle(Style::TEXT_COLOR_DISABLED, Color{0x04, 0x22, 0x04, 0xff});
            SevenSegmentDisplay(20, _segValue, 4);
        }

        Label("Amber LED \xe2\x80\x94 hex:");
        {
            StyleManager::Scope led;
            led.setStyle(Style::TEXT_COLOR_NORMAL,   Color{0xff, 0x88, 0x00, 0xff});
            led.setStyle(Style::TEXT_COLOR_DISABLED, Color{0x22, 0x10, 0x00, 0xff});
            SevenSegmentDisplay(20, _segValue, 4, 0, true);
        }

        Label("Red LED \xe2\x80\x94 decimal, dp at position 2 (\"XX.XX\"):");
        {
            StyleManager::Scope led;
            led.setStyle(Style::TEXT_COLOR_NORMAL,   Color{0xff, 0x22, 0x00, 0xff});
            led.setStyle(Style::TEXT_COLOR_DISABLED, Color{0x28, 0x04, 0x00, 0xff});
            SevenSegmentDisplay(20, _segValue, 4, 2);
        }

        Label("Red LED \xe2\x80\x94 height 30, 6 hex digits:");
        {
            StyleManager::Scope led;
            led.setStyle(Style::TEXT_COLOR_NORMAL,   Color{0xff, 0x22, 0x00, 0xff});
            led.setStyle(Style::TEXT_COLOR_DISABLED, Color{0x28, 0x04, 0x00, 0xff});
            SevenSegmentDisplay(30, _segValue, 6, 0, true);
        }
        End();

        Begin();
        Label("Til311Display  (only ever made in red, auto-incrementing)");
        Space(2);

        Label("dotSize=2, hex:");
        {
            StyleManager::Scope led;
            led.setStyle(Style::TEXT_COLOR_NORMAL,   Color{0xff, 0x42, 0x20, 0xff});
            led.setStyle(Style::TEXT_COLOR_DISABLED, Color{0x28, 0x04, 0x00, 0xff});
            Til311Display(1, _segValue, 4, 0, true);
        }

        Label("dotSize=2, decimal:");
        {
            StyleManager::Scope led;
            led.setStyle(Style::TEXT_COLOR_NORMAL,   Color{0xff, 0x42, 0x20, 0xff});
            led.setStyle(Style::TEXT_COLOR_DISABLED, Color{0x28, 0x04, 0x00, 0xff});
            Til311Display(2, _segValue, 4);
        }

        Label("dotSize=2, dp at position 2 (\"XX.XX\"):");
        {
            StyleManager::Scope led;
            led.setStyle(Style::TEXT_COLOR_NORMAL,   Color{0xff, 0x42, 0x20, 0xff});
            led.setStyle(Style::TEXT_COLOR_DISABLED, Color{0x28, 0x04, 0x00, 0xff});
            Til311Display(2, _segValue, 4, 2, true);
        }

        Label("dotSize=3, 6 hex digits:");
        {
            StyleManager::Scope led;
            led.setStyle(Style::TEXT_COLOR_NORMAL,   Color{0xff, 0x42, 0x20, 0xff});
            led.setStyle(Style::TEXT_COLOR_DISABLED, Color{0x28, 0x04, 0x00, 0xff});
            Til311Display(3, _segValue, 6, 0, true);
        }
        End();
        EndColumns();
    }

    // -----------------------------------------------------------------------
    // Tab: Dialogs - ModalFileDialog in all four variants
    // -----------------------------------------------------------------------

    void drawDialogsTab()
    {
        SetSpacing(4);
        Label("ModalFileDialog — open one of the four dialog types:");
        Space(4);

        BeginColumns();
        SetSpacing(2);
        SetNextRelativeWidth(0.25f);
        if (Button("Open File")) {
            _fileDlg = {FileDialogInfo::OpenFile, "Open File", _fileDlg.path};
            _fileDlgOpen = true;
        }
        SetTooltip("fdi.selection holds the selected file path on OK");
        SetNextRelativeWidth(0.25f);
        if (Button("Save File")) {
            _fileDlg = {FileDialogInfo::SaveFile, "Save File As...", _fileDlg.path};
            _fileDlgOpen = true;
        }
        SetTooltip("fdi.path + fdi.filename give the full target path on OK");
        SetNextRelativeWidth(0.25f);
        if (Button("Select Folder")) {
            _fileDlg = {FileDialogInfo::SelectFolder, "Select Folder", _fileDlg.path};
            _fileDlgOpen = true;
        }
        SetTooltip("fdi.path is set to the chosen folder on OK");
        if (Button("Multi-Select")) {
            _fileDlg = {FileDialogInfo::SelectMultipleFiles, "Select Multiple Files", _fileDlg.path};
            _fileDlgOpen = true;
        }
        SetTooltip("fdi.selection holds all chosen file paths on OK");
        EndColumns();

        Space(6);
        if (!_lastDialogResult.empty()) {
            Label("Last result:");
            Label(_lastDialogResult.c_str());
        }
        else {
            Label("(no dialog opened yet)");
        }

        Space(6);
        Separator();
        Space(4);
        Label("Re-opening a dialog remembers the last-visited directory.");
        Label("Always call ModalFileDialog() outside any BeginPanel/End block");
        Label("so it renders on top of the rest of the UI.");
    }

    // -----------------------------------------------------------------------
    // File dialog modal (drawn after EndPanel so it appears on top)
    // -----------------------------------------------------------------------

    void drawFileDialog()
    {
        if (!_fileDlgOpen)
            return;
        if (ModalFileDialog(_fileDlg, &_fileDlgOpen)) {
            _fileDlgOpen = false;
            if (_fileDlg.action == FileDialogInfo::CloseAction::Okay) {
                switch (_fileDlg.type) {
                    case FileDialogInfo::SelectFolder:
                        _lastDialogResult = fmt::format("Folder: {}", _fileDlg.path);
                        break;
                    case FileDialogInfo::SaveFile:
                        _lastDialogResult = fmt::format("Save to: {}",
                            (fs::path(_fileDlg.path) / _fileDlg.filename).string());
                        break;
                    default:
                        if (_fileDlg.selection.size() == 1)
                            _lastDialogResult = fmt::format("File: {}",
                                fs::path(_fileDlg.selection.front()).filename().string());
                        else if (!_fileDlg.selection.empty())
                            _lastDialogResult = fmt::format("{} files selected",
                                _fileDlg.selection.size());
                        break;
                }
            }
            else {
                _lastDialogResult = "Cancelled";
            }
        }
    }

    // -----------------------------------------------------------------------
    // Floating modals (drawn after the main panel so they appear on top)
    // -----------------------------------------------------------------------

    void drawAboutModal()
    {
        if (!BeginWindowBox({-1, -1, 420, 260}, "About rlGuipp Showcase", &_aboutOpen, WindowBoxFlags(WBF_CLOSABLE | WBF_MOVABLE | WBF_MODAL)))
            return;

        SetStyle(DEFAULT, BORDER_WIDTH, 0);
        BeginScrollPanel(-1, {0, 0, 400, 340}, &_aboutScroll);
        SetRowHeight(10);
        SetSpacing(2);
        Label("rlGuipp Showcase");
        Space(4);
        Label(fmt::format("rlGuipp version: {}", RLGUIPP_VERSION_STRING).c_str());
        Space(6);
        Label("rlGuipp is an immediate-mode GUI layout wrapper around raygui that");
        Label("adds hierarchical container primitives, popup menus, modals, and");
        Label("more - while staying within raygui's pure immediate-mode model.");
        Space(6);
        Label("This showcase covers:");
        SetIndent(8);
        Label("Layout   - BeginColumns, BeginPanel, BeginGroupBox, SetIndent");
        Label("Controls - Button, Toggle, ToggleGroup, CheckBox, ButtonMulti");
        Label("Input    - TextBox, Spinner, ValueBox, ComboBox, DropdownBox");
        Label("Sliders  - Slider, SliderBar, ProgressBar, ColorPicker");
        Label("Lists    - ListView, SimpleListView, BeginTableView");
        Label("Windows  - BeginWindowBox, BeginPopup, BeginScrollPanel");
        Label("Dialogs  - ModalFileDialog (Open, Save, Folder, Multi-Select)");
        Label("Widgets  - SevenSegmentDisplay (hex/dec, decimal point)");
        SetIndent(0);
        EndScrollPanel();
        SetStyle(DEFAULT, BORDER_WIDTH, 1);

        EndWindowBox();
        if (IsKeyPressed(KEY_ESCAPE))
            _aboutOpen = false;
    }

    void drawDemoModal()
    {
        if (!BeginWindowBox({-1, -1, 380, 280}, "Demo Modal - BeginScrollPanel inside", &_demoModalOpen, WindowBoxFlags(WBF_CLOSABLE | WBF_MOVABLE | WBF_MODAL)))
            return;

        SetStyle(DEFAULT, BORDER_WIDTH, 0);
        BeginScrollPanel(-1, {0, 0, 360, 520}, &_demoModalScroll);
        SetRowHeight(10);
        SetSpacing(3);
        Label("This content is inside a BeginScrollPanel.");
        Label("Scroll down to see more rows.");
        Space(4);
        for (int i = 1; i <= 40; ++i)
            Label(fmt::format("Scrollable row {:02d} - lorem ipsum dolor sit amet.", i).c_str());
        EndScrollPanel();
        SetStyle(DEFAULT, BORDER_WIDTH, 1);

        EndWindowBox();
        if (IsKeyPressed(KEY_ESCAPE))
            _demoModalOpen = false;
    }

    float frameTime() const { return _goldenMode ? 0.0f : GetFrameTime(); }

public:
    // -----------------------------------------------------------------------
    // Golden image tests
    // -----------------------------------------------------------------------

    int runGoldenTests(const fs::path& approvedDir, const fs::path& outDir, bool updateApproved)
    {
        fs::create_directories(outDir);
        if (updateApproved)
            fs::create_directories(approvedDir);
        createFileDialogFixture(outDir / "fixture");
        // the file dialogs show the relative path "fixture", so it does not depend on the build directory
        fs::current_path(outDir);

        struct GoldenCase
        {
            const char* name;
            int tab;
            int theme;
            std::function<void()> setup;
        };
        enum Tabs { LAYOUT, CONTROLS, INPUT, SLIDERS, LISTS, WINDOWS, DIALOGS, WIDGETS };
        auto openFileDialog = [this](FileDialogInfo::DialogType type, const char* title) {
            _fileDlg = {type, title, "fixture"};
            _fileDlgOpen = true;
        };
        const std::vector<GoldenCase> cases = {
            {"tab-layout", LAYOUT, 0, {}},
            {"tab-controls", CONTROLS, 0, {}},
            {"tab-input", INPUT, 0, {}},
            {"tab-sliders", SLIDERS, 0, {}},
            {"tab-lists", LISTS, 0, {}},
            {"tab-windows", WINDOWS, 0, {}},
            {"tab-dialogs", DIALOGS, 0, {}},
            {"tab-widgets", WIDGETS, 0, {}},
            {"theme-warm-controls", CONTROLS, 1, {}},
            {"theme-light-layout", LAYOUT, 2, {}},
            {"main-menu", LAYOUT, 0, [this]() { PopupMenu::Open({1.0f, 20.0f}, _mainMenu); }},
            // the File menu is the first title of the bar, so it opens at the bar's left edge
            {"menu-bar-file", LAYOUT, 0, [this]() { PopupMenu::Open({_menuBarRect.x, _menuBarRect.y + _menuBarRect.height}, _menuBar[0].menu); }},
            {"no-status-bar", LAYOUT, 0, [this]() { _showStatusBar = false; }},
            {"about-modal", LAYOUT, 0, [this]() { _aboutOpen = true; }},
            {"demo-modal", WINDOWS, 0, [this]() { _demoModalOpen = true; }},
            {"inline-popup", WINDOWS, 0, [this]() { _inlinePopupOpen = true; }},
            {"file-dialog-open", DIALOGS, 0, [=]() { openFileDialog(FileDialogInfo::OpenFile, "Open File"); }},
            {"file-dialog-save", DIALOGS, 0, [=]() { openFileDialog(FileDialogInfo::SaveFile, "Save File As..."); }},
            {"file-dialog-folder", DIALOGS, 0, [=]() { openFileDialog(FileDialogInfo::SelectFolder, "Select Folder"); }},
        };

        int failed = 0;
        for (const auto& gc : cases) {
            _activeTab = gc.tab;
            _themeIdx = gc.theme;
            _styleManager.setTheme(gc.theme);
            _aboutOpen = _demoModalOpen = _inlinePopupOpen = _fileDlgOpen = false;
            _showStatusBar = true;
            _aboutScroll = _demoModalScroll = {};
            PopupMenu::Close();
            // one frame to close what the previous case opened, then a few frames to settle
            // deferred widgets (dropdowns, popups, the file dialog reads its directory in its first frame)
            update();
            if (gc.setup)
                gc.setup();
            for (int i = 0; i < 4; ++i)
                update();
            auto actual = (outDir / (std::string(gc.name) + ".png")).string();
            auto approved = (approvedDir / (std::string(gc.name) + ".png")).string();
            exportFrame(actual.c_str());
            if (updateApproved) {
                fs::copy_file(actual, approved, fs::copy_options::overwrite_existing);
                std::printf("updated %s\n", gc.name);
                continue;
            }
            auto diff = (outDir / (std::string(gc.name) + "-diff.png")).string();
            auto result = compareImages(actual, approved, diff);
            if (result.empty()) {
                std::printf("ok      %s\n", gc.name);
            }
            else {
                std::printf("FAILED  %s: %s\n", gc.name, result.c_str());
                ++failed;
            }
        }
        if (!updateApproved)
            std::printf("%d of %d golden image tests failed\n", failed, (int)cases.size());
        return failed ? 1 : 0;
    }

private:
    // A directory with fixed content and times for the file dialog cases
    static void createFileDialogFixture(const fs::path& dir)
    {
        fs::remove_all(dir);
        fs::create_directories(dir / "games");
        auto writeFile = [&dir](const char* name, size_t size) {
            std::string content(size, 'x');
            if (auto* f = std::fopen((dir / name).string().c_str(), "wb")) {
                std::fwrite(content.data(), 1, content.size(), f);
                std::fclose(f);
            }
        };
        writeFile("Breakout.ch8", 280);
        writeFile("notes.txt", 1234);
        writeFile("screenshot.png", 20480);
        // a local time, so the dialog shows the same time in every time zone
        std::tm tm{};
        tm.tm_year = 2024 - 1900;
        tm.tm_mon = 2;
        tm.tm_mday = 15;
        tm.tm_hour = 12;
        tm.tm_min = 34;
        tm.tm_sec = 56;
        tm.tm_isdst = -1;
        auto fixedTime = std::chrono::system_clock::from_time_t(std::mktime(&tm));
        for (const auto& entry : fs::directory_iterator(dir)) {
            fs::last_write_time(entry.path(), fixedTime);
        }
    }

    // Returns an empty string if both images are equal, otherwise what differs; then a diff image is written
    static std::string compareImages(const std::string& actualPath, const std::string& approvedPath, const std::string& diffPath)
    {
        if (!fs::exists(approvedPath))
            return "no approved image, run with --update to create it";
        Image actual = LoadImage(actualPath.c_str());
        Image approved = LoadImage(approvedPath.c_str());
        std::string result;
        if (actual.width != approved.width || actual.height != approved.height) {
            result = fmt::format("size {}x{} instead of {}x{}", actual.width, actual.height, approved.width, approved.height);
        }
        else {
            Color* a = LoadImageColors(actual);
            Color* b = LoadImageColors(approved);
            std::vector<Color> diff(actual.width * actual.height);
            int differences = 0;
            for (size_t i = 0; i < diff.size(); ++i) {
                if (a[i].r != b[i].r || a[i].g != b[i].g || a[i].b != b[i].b || a[i].a != b[i].a) {
                    diff[i] = Color{255, 0, 0, 255};
                    ++differences;
                }
                else {
                    auto gray = static_cast<unsigned char>((b[i].r + b[i].g + b[i].b) / 9);
                    diff[i] = Color{gray, gray, gray, 255};
                }
            }
            if (differences) {
                Image diffImage{diff.data(), actual.width, actual.height, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
                ExportImage(diffImage, diffPath.c_str());
                result = fmt::format("{} pixels differ, see {}", differences, diffPath);
            }
            UnloadImageColors(a);
            UnloadImageColors(b);
        }
        UnloadImage(actual);
        UnloadImage(approved);
        return result;
    }

    // -----------------------------------------------------------------------
    // State
    // -----------------------------------------------------------------------

    bool _goldenMode{false};
    int _activeTab{0};
    StyleManager _styleManager;

    // Menu
    PopupMenu::Menu          _mainMenu;
    PopupMenu::Bar           _menuBar;
    Rectangle                _menuBarRect{};
    bool                     _showStatusBar{true};
    std::vector<std::string> _themeNames;
    int _themeIdx{0};

    // Modals
    bool    _aboutOpen{false};
    Vector2 _aboutScroll{};
    bool    _demoModalOpen{false};
    Vector2 _demoModalScroll{};

    // Controls tab
    bool        _toggleState{false};
    int         _toggleGroupState{0};
    bool        _checkbox1{false};
    bool        _checkbox2{true};
    std::string _lastClickedButton;

    // Input tab
    char        _charBuf[256]{"Edit me (char array)"};
    std::string _stringBuf{"Edit me (std::string)"};
    int         _spinnerVal{42};
    int         _valueBoxVal{100};
    int         _comboActive{0};
    int         _dropdownActive{0};
    int         _dropdownUpActive{0};

    // Sliders tab
    float   _sliderA{0.5f};
    float   _sliderB{25.0f};
    float   _sliderBar{0.3f};
    float   _sliderBar2{0.0f};
    float   _progress{0.0f};
    Color   _pickedColor{100, 200, 100, 255};
    float   _colorAlpha{1.0f};

    // Lists tab
    int     _listScrollIdx{0};
    int     _listActive{0};
    Vector2 _simpleListScroll{};
    int     _simpleListActive{-1};
    Vector2 _tableScroll{};

    // Windows tab
    bool        _inlinePopupOpen{false};
    std::string _inlinePopupResult;

    // Dialogs tab
    bool            _fileDlgOpen{false};
    FileDialogInfo  _fileDlg{FileDialogInfo::OpenFile, "Open File", "."};
    std::string     _lastDialogResult;

    // Widgets tab
    float   _segTimer{0.0f};
    int64_t _segValue{0};
};

int main(int argc, char* argv[])
{
    std::optional<fs::path> approvedDir;
    fs::path outDir = "golden-out";
    bool update = false;
    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "--golden" && i + 1 < argc)
            approvedDir = fs::absolute(argv[++i]);
        else if (arg == "--out" && i + 1 < argc)
            outDir = argv[++i];
        else if (arg == "--update")
            update = true;
        else {
            std::fprintf(stderr, "usage: %s [--golden <approved-dir> [--out <output-dir>] [--update]]\n", argv[0]);
            return 2;
        }
    }
    if (approvedDir) {
        SetTraceLogLevel(LOG_WARNING);
        ShowcaseApp app(true);
        return app.runGoldenTests(*approvedDir, fs::absolute(outDir), update);
    }
    ShowcaseApp app;
    while (!app.shouldClose())
        app.update();
    return 0;
}
