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
//---------------------------------------------------------------------------------------

#include <rlguipp/rlguiapp.hpp>
#include <rlguipp/rlguipp.hpp>
#include <rlguipp/rlguipp_filedialog.hpp>
#include <rlguipp/rlguipp_leds.hpp>
#include <stylemanager.hpp>
#include <fmt/format.h>
#include <ghc/fs_fwd.hpp>
#include <cmath>
#include <optional>
#include <string>
#include <vector>

namespace fs = ghc::filesystem;

using namespace gui;

class ShowcaseApp : public gui::Application {
public:
    ShowcaseApp() : gui::Application("rlGuipp Showcase", {800, 600, 2})
    {
        _styleManager.addTheme("Warm",  30.0f, 90, false);
        _styleManager.addTheme("Dark", 200.0f, 90, true);
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
        _themeNames = {"Default", "Warm", "Dark"};

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
            PopupMenu::Submenu("Theme", std::move(themeMenu)),
            PopupMenu::Separator(),
            PopupMenu::Action("Quit", [this]() { requestClose(); }, "⌘Q"),
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
        StatusBar({
            {0.5f,  "rlGuipp Showcase v" RLGUIPP_VERSION_STRING},
            {0.25f, fmt::format("{:.0f} FPS", (float)GetFPS()).c_str()},
            {0.25f, fmt::format("Theme: {}", _themeNames[_themeIdx]).c_str()},
        });

        // Toolbar - burger menu left, title label fills remaining space.
        SetRowHeight(20);
        SetSpacing(0);
        BeginColumns();
        {
            if (iconButton(ICON_BURGER_MENU, PopupMenu::IsMenuOpen(_mainMenu)) && !PopupMenu::IsOpen())
                PopupMenu::Open({1.0f, GetCurrentPos().y + 20}, _mainMenu);
            SetTooltip("Main menu  (About is in the menu)");
            SetNextWidth(GetContentAvailable().width);
            Label("rlGuipp Showcase");
        }
        EndColumns();

        // Main content panel with tab view.
        //Begin();
        BeginPanel("rlGuipp Feature Showcase", {4, 4});
        {
            static int activeTab = 0;
            SetSpacing(0);
            BeginTabView(&activeTab);
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
        TextBox(_charBuf, sizeof(_charBuf));
        SetTooltip("Click to edit - char array");

        Label("TextBox (std::string variant):");
        TextBox(_stringBuf, 256);
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
                Spinner("", &_spinnerVal, 0, 100);

                Space(4);
                Label("ValueBox:");
                ValueBox("", &_valueBoxVal, 0, 999);

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
        _progress = std::fmod(_progress + GetFrameTime() * 0.25f, 1.0f);
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
                SimpleListView(halfH, items, &_simpleListScroll, &_simpleListActive);
            }
            End();

            // Right: table view with header + data rows
            Begin();
            {
                SetSpacing(4);
                Label("BeginTableView (header + 20 data rows, scrollable):");
                BeginTableView(area.height - 32, 3, &_tableScroll);

                // Header row with tinted background
                Color headerBg = GetColor(GetStyle(DEFAULT, BASE_COLOR_FOCUSED));
                if (TableNextRow(16, headerBg)) {
                    if (TableNextColumn(120)) Label("Name");
                    if (TableNextColumn(80))  Label("Value");
                    if (TableNextColumn(80))  Label("Status");
                }

                // Data rows
                static const char* statuses[] = {"OK", "Warn", "Error"};
                for (int i = 0; i < 20; ++i) {
                    if (!TableNextRow(16)) continue;
                    if (TableNextColumn(120)) Label(fmt::format("Row {:02d}", i + 1).c_str());
                    if (TableNextColumn(80))  Label(fmt::format("{}", i * 7 + 3).c_str());
                    if (TableNextColumn(80))  Label(statuses[i % 3]);
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
        if (BeginPopup({anchor.x, anchor.y + anchor.height + 2, 120, 56}, &_inlinePopupOpen)) {
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
        _segTimer += GetFrameTime();
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

    // -----------------------------------------------------------------------
    // State
    // -----------------------------------------------------------------------

    StyleManager _styleManager;

    // Menu
    PopupMenu::Menu          _mainMenu;
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

int main()
{
    ShowcaseApp app;
    while (!app.shouldClose())
        app.update();
    return 0;
}
