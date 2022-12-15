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
// Supported controls:
//      Space
//      Label
//      Button
//      LabelButton
//      Toggle
//      ToggleGroup
//      CheckBox
//      ComboBox
//      DropdownBox
//      Spinner
//      ValueBox
//      TextBox
//      TextBoxMulti
//      Slider
//      SliderBar
//      ProgressBar
//      StatusBar
//      Grid
//      ListView
//      ListViewEx
//      MessageBox
//      TextInputBox
//      ColorPicker
//      ColorPanel
//      ColorBarAlpha
//      ColorBarHue
//---------------------------------------------------------------------------------------

#ifndef RLGUIPP_HPP
#define RLGUIPP_HPP

// rlGuipp version in decimal (major * 10000 + minor * 100 + patch)
#define RLGUIPP_VERSION 10000L
#define RLGUIPP_VERSION_STRING "0.1.0"

#ifdef RLGUIPP_IMPLEMENTATION
#define RAYGUI_IMPLEMENTATION
#endif

#define RLGUIPP_API

extern "C" {

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wnarrowing"
#pragma GCC diagnostic ignored "-Wc++11-narrowing"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wimplicit-int-float-conversion"
#pragma GCC diagnostic ignored "-Wenum-compare"
#pragma GCC diagnostic ignored "-Wunused-result"
#if __clang__
#pragma GCC diagnostic ignored "-Wenum-compare-conditional"
#endif
#endif  // __GNUC__

#include "raygui.h"

#pragma GCC diagnostic pop
}

#include <cstring>
#include <initializer_list>
#include <string>

namespace gui {

inline const auto DEFAULT_ROW_HEIGHT = 26.0f;

enum WindowBoxFlags { WBF_NONE = 0, WBF_CLOSABLE = 1, WBF_MOVABLE = 2, WBF_MODAL = 4 };

RLGUIPP_API void BeginGui(Rectangle area = {}, RenderTexture* renderTexture = nullptr, Vector2 mouseOffset = {}, Vector2 guiScale = {1.0f, 1.0f}); // Start the work on a GUI, needs to be closed with EndGui(), renderTexture is needed if rendering to a texture instead of the screen
RLGUIPP_API void EndGui();                                                                                                // ends the GUI description
RLGUIPP_API void UnloadGui();                                                                                             // unload any stuff cached by the gui handling
RLGUIPP_API void Begin();                                                                                                 // start of a hierarchical group of elements, must be closed with End()
RLGUIPP_API void End();                                                                                                   // end of the group
RLGUIPP_API void BeginColumns();                                                                                          // start of a list of columns of elements, must be closed with EndColumns()
RLGUIPP_API void EndColumns();                                                                                            // end the column grouping
RLGUIPP_API void BeginPanel(const char* text = nullptr, Vector2 padding = {5, 5});                                        // start a panel (a group with a kind of title bar, if title is given), must be closed with EndPanel()
RLGUIPP_API void EndPanel();                                                                                              // end the description of a panel group
RLGUIPP_API void BeginTabView(int *activeTab);                                                                            // start a tab view (a stack of groups with labeled tabs on top), must be closed with EndTabView()
RLGUIPP_API void EndTabView();                                                                                            // end the description of a tab view
RLGUIPP_API bool BeginTab(const char* text, Vector2 padding = {5, 5});                                                    // start a tab in a tab view (a page with the given text as label top), must be closed with EndTab()
RLGUIPP_API void EndTab();                                                                                                // end the description of a Tab group
RLGUIPP_API void BeginScrollPanel(float height, Rectangle content, Vector2* scroll);                                      // start a scrollable panel with the given content size (pos is ignored), and scrolled to offset scroll
RLGUIPP_API void EndScrollPanel();                                                                                        // end the description of the scroll panel
RLGUIPP_API void BeginTableView(float height, int numColumns);                                                            //
RLGUIPP_API void TableNextRow(float height, Color background = {0, 0, 0, 0});                                             //
RLGUIPP_API bool TableNextColumn();                                                                                       //
RLGUIPP_API void EndTableView();                                                                                          //
RLGUIPP_API void BeginGroupBox(const char* text = nullptr);                                                               // start a group box, similar to panel but no title bar, title is instead in a gap of the border, must be closed with EndGroupBox()
RLGUIPP_API void EndGroupBox();                                                                                           // end the description of a group box
RLGUIPP_API void BeginPopup(Rectangle area, bool* isOpen);                                                                // show a popup at the given area, open as long as `*isOpen` is true
RLGUIPP_API void EndPopup();                                                                                              // end of the popup description
RLGUIPP_API bool BeginWindowBox(Rectangle area, const char* title, bool* isOpen, WindowBoxFlags flags = WBF_NONE);        // same as a popup, but with a title bar and a close button, optionally draggable
RLGUIPP_API void EndWindowBox();                                                                                          // end of the WindowBox
RLGUIPP_API void SetState(int state);                                                                                     // same as raygui GuiSetState
RLGUIPP_API int GetState();                                                                                               // same as raygui GuiGetState
RLGUIPP_API void SetStyle(int control, int property, int value);                                                          // Set one style property
RLGUIPP_API int GetStyle(int control, int property);                                                                      // Get one style property
RLGUIPP_API void SetIndent(float width);                                                                                  // Indents all following elements in this level
RLGUIPP_API void SetReserve(float width);                                                                                 // Sets the space reserved for right side labels of bar widgets
RLGUIPP_API void SetNextWidth(float width);                                                                               // Set the width of the next element, default is the width of the parent
RLGUIPP_API void SetRowHeight(float height);                                                                              // Set the height for elements that can be typically in a row, like buttons, edit fields, spinner...
RLGUIPP_API void SetSpacing(float spacing);                                                                               // Set the spacing, depending on the parent layout it will set the horizontal or vertical spacing
RLGUIPP_API Vector2 GetCurrentPos();                                                                                      // Get the position that will be used by the next widget
RLGUIPP_API Rectangle GetContentAvailable();                                                                              // Get the area from current position to content edges
RLGUIPP_API Rectangle GetLastWidgetRect();                                                                                // Get the position and dimensions of the previous widget
RLGUIPP_API void SetTooltip(const std::string& tooltip);                                                                  // Set a tooltip for the rectangle of the previously defined widget
RLGUIPP_API void Space(float size = -1);                                                                                  // Depending on the parent it inserts a vertical or horizontal space, defaulting to the spacing
RLGUIPP_API void Separator(float size = -1);                                                                              // Insert a seperator line
RLGUIPP_API void Label(const char* text);                                                                                 // Label control, shows text
RLGUIPP_API bool Button(const char* text);                                                                                // Button control, returns true when clicked
RLGUIPP_API bool LabelButton(const char* text);                                                                           // Label button control, show true when clicked
RLGUIPP_API bool Toggle(const char* text, bool active);                                                                   // Toggle Button control, returns true when active
RLGUIPP_API int ToggleGroup(const char* text, int active);                                                                // Toggle Group control, returns active toggle index
RLGUIPP_API bool CheckBox(const char* text, bool checked);                                                                // Check Box control, returns true when active
RLGUIPP_API int ComboBox(const char* text, int active);                                                                   // Combo Box control, returns selected item index
RLGUIPP_API bool DropdownBox(const char* text, int* active);                                                              // Dropdown Box control, returns selected item
RLGUIPP_API bool Spinner(const char* text, int* value, int minValue, int maxValue, bool editMode);                        // Spinner control, returns selected value
RLGUIPP_API bool ValueBox(const char* text, int* value, int minValue, int maxValue, bool editMode);                       // Value Box control, updates input text with numbers
RLGUIPP_API void SetKeyboardFocus(void* key);                                                                             // Claim keyboard focus and set focus key to `key`
RLGUIPP_API bool HasKeyboardFocus(void* key);                                                                             // Check if key is current key for keyboard focus
RLGUIPP_API bool TextBox(char* text, int textSize);                                                                       // Text Box control, updates input text
RLGUIPP_API bool TextBox(std::string& text, int textSize);                                                                // same for std::string
RLGUIPP_API bool TextBoxMulti(char* text, int textSize, bool editMode);                                                   // Text Box control with multiple lines
RLGUIPP_API float Slider(const char* textLeft, const char* textRight, float value, float minValue, float maxValue);       // Slider control, returns selected value
RLGUIPP_API float Slider(const char* textLeft, float value, float minValue, float maxValue);                              // Slider control with default "%.2f" formatting, returns selected value
RLGUIPP_API float SliderBar(const char* textLeft, const char* textRight, float value, float minValue, float maxValue);    // Slider Bar control, returns selected value
RLGUIPP_API float SliderBar(const char* textLeft, float value, float minValue, float maxValue);                           // Slider Bar control with default "%.2f" formatting, returns selected value
RLGUIPP_API float ProgressBar(const char* textLeft, const char* textRight, float value, float minValue, float maxValue);  // Progress Bar control, shows current progress value
RLGUIPP_API void StatusBar(const char* text);                                                                             // Status Bar control, shows info text
RLGUIPP_API void StatusBar(std::initializer_list<std::pair<float, const char*>> fields);                                  // Status Bar control, shows multiple fields
RLGUIPP_API Vector2 Grid(float height, float spacing, int subdivs);                                                       // Grid control, returns mouse cell position
// Advanced controls set
RLGUIPP_API int ListView(float height, const char* text, int* scrollIndex, int active);                                                         // List View control, returns selected list item index
RLGUIPP_API int ListViewEx(float height, const char** text, int count, int* focus, int* scrollIndex, int active);                               // List View with extended parameters
RLGUIPP_API int MessageBox(const char* title, const char* message, const char* buttons);                                                        // Message Box control, displays a message
RLGUIPP_API int TextInputBox(const char* title, const char* message, const char* buttons, char* text, int textMaxSize, int* secretViewActive);  // Text Input Box control, ask for text, supports secret
RLGUIPP_API Color ColorPicker(Color color);                                                                                                     // Color Picker control (multiple color controls)
RLGUIPP_API Color ColorPanel(const char* text, Color color);                                                                                    // Color Panel control
RLGUIPP_API float ColorBarAlpha(const char* text, float alpha);                                                                                 // Color Bar Alpha control
RLGUIPP_API float ColorBarHue(const char* text, float value);                                                                                   // Color Bar Hue control

RLGUIPP_API const char* IconText(int icon, const char* text);

RLGUIPP_API bool BeginMenuBar();
RLGUIPP_API void EndMenuBar();
RLGUIPP_API bool BeginMenu(const char* text);
RLGUIPP_API void EndMenu();
#define MENU_SHORTCUT(modifier, key) static_cast<uint32_t>(((modifier) << 16) | (key))
RLGUIPP_API bool MenuItem(const char* text, uint32_t shortcut = 0, bool* selected = nullptr);
RLGUIPP_API int BeginPopupMenu(Vector2 position, const char* items);  // A popup menu with the items being given in raygui way seperated
RLGUIPP_API void EndPopupMenu();

}  // namespace gui

#endif  // RLGUIPP_HPP

//-----------------------------------------------------------------------------
// IMPLEMENTATION
//-----------------------------------------------------------------------------
#ifdef RLGUIPP_IMPLEMENTATION

#include <algorithm>
#include <stack>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace gui {

struct PopupContext
{
    explicit PopupContext(Rectangle rect, bool* isOpen);
    PopupContext(const PopupContext&) = delete;
    PopupContext(PopupContext&& other) noexcept
        : _level(other._level)
        , _position(other._position)
        , _content(other._content)
        , _lastUpdate(other._lastUpdate)
        , _isOpen(other._isOpen)
    {
        other._content = RenderTexture{0};
    }
    ~PopupContext() { UnloadRenderTexture(_content); }
    int level() const { return _level; }
    Vector2 position() const { return {std::round(_position.x), std::round(_position.y)}; }
    Rectangle bounds() const { return {std::round(_position.x), std::round(_position.y), (float)_content.texture.width, (float)_content.texture.height}; }
    RenderTexture& texture() { return _content; }
    void move(float dx, float dy) { _position.x += dx, _position.y += dy; }
    void render() const
    {
        if (*_isOpen) {
            DrawRectangle(position().x + 4, position().y + 4, _content.texture.width, _content.texture.height, {0, 0, 0, 96});
            DrawTextureRec(_content.texture, {0, 0, (float)_content.texture.width, -(float)_content.texture.height}, position(), ::WHITE);
        }
    }
    PopupContext& operator=(const PopupContext&) = delete;
    PopupContext& operator=(PopupContext&& other) noexcept
    {
        _level = other._level;
        _content = other._content;
        other._content = RenderTexture{0};
        _lastUpdate = other._lastUpdate;
        _isOpen = other._isOpen;
        return *this;
    }

    static void renderPopups();
    static void cleanupPoups();
    static PopupContext* find(bool* isOpen);

    int _level{0};
    Vector2 _position{0, 0};
    RenderTexture _content{};
    int64_t _lastUpdate{0};
    WindowBoxFlags _flags{WBF_NONE};
    bool* _isOpen;
};

struct TabViewContext
{
    int *activeTab{nullptr};
    int currentTab{0};
    float tabOffset{0.0f};
    static TabViewContext& getContext(int* activeTab);
};

struct ScrollPanelContext
{
    Rectangle area{};
    Vector2* scroll{nullptr};
};

struct MenuBar
{
    Rectangle area;
    std::vector<uint64_t> _leftMenus;
    static MenuBar& getMenuBar(void* id);
};

struct MenuContext
{
    Rectangle area;
    bool isOpen{false};
    float height{0};
    float maxWidth{0};
    static MenuContext& getContext(const char* text);
};

struct TableContext
{
    int numColumns{0};
    bool hasMeasured{false};
    bool lockedGui{false};
    std::vector<float> columnWidth;
    float curX{0};
    float curY{0};
    float curWidth{0};
    float curHeight{0};
    float curRowHeight{0};
    float curColumnWidth{0};
    int curRow{0};
    int curColumn{0};
    Vector2* scroll{nullptr};
    static TableContext& getContext(Vector2* scroll);
};

struct GuiContext
{
    enum Type { ctROOT, ctGROUP, ctCOLUMNS, ctTABVIEW, ctTAB, ctPOPUP, ctSCROLLPANEL, ctMENUBAR, ctMENU };
    using ContextData = std::variant<ScrollPanelContext*, RenderTexture*, MenuBar*, MenuContext*, TableContext*, TabViewContext*>;
    Type type;
    Vector2 initialPos{};
    Vector2 currentPos{};
    Rectangle area{};
    Rectangle content{};
    Rectangle lastWidgetRect{};
    bool horizontal{false};
    bool bordered{false};
    int level{0};
    float maxSize{0};
    float rowHeight{DEFAULT_ROW_HEIGHT};
    float nextWidth{-1};
    float nextHeight{-1};
    float spacingH{15};
    float spacingV{4};
    float indent{0};
    float reserve{0};
    Vector2 padding{0, 0};
    Vector2 mouseOffset{0, 0};
    Vector2 scrollOffset{0, 0};
    std::string groupName;
    ContextData contextData;
    // RenderTexture* texture{nullptr};

    void increment(Vector2 size)
    {
        auto x = currentPos.x;
        auto y = currentPos.y;
        if (horizontal) {
            currentPos.x += size.x + spacingH;
            maxSize = std::max(size.y, maxSize);
        }
        else {
            currentPos.y += size.y + spacingV;
            maxSize = std::max(size.x, maxSize);
        }
        nextWidth = -1;
        nextHeight = -1;
        lastWidgetRect = {x, y, size.x, size.y};
    }
    void wrap()
    {
        if (horizontal) {
            currentPos.x = area.x;
            currentPos.y += maxSize;
        }
        else {
            currentPos.x += maxSize;
            currentPos.y = area.y;
        }
    }
    Vector2 standardSize(float height = -1) const { return {nextWidth > 0.0f ? nextWidth : content.width - currentPos.x + content.x, height > 0 ? height : (nextHeight > 0 ? nextHeight : rowHeight)}; }
    inline static Rectangle lastControlRect{};
    inline static int64_t frameId{0};  // Note: even at 1000fps this would have just overflown when started around the start of the permian (about 300Ma) ;-)
    inline static Vector2 guiScale{1.0f, 1.0f};
    struct DropdownInfo
    {
        Rectangle rect{};
        bool clicked{false};
        std::string text;
        bool editMode{false};
        int64_t lastUpdate{0};
        int64_t lastDraw{0};
        int state{0};
        unsigned int style[RAYGUI_MAX_PROPS_BASE + RAYGUI_MAX_PROPS_EXTENDED];
    };
    inline static std::unordered_map<int*, DropdownInfo> dropdownBoxes;
    inline static int* openDropdownboxId{nullptr};
    inline static void* editFocusId{nullptr};
    inline static GuiContext* rootContext{nullptr};
    inline static std::string tooltipText;
    inline static Rectangle tooltipParentRect{};
    inline static float tooltipTimer{};
    static bool deferDropdownBox(Rectangle rect, const char* text, int* active)
    {
        auto iter = dropdownBoxes.find(active);
        if (iter != dropdownBoxes.end()) {
            iter->second.lastUpdate = frameId;
            for (int i = 0; i < RAYGUI_MAX_PROPS_BASE + RAYGUI_MAX_PROPS_EXTENDED; ++i) {
                iter->second.style[i] = GetStyle(DROPDOWNBOX, i);
            }
            return iter->second.clicked;
        }
        else {
            iter = dropdownBoxes.emplace(active, DropdownInfo{rect, false, std::string(text), false, 0, 0, GetState()}).first;
            for (int i = 0; i < RAYGUI_MAX_PROPS_BASE + RAYGUI_MAX_PROPS_EXTENDED; ++i) {
                iter->second.style[i] = GetStyle(DROPDOWNBOX, i);
            }
            return false;
        }
    }
    static void closeOpenDropdownBox()
    {
        if (openDropdownboxId) {
            auto iter = dropdownBoxes.find(openDropdownboxId);
            if (iter != dropdownBoxes.end()) {
                iter->second.editMode = false;
                iter->second.clicked = false;
                openDropdownboxId = nullptr;
            }
        }
    }
    static void handleDeferredDropBoxes()
    {
        for (auto& [active, info] : dropdownBoxes) {
            if (info.lastDraw < info.lastUpdate && info.lastUpdate == frameId) {
                for (int i = 0; i < RAYGUI_MAX_PROPS_BASE + RAYGUI_MAX_PROPS_EXTENDED; ++i) {
                    SetStyle(DROPDOWNBOX, i, info.style[i]);
                }
                if (GuiDropdownBox(info.rect, info.text.c_str(), active, info.editMode)) {
                    if (openDropdownboxId != active) {
                        closeOpenDropdownBox();
                    }
                    info.clicked = info.editMode;
                    info.editMode = !info.editMode;
                    openDropdownboxId = info.editMode ? active : nullptr;
                }
                else {
                    info.clicked = false;
                }
                info.lastDraw = info.lastUpdate;
            }
        }
        if(!GuiContext::tooltipText.empty()) {
            auto move = GetMouseDelta();
            if(std::abs(move.x) > 0.01f || std::abs(move.y) > 0.01f) {
                GuiContext::tooltipTimer = 1.0f;
            }
            else {
                GuiContext::tooltipTimer -= GetFrameTime();
            }
            if(GuiContext::tooltipTimer <= 0.0f) {
                auto size = MeasureTextEx(GuiGetFont(), GuiContext::tooltipText.c_str(), 8, 0);
                Rectangle tipRect{
                    GuiContext::tooltipParentRect.x + GuiContext::tooltipParentRect.width/2 - size.x/2 - 3,
                    GuiContext::tooltipParentRect.y + GuiContext::tooltipParentRect.height*2/3,
                    size.x + 6, size.y + 6
                };
                DrawRectangle(tipRect.x, tipRect.y, tipRect.width, tipRect.height, {0,0,0,128});
                DrawTextEx(GuiGetFont(), GuiContext::tooltipText.c_str(), {tipRect.x + 3, tipRect.y + 3}, 8, 0, WHITE);
            }
        }
    }
};

inline static std::stack<GuiContext> g_contextStack;
inline static std::unordered_map<bool*, PopupContext> g_popupMap;
inline static std::unordered_map<Vector2*, ScrollPanelContext> g_scrollPanelMap;
inline static PopupContext* g_popupUnderMouse{nullptr};
inline static std::unordered_map<void*, MenuBar> g_menuBars;
inline static std::unordered_map<uint64_t, MenuContext> g_menuContextMap;
inline static std::unordered_map<Vector2*, TableContext> g_tableContextMap;
inline static std::unordered_map<int*,TabViewContext> g_tabviewContextMap;

PopupContext::PopupContext(Rectangle rect, bool* isOpen)
    : _level((int)g_contextStack.size())
    , _position{rect.x, rect.y}
    , _lastUpdate(GuiContext::frameId)
    , _isOpen(isOpen)
{
    _content = LoadRenderTexture((int)rect.width, (int)rect.height);
}

void PopupContext::renderPopups()
{
    for (auto iter = g_popupMap.begin(), last = g_popupMap.end(); iter != last;) {
        if (iter->second._lastUpdate < GuiContext::frameId) {
            // iter = g_popupMap.erase(iter);
            ++iter;
        }
        else {
            if (*iter->first) {
                if((iter->second._flags & WBF_MODAL) && GuiContext::rootContext)
                    DrawRectangle(GuiContext::rootContext->area.x, GuiContext::rootContext->area.y, GuiContext::rootContext->area.width, GuiContext::rootContext->area.height, {0,0,0,128});
                iter->second.render();
            }
            ++iter;
        }
    }
}

void PopupContext::cleanupPoups()
{
    for (auto iter = g_popupMap.begin(), last = g_popupMap.end(); iter != last;) {
        if (!*iter->first && iter->second._lastUpdate < GuiContext::frameId) {
            iter = g_popupMap.erase(iter);
        }
        else {
            ++iter;
        }
    }
}

PopupContext* PopupContext::find(bool* isOpen)
{
    auto iter = g_popupMap.find(isOpen);
    if (iter != g_popupMap.end()) {
        iter->second._lastUpdate = GuiContext::frameId;
        return &iter->second;
    }
    return nullptr;
}

namespace detail {
uint64_t fnv_64a_str(const char* str, uint64_t hval);
};

MenuBar& MenuBar::getMenuBar(void* id)
{
    auto iter = g_menuBars.find(id);
    if (iter == g_menuBars.end()) {
        iter = g_menuBars.emplace(std::make_pair(id, MenuBar())).first;
    }
    return iter->second;
}

MenuContext& MenuContext::getContext(const char* text)
{
    auto hash = detail::fnv_64a_str(text, 0xbeef);
    auto iter = g_menuContextMap.find(hash);
    if (iter == g_menuContextMap.end()) {
        iter = g_menuContextMap.emplace(std::make_pair(hash, MenuContext())).first;
    }
    return iter->second;
}

TableContext& TableContext::getContext(Vector2* scroll)
{
    auto iter = g_tableContextMap.find(scroll);
    if (iter == g_tableContextMap.end()) {
        iter = g_tableContextMap.emplace(std::make_pair(scroll, TableContext())).first;
    }
    return iter->second;
}

TabViewContext& TabViewContext::getContext(int* activeTab)
{
    auto iter = g_tabviewContextMap.find(activeTab);
    if(iter == g_tabviewContextMap.end()) {
        iter = g_tabviewContextMap.emplace(std::make_pair(activeTab, TabViewContext())).first;
    }
    return iter->second;
}

namespace detail {

//---------------------------------------------------------------------------
// fnv_64a_str - 64 bit Fowler/Noll/Vo-0 FNV-1a hash code
// Please do not copyright this code.  This code is in the public domain.
//
// LANDON CURT NOLL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
// INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO
// EVENT SHALL LANDON CURT NOLL BE LIABLE FOR ANY SPECIAL, INDIRECT OR
// CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
// USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
// OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
// PERFORMANCE OF THIS SOFTWARE.
//
// By:
//	chongo <Landon Curt Noll> /\oo/\
//      http://www.isthe.com/chongo/
//
// Share and Enjoy!	:-)
// (Code minimally adapted by Gulrak)
uint64_t fnv_64a_str(const char* str, uint64_t hval)
{
    auto* s = (unsigned char*)str; /* unsigned string */

    /*
     * FNV-1a hash each octet of the string
     */
    while (*s) {
        /* xor the bottom with the current octet */
        hval ^= (uint64_t)*s++;

        /* multiply by the 64 bit FNV magic prime mod 2^64 */
        hval *= ((uint64_t)0x100000001b3ULL);
    }
    /* return our new hash value */
    return hval;
}
//---------------------------------------------------------------------------

void updatePopupUnderMouse()
{
    g_popupUnderMouse = nullptr;
    if (!g_popupMap.empty()) {
        auto mouse = GetMousePosition();
        for (auto& [isOpen, popup] : g_popupMap) {
            if (*isOpen) {
                if ((popup._flags & WBF_MODAL) || CheckCollisionPointRec(mouse, popup.bounds())) {
                    if (!g_popupUnderMouse || g_popupUnderMouse->level() < popup.level()) {
                        g_popupUnderMouse = &popup;
                    }
                }
            }
        }
    }
}

GuiContext& context()
{
    if (g_contextStack.empty()) {
        throw std::runtime_error("No valid gui context, only call gui functions between at least one gui::Begin() and gui::End()!");
    }
    return g_contextStack.top();
}

template <typename Func, typename... Args>
typename std::invoke_result<Func, Rectangle, Args...>::type defaultWidget(Func fp, Args&&... args)
{
    auto& ctx = detail::context();
    auto size = ctx.standardSize();
    Rectangle bounds{ctx.currentPos.x + ctx.scrollOffset.x, ctx.currentPos.y + ctx.scrollOffset.y, size.x, size.y};
    ctx.increment(size);
    return (*fp)(bounds, std::forward<Args>(args)...);
}

template <typename Func, typename... Args>
typename std::invoke_result<Func, Rectangle, Args..., bool>::type editableWidget(Func fp, void* key, Args&&... args)
{
    auto& ctx = detail::context();
    auto size = ctx.standardSize();
    auto editMode = HasKeyboardFocus(key);
    auto rc = (*fp)({ctx.currentPos.x + ctx.scrollOffset.x, ctx.currentPos.y + ctx.scrollOffset.y, size.x, size.y}, std::forward<Args>(args)..., editMode);
    if (rc) {
        SetKeyboardFocus(editMode ? nullptr : key);
    }
    ctx.increment(size);
    return rc;
}

}  // namespace detail

void SetKeyboardFocus(void* key)
{
    GuiContext::editFocusId = key;
}

bool HasKeyboardFocus(void* key)
{
    return GuiContext::editFocusId == key;
}

void BeginGui(Rectangle area, RenderTexture* renderTexture, Vector2 mouseOffset, Vector2 guiScale)
{
    if (area.width <= 0) {
        area.width = renderTexture ? renderTexture->texture.width : GetScreenWidth();
    }
    if (area.height <= 0) {
        area.height = renderTexture ? renderTexture->texture.height : GetScreenHeight();
    }
    if (!g_contextStack.empty()) {
        throw std::runtime_error("Nesting of gui::BeginGui/gui::EndGui not allowed!");
    }
    g_contextStack.push({GuiContext::ctROOT, {area.x, area.y}, {area.x, area.y}, area, area});
    g_contextStack.top().mouseOffset = mouseOffset;
    SetMouseOffset(mouseOffset.x, mouseOffset.y);

    detail::updatePopupUnderMouse();
    if (!g_popupMap.empty()) {
        if (renderTexture)
            EndTextureMode();
        PopupContext::cleanupPoups();
        if (renderTexture)
            BeginTextureMode(*renderTexture);
    }
    if ((GuiContext::openDropdownboxId || g_popupUnderMouse) && !GuiIsLocked()) {
        TraceLog(LOG_DEBUG, "GUI is locked!");
        GuiLock();
    }
    else if (!(GuiContext::openDropdownboxId || g_popupUnderMouse) && GuiIsLocked()) {
        TraceLog(LOG_DEBUG, "GUI is unlocked!");
        GuiUnlock();
    }
    g_contextStack.top().contextData = renderTexture;
    GuiContext::tooltipText.clear();
    ++GuiContext::frameId;
    GuiContext::guiScale = guiScale;
    /*
    if (renderTexture) {
        GuiContext::guiScale = {(float)GetScreenWidth() / renderTexture->texture.width, (float)GetScreenHeight() / renderTexture->texture.height};
    }
    else {
        GuiContext::guiScale = {1.0f, 1.0f};
    }
     */
    // DrawRectangleRec(area, GREEN);
    GuiContext::rootContext = &g_contextStack.top();
}

void EndGui()
{
    if (g_contextStack.size() != 1) {
        throw std::runtime_error("Unbalanced gui::Begin*/gui::End*!");
    }
    g_contextStack = std::stack<GuiContext>();
    GuiContext::handleDeferredDropBoxes();
    PopupContext::renderPopups();
#if !defined(PLATFORM_WEB) && !defined(NDEBUG) && defined(RLGUIPP_DEBUG_CURSOR)
    auto pos = GetMousePosition();
    GuiDrawIcon(20, pos.x - 1, pos.y, 1, ::WHITE);
    GuiDrawIcon(20, pos.x + 1, pos.y, 1, ::WHITE);
    GuiDrawIcon(20, pos.x, pos.y - 1, 1, ::WHITE);
    GuiDrawIcon(20, pos.x, pos.y + 1, 1, ::WHITE);
    GuiDrawIcon(20, pos.x, pos.y, 1, ::BLACK);
#endif
    GuiContext::rootContext = nullptr;
}

void UnloadGui()
{
    g_menuContextMap.clear();
    g_popupMap.clear();
}

void Begin()
{
    auto& ctxParent = detail::context();
    g_contextStack.push(ctxParent);
    auto& ctx = detail::context();
    ctx.type = GuiContext::ctGROUP;
    ctx.area = {ctxParent.currentPos.x, ctxParent.currentPos.y, ctx.nextWidth > 0 ? ctx.nextWidth : ctxParent.content.width + ctxParent.content.x - ctxParent.currentPos.x, ctxParent.content.height + ctxParent.content.y - ctxParent.currentPos.y};
    ctx.content = ctx.area;
    ctx.initialPos = ctx.currentPos;
    ctx.horizontal = false;
    ctx.bordered = false;
    ctx.level++;
    ctx.nextWidth = ctx.nextHeight = -1;
    ctx.maxSize = 0;
}

void End()
{
    if (g_contextStack.size() < 2) {
        throw std::runtime_error("Unbalanced gui::Begin*/gui::End*!");
    }
    // if (detail::context().horizontal) {
    //     throw std::runtime_error("Unbalanced gui::BeginColumns/gui::EndColumns!");
    // }
    auto ctxOld = g_contextStack.top();
    g_contextStack.pop();
    auto& ctx = detail::context();
    if (ctxOld.horizontal) {
        // DrawLine(ctxOld.area.x, ctxOld.area.y, ctxOld.currentPos.x, ctxOld.initialPos.y + ctxOld.maxSize, RED);
        ctx.increment({ctxOld.currentPos.x - ctxOld.area.x, ctxOld.maxSize});
    }
    else {
        // DrawLine(ctxOld.area.x, ctxOld.area.y, ctxOld.initialPos.x + ctxOld.maxSize, ctxOld.currentPos.y, GREEN);
        ctx.increment({ctxOld.maxSize, ctxOld.currentPos.y - ctxOld.area.y});
    }
}

void BeginColumns()
{
    Begin();
    detail::context().type = GuiContext::ctCOLUMNS;
    detail::context().horizontal = true;
}

void EndColumns()
{
    if (!detail::context().horizontal) {
        throw std::runtime_error("Unbalanced gui::BeginColumns/gui::EndColumns!");
    }
    // detail::context().horizontal = false;
    End();
}

void BeginPanel(const char* text, Vector2 padding)
{
    auto& ctxParent = detail::context();
    g_contextStack.push(ctxParent);
    auto& ctx = detail::context();
    auto size = ctx.standardSize();
    ctx.type = GuiContext::ctGROUP;
    if (text) {
        GuiStatusBar({ctxParent.currentPos.x, ctxParent.currentPos.y, size.x, ctx.rowHeight}, text);
        ctx.area = {ctxParent.currentPos.x, ctxParent.currentPos.y, size.x, ctxParent.content.height + ctxParent.content.y - ctxParent.currentPos.y};
        ctx.content = {ctx.area.x + padding.x, ctx.area.y + ctx.rowHeight + padding.y, ctx.area.width - 2 * padding.x, ctx.area.height - ctx.rowHeight - 2 * padding.y};
    }
    else {
        ctx.area = {ctxParent.currentPos.x, ctxParent.currentPos.y, size.x, ctxParent.content.height + ctxParent.content.y - ctxParent.currentPos.y};
        ctx.content = {ctx.area.x + padding.x, ctx.area.y + padding.y, ctx.area.width - 2 * padding.x, ctx.area.height - 2 * padding.y};
    }
    ctx.initialPos = {ctx.content.x, ctx.content.y};
    ctx.currentPos = ctx.initialPos;
    ctx.horizontal = false;
    ctx.bordered = true;
    ctx.level++;
    ctx.groupName = text ? text : "";
    ctx.nextWidth = ctx.nextHeight = -1;
    ctx.maxSize = 0;
    ctx.padding = padding;
}

void EndPanel()
{
    auto& ctx = detail::context();
    if (ctx.level > 1) {
        GuiDrawRectangle({ctx.area.x, ctx.area.y, ctx.area.width, ctx.currentPos.y - ctx.area.y + ctx.padding.y}, 1, Fade(GetColor(gui::GetStyle(DEFAULT, LINE_COLOR)), guiAlpha), {0, 0, 0, 0});
    }
    else {
        GuiDrawRectangle({ctx.area.x, ctx.area.y, ctx.area.width, ctx.area.height}, 1, Fade(GetColor(gui::GetStyle(DEFAULT, LINE_COLOR)), guiAlpha), {0, 0, 0, 0});
    }
    // ctx.increment({0, ctx.currentPos.y - ctx.area.y + (ctx.groupName.empty() ? 10 : RAYGUI_WINDOWBOX_STATUSBAR_HEIGHT + 10)});
    //ctx.increment({1,2});
    ctx.maxSize = ctx.area.width;
    End();
}

void BeginTabView(int *activeTab)
{
    auto& tvc = TabViewContext::getContext(activeTab);
    auto& ctxParent = detail::context();
    g_contextStack.push(ctxParent);
    auto& ctx = detail::context();
    auto size = ctx.standardSize();
    ctx.type = GuiContext::ctTABVIEW;
    tvc.activeTab = activeTab;
    tvc.currentTab = 0;
    tvc.tabOffset = 2.0f;
    ctx.contextData = &tvc;
    GuiStatusBar({ctxParent.currentPos.x, ctxParent.currentPos.y, size.x, ctx.rowHeight}, " ");
}

void EndTabView()
{
    auto& ctx = detail::context();
    if (ctx.level > 1) {
        GuiDrawRectangle({ctx.area.x, ctx.area.y, ctx.area.width, ctx.currentPos.y - ctx.area.y + ctx.padding.y}, 1, Fade(GetColor(gui::GetStyle(DEFAULT, LINE_COLOR)), guiAlpha), {0, 0, 0, 0});
    }
    else {
        GuiDrawRectangle({ctx.area.x, ctx.area.y, ctx.area.width, ctx.area.height}, 1, Fade(GetColor(gui::GetStyle(DEFAULT, LINE_COLOR)), guiAlpha), {0, 0, 0, 0});
    }
    // ctx.increment({0, ctx.currentPos.y - ctx.area.y + (ctx.groupName.empty() ? 10 : RAYGUI_WINDOWBOX_STATUSBAR_HEIGHT + 10)});
    //ctx.increment({1,2});
    ctx.maxSize = ctx.area.width;
    End();
}

bool BeginTab(const char* text, Vector2 padding)
{
    auto& ctxParent = detail::context();
    auto& tvc = *std::get<TabViewContext*>(ctxParent.contextData);
    auto labelSize = MeasureTextEx(GuiGetFont(), text, 8, 0);
    bool isActive = *tvc.activeTab == tvc.currentTab;
    bool hoversOverTab = CheckCollisionPointRec(GetMousePosition(), {ctxParent.currentPos.x + tvc.tabOffset + 1, ctxParent.currentPos.y + 3, labelSize.x + 4, ctxParent.rowHeight - 4});
    auto textcol = Fade(GetColor(gui::GetStyle(TEXTBOX, isActive || hoversOverTab ? TEXT + (guiState * 3) : TEXT_COLOR_DISABLED)), guiAlpha);
    auto linecol = Fade(GetColor(gui::GetStyle(DEFAULT, BORDER_COLOR_NORMAL)), guiAlpha);
    if(!isActive && IsMouseButtonPressed(0) && hoversOverTab) {
        *tvc.activeTab = tvc.currentTab;
    }
    DrawRectangle(ctxParent.currentPos.x + tvc.tabOffset + 1, ctxParent.currentPos.y + 3, labelSize.x + 4, ctxParent.rowHeight - 4, Fade(GetColor(GuiGetStyle(STATUSBAR, isActive ? BASE_COLOR_NORMAL : BASE_COLOR_DISABLED)), guiAlpha));
    GuiDrawText(text, {ctxParent.currentPos.x + tvc.tabOffset + 1, ctxParent.currentPos.y + 2, labelSize.x + 4, ctxParent.rowHeight - 3}, TEXT_ALIGN_CENTER, textcol);
    DrawRectangle(ctxParent.currentPos.x + tvc.tabOffset, ctxParent.currentPos.y + 3, 1, ctxParent.rowHeight - 3, linecol);
    DrawRectangle(ctxParent.currentPos.x + tvc.tabOffset + 1, ctxParent.currentPos.y + 2, labelSize.x + 4, 1, linecol);
    DrawRectangle(ctxParent.currentPos.x + tvc.tabOffset + labelSize.x + 5, ctxParent.currentPos.y + 3, 1, ctxParent.rowHeight - 3, linecol);
    if(isActive) {
        DrawRectangle(ctxParent.currentPos.x + tvc.tabOffset + 1, ctxParent.currentPos.y + ctxParent.rowHeight - 1, labelSize.x + 4, 1, Fade(GetColor(GuiGetStyle(TEXTBOX, BASE_COLOR_NORMAL)), guiAlpha));
    }
    else {
        DrawRectangle(ctxParent.currentPos.x + tvc.tabOffset + 1, ctxParent.currentPos.y + ctxParent.rowHeight - 1, labelSize.x + 4, 1, linecol);
    }
    tvc.tabOffset += labelSize.x + 7;
    tvc.currentTab++;
    if(!isActive) {
        return false;
    }
    g_contextStack.push(ctxParent);
    auto& ctx = detail::context();
    auto size = ctx.standardSize();
    ctx.type = GuiContext::ctTAB;
    ctx.area = {ctxParent.currentPos.x, ctxParent.currentPos.y, size.x, ctxParent.content.height + ctxParent.content.y - ctxParent.currentPos.y};
    ctx.content = {ctx.area.x + padding.x, ctx.area.y + ctx.rowHeight + padding.y, ctx.area.width - 2 * padding.x, ctx.area.height - ctx.rowHeight - 2 * padding.y};
    ctx.initialPos = {ctx.content.x, ctx.content.y};
    ctx.currentPos = ctx.initialPos;
    ctx.horizontal = false;
    ctx.bordered = true;
    ctx.level++;
    ctx.groupName = text ? text : "";
    ctx.nextWidth = ctx.nextHeight = -1;
    ctx.maxSize = 0;
    ctx.padding = padding;
    return true;
}

void EndTab()
{
    auto& ctx = detail::context();
    if (ctx.level > 1) {
        GuiDrawRectangle({ctx.area.x, ctx.area.y, ctx.area.width, ctx.currentPos.y - ctx.area.y + ctx.padding.y}, 1, Fade(GetColor(gui::GetStyle(DEFAULT, LINE_COLOR)), guiAlpha), {0, 0, 0, 0});
    }
    else {
        GuiDrawRectangle({ctx.area.x, ctx.area.y, ctx.area.width, ctx.area.height}, 1, Fade(GetColor(gui::GetStyle(DEFAULT, LINE_COLOR)), guiAlpha), {0, 0, 0, 0});
    }
    g_contextStack.pop();
}

void BeginScrollPanel(float height, Rectangle content, Vector2 *scroll)
{
    auto& ctxParent = detail::context();
    g_contextStack.push(ctxParent);
    auto& ctx = detail::context();
    auto size = ctx.standardSize();
    ctx.type = GuiContext::ctSCROLLPANEL;
    ctx.area = {ctxParent.currentPos.x, ctxParent.currentPos.y, size.x, height > 0 ? height : ctxParent.content.height + ctxParent.content.y - ctxParent.currentPos.y};
    ctx.content = {0, 0, content.width, content.height};
    ctx.initialPos = {5, 5};
    ctx.currentPos = ctx.initialPos;
    ctx.horizontal = false;
    ctx.bordered = false;
    ctx.level = 0;
    ctx.nextWidth = ctx.nextHeight = -1;
    ctx.maxSize = 0;
    ctx.padding = {0, 0};

    Rectangle view = GuiScrollPanel(ctx.area, NULL, ctx.content, scroll);
    ctx.scrollOffset = {ctx.area.x + scroll->x, ctx.area.y + scroll->y};
    BeginScissorMode(view.x, view.y, view.width, view.height);
}

void EndScrollPanel()
{
    if (detail::context().type != GuiContext::ctSCROLLPANEL) {
        throw std::runtime_error("Unbalanced gui::BeginScrollPanel/gui::EndScrollPanel!");
    }
    EndScissorMode();
    auto ctxOld = g_contextStack.top();
    g_contextStack.pop();
    auto& ctx = detail::context();
    ctx.increment({ctx.area.width, ctxOld.area.height});
}

void BeginTableView(float height, int numColumns, Vector2 *scroll)
{
    auto& tc = TableContext::getContext(scroll);
    auto& ctx = detail::context();
    if(numColumns != tc.numColumns)
        tc.hasMeasured = false;
    auto contentHeight = tc.curHeight;
    auto contentWidth = tc.curWidth;
    tc.scroll = scroll;
    tc.numColumns = numColumns;
    tc.curRow = -1;
    tc.curColumn = -1;
    tc.curRowHeight = 0;
    tc.curColumnWidth = 0;
    tc.curX = 0;
    tc.curY = 0;
    tc.curWidth = 0;
    tc.curHeight = 0;
    tc.columnWidth.clear();
    ctx.contextData = &tc;
    BeginScrollPanel(height, {0, 0, contentWidth, contentHeight+4}, scroll);
    if(!CheckCollisionPointRec(GetMousePosition(), detail::context().area) && !GuiIsLocked()) {
        GuiLock();
        tc.lockedGui = true;
    }
}

void TableNextRow(float height, Color background)
{
    auto& ctx = detail::context();
    if(!std::holds_alternative<gui::TableContext*>(ctx.contextData))
        throw std::runtime_error("TableNextRow outside BeginTableView/EndTableView");
    auto& tc = *std::get<TableContext*>(ctx.contextData);
    ++tc.curRow;
    tc.curRowHeight = height;
    tc.curHeight += height;
    tc.curWidth = 0;
    if(background.a) {
        DrawRectangle(ctx.area.x, ctx.area.y + 2 + tc.curHeight - tc.curRowHeight + tc.scroll->y, ctx.area.width, tc.curRowHeight, background);
    }
}

bool TableNextColumn(float width)
{
    auto& ctx = detail::context();
    if(!std::holds_alternative<gui::TableContext*>(ctx.contextData))
        throw std::runtime_error("TableNextColumn outside BeginTableView/EndTableView");
    auto& tc = *std::get<TableContext*>(ctx.contextData);
    if(width > 0 && width <= 1.0f) {
        width = ctx.area.width * width;
    }
    ++tc.curColumn;
    tc.curColumnWidth = width;
    tc.curWidth += width;
    if(tc.columnWidth.size() < tc.numColumns) {
        tc.columnWidth.push_back(width);
    }
    else {
        // TODO: more checks
    }
    ctx.content = {tc.curWidth - width, tc.curHeight - tc.curRowHeight, width, tc.curRowHeight};
    ctx.initialPos = ctx.currentPos = {ctx.content.x + 2, ctx.content.y + 2};
    return tc.scroll && ctx.initialPos.y + tc.curRowHeight >= -tc.scroll->y && ctx.initialPos.y < -tc.scroll->y + ctx.area.height;
}

void EndTableView()
{
    auto& ctx = detail::context();
    if(!std::holds_alternative<gui::TableContext*>(ctx.contextData))
        throw std::runtime_error("EndTableView without matching BeginTableView");
    auto& tc = *std::get<TableContext*>(ctx.contextData);
    tc.hasMeasured = true;
    EndScrollPanel();
    if(tc.lockedGui) {
        tc.lockedGui = false;
        GuiUnlock();
    }
}

void BeginGroupBox(const char* text)
{
    auto& ctxParent = detail::context();
    g_contextStack.push(ctxParent);
    auto& ctx = detail::context();
    auto size = ctx.standardSize();
    ctx.type = GuiContext::ctGROUP;
    ctx.area = {ctxParent.currentPos.x, ctxParent.currentPos.y, size.x, ctxParent.area.height + ctxParent.area.y - ctxParent.currentPos.y};
    ctx.content = {ctx.area.x + 5, ctx.area.y + ctx.rowHeight * 2 / 3, ctx.area.width - 10, ctx.area.height - ctx.rowHeight * 2 / 3};
    ctx.initialPos = {ctx.content.x, ctx.content.y};
    ctx.currentPos = ctx.initialPos;
    ctx.horizontal = false;
    ctx.bordered = true;
    ctx.level++;
    ctx.groupName = text ? text : "";
    ctx.nextWidth = ctx.nextHeight = -1;
    ctx.maxSize = 0;
    ctx.padding = {0, 0};
}

void EndGroupBox()
{
    auto& ctx = detail::context();
    GuiGroupBox({ctx.area.x, ctx.area.y + 8, ctx.area.width, ctx.currentPos.y - ctx.area.y}, ctx.groupName.c_str());
    ctx.increment({0, ctx.spacingV});
    End();
}

void BeginPopup(Rectangle area, bool* isOpen)
{
    if(GuiContext::rootContext) {
        const auto* root = GuiContext::rootContext;
        if(area.x < 0)
            area.x = (root->area.width - area.width) / 2.0f;
        if(area.y < 0)
            area.y = (root->area.height - area.height) / 2.0f;
    }
    auto& ctxParent = detail::context();
    g_contextStack.push(ctxParent);
    auto& ctx = detail::context();
    ctx.type = GuiContext::ctPOPUP;
    ctx.area = {0, 0, area.width, area.height};
    ctx.content = ctx.area;  //{2, 2, area.width - 4, area.height - 4};
    ctx.initialPos = {0, 0};
    ctx.currentPos = ctx.initialPos;
    ctx.horizontal = false;
    ctx.bordered = false;
    ctx.level = 0;
    ctx.nextWidth = ctx.nextHeight = -1;
    ctx.maxSize = 0;
    ctx.padding = {0, 0};

    if (std::holds_alternative<RenderTexture*>(ctxParent.contextData)) {
        EndTextureMode();
    }
    auto* popup = PopupContext::find(isOpen);
    if (!popup) {
        popup = &g_popupMap.emplace(isOpen, PopupContext{area, isOpen}).first->second;
    }
    ctx.mouseOffset = {ctxParent.mouseOffset.x - popup->position().x * GuiContext::guiScale.x, ctxParent.mouseOffset.y - popup->position().y * GuiContext::guiScale.y};
    ctx.contextData = &popup->texture();
    BeginTextureMode(popup->texture());
    GuiDrawRectangle({0, 0, area.width, area.height}, 1, GetColor(GetStyle(DEFAULT, BORDER_COLOR_NORMAL)), GetColor(GetStyle(DEFAULT, BACKGROUND_COLOR)));
    SetMouseOffset((int)ctx.mouseOffset.x, (int)ctx.mouseOffset.y);
    if (g_popupUnderMouse == popup && GuiIsLocked()) {
        GuiUnlock();
    }
}

void EndPopup()
{
    if (g_popupUnderMouse && !GuiIsLocked()) {
        GuiLock();
    }
    if (g_contextStack.empty() || !std::holds_alternative<RenderTexture*>(g_contextStack.top().contextData)) {
        throw std::runtime_error("Unbalanced gui::BeginPopup/gui::EndPopup!");
    }
    EndTextureMode();
    g_contextStack.pop();
    SetMouseOffset((int)g_contextStack.top().mouseOffset.x, (int)g_contextStack.top().mouseOffset.y);
    if (std::get<RenderTexture*>(g_contextStack.top().contextData)) {
        BeginTextureMode(*std::get<RenderTexture*>(g_contextStack.top().contextData));
    }
}

bool BeginWindowBox(Rectangle area, const char* title, bool* isOpen, WindowBoxFlags flags)
{
    static bool inDrag{false};
    auto mouseParent = detail::context().mouseOffset;
    BeginPopup(area, isOpen);
    auto popup = PopupContext::find(isOpen);
    if(popup)
        popup->_flags = flags;
    auto& ctx = detail::context();
    auto rc = GuiWindowBox({0, 0, area.width, area.height}, title);
    if (!rc && (flags & WBF_MOVABLE) && IsMouseButtonPressed(0) && CheckCollisionPointRec(GetMousePosition(), {0, 0, area.width, RAYGUI_WINDOWBOX_STATUSBAR_HEIGHT})) {
        inDrag = true;
    }
    else if (inDrag) {
        auto delta = GetMouseDelta();
        if (popup) {
            popup->move(delta.x / GuiContext::guiScale.x, delta.y / GuiContext::guiScale.y);
            ctx.mouseOffset = {mouseParent.x - popup->position().x * GuiContext::guiScale.x, mouseParent.y - popup->position().y * GuiContext::guiScale.y};
            SetMouseOffset((int)ctx.mouseOffset.x, (int)ctx.mouseOffset.y);
        }
        else {
            inDrag = false;
        }
        if (IsMouseButtonReleased(0)) {
            inDrag = false;
        }
    }
    ctx.content = {ctx.area.x + 5, ctx.area.y + RAYGUI_WINDOWBOX_STATUSBAR_HEIGHT + 5, ctx.area.width - 10, ctx.area.height - RAYGUI_WINDOWBOX_STATUSBAR_HEIGHT - 10};
    ctx.initialPos = {ctx.content.x, ctx.content.y};
    ctx.currentPos = ctx.initialPos;
    return rc;
}

void EndWindowBox()
{
    EndPopup();
}

void SetState(int state)
{
    GuiSetState(state);
}

int GetState()
{
    return GuiGetState();
}

void SetStyle(int control, int property, int value)
{
    GuiSetStyle(control, property, value);
}

int GetStyle(int control, int property)
{
    return GuiGetStyle(control, property);
}

void SetIndent(float width)
{
    auto& ctx = detail::context();
    ctx.currentPos.x = ctx.initialPos.x + width;
}

void SetReserve(float width)
{
    auto& ctx = detail::context();
    ctx.reserve = width;
}

void SetNextWidth(float width)
{
    detail::context().nextWidth = width;
}

void SetRowHeight(float height)
{
    detail::context().rowHeight = height;
}

void SetSpacing(float spacing)
{
    auto& ctx = detail::context();
    if (ctx.horizontal) {
        ctx.spacingH = spacing;
    }
    else {
        ctx.spacingV = spacing;
    }
}

Vector2 GetCurrentPos()
{
    return detail::context().currentPos;
}

Rectangle GetContentAvailable()
{
    auto& ctx = detail::context();
    return {ctx.currentPos.x, ctx.currentPos.y, ctx.content.width + ctx.content.x - ctx.currentPos.x, ctx.content.height + ctx.content.y - ctx.currentPos.y};
}

Rectangle GetLastWidgetRect()
{
    return detail::context().lastWidgetRect;
}

void SetTooltip(const std::string& tooltip)
{
    auto rect = GetLastWidgetRect();
    //if(tooltip == "PAUSE") {
    //    TraceLog(LOG_INFO, "SetToolip: %f,%f,%f,%f", rect.x, rect.y, rect.width, rect.height);
    //}
    if(CheckCollisionPointRec(GetMousePosition(), rect)) {
        GuiContext::tooltipText = tooltip;
        GuiContext::tooltipParentRect = rect;
    }
}

void Space(float size)
{
    auto& ctx = detail::context();
    if (size < 0) {
        size = ctx.horizontal ? ctx.spacingH : ctx.spacingV;
    }
    if (ctx.horizontal) {
        ctx.increment({size, ctx.rowHeight});
    }
    else {
        ctx.increment({ctx.content.width, size});
    }
}

void Separator(float size)
{
    auto& ctx = detail::context();
    if (size <= 0) {
        size = ctx.spacingV;
    }
    auto offset = -ctx.spacingV + (int)(size / 2) + 1;
    if (ctx.bordered) {
        DrawLine(ctx.area.x, ctx.currentPos.y + offset, ctx.area.x + ctx.area.width, ctx.currentPos.y + offset, Fade(GetColor(gui::GetStyle(DEFAULT, LINE_COLOR)), guiAlpha));
    }
    else {
        DrawLine(ctx.content.x, ctx.currentPos.y + offset, ctx.content.x + ctx.content.width, ctx.currentPos.y + offset, Fade(GetColor(gui::GetStyle(DEFAULT, LINE_COLOR)), guiAlpha));
    }
    ctx.increment({ctx.content.width, -2 * ctx.spacingV + size});
}

void Label(const char* text)
{
    detail::defaultWidget(GuiLabel, text);
}

bool Button(const char* text)
{
    return detail::defaultWidget(GuiButton, text);
}

bool LabelButton(const char* text)
{
    return detail::defaultWidget(GuiLabelButton, text);
}

bool Toggle(const char* text, bool active)
{
    return detail::defaultWidget(GuiToggle, text, active);
}

static void CountGuiTextItems(const char* text, short& numRows, short& numCols)
{
    numRows = 1;
    numCols = 1;
    short col = 1;
    char c;
    while ((c = *text++)) {
        if (c == ';') {
            ++col;
            if (col > numCols)
                numCols = col;
        }
        else if (c == '\n') {
            ++numRows;
            col = 1;
        }
    }
}

int ToggleGroup(const char* text, int active)
{
    auto& ctx = detail::context();
    bool asLines = false;
    short rows = 0;
    short cols = 0;
    CountGuiTextItems(text, rows, cols);
    auto size = ctx.standardSize();
    auto rc = GuiToggleGroup({ctx.currentPos.x + ctx.scrollOffset.x, ctx.currentPos.y + ctx.scrollOffset.y, size.x, size.y}, text, active);
    ctx.increment({cols * size.x + (cols - 1) * (float)GuiGetStyle(TOGGLE, GROUP_PADDING), rows * size.y + (rows - 1) * (float)GuiGetStyle(TOGGLE, GROUP_PADDING)});
    return rc;
}

bool CheckBox(const char* text, bool checked)
{
    auto& ctx = detail::context();
    auto size = ctx.standardSize();
    auto rc = GuiCheckBox({ctx.currentPos.x + ctx.scrollOffset.x, ctx.currentPos.y + ctx.scrollOffset.y + (ctx.rowHeight - 15) / 2, 15, 15}, text, checked);
    ctx.increment(size);
    return rc;
}

int ComboBox(const char* text, int active)
{
    return detail::defaultWidget(GuiComboBox, text, active);
}

bool DropdownBox(const char* text, int* active)
{
    return detail::defaultWidget(GuiContext::deferDropdownBox, text, active);
}

bool Spinner(const char* text, int* value, int minValue, int maxValue)
{
    return detail::editableWidget(GuiSpinner, (void*)value, text, value, minValue, maxValue);
}

bool ValueBox(const char* text, int* value, int minValue, int maxValue)
{
    return detail::editableWidget(GuiValueBox, (void*)value, text, value, minValue, maxValue);
}

namespace detail {

bool TextBoxImpl(Rectangle bounds, char* text, int bufferSize, bool editMode)
{
    GuiState state = guiState;
    bool pressed = false;
    int textWidth = GetTextWidth(text);
    auto textBounds = GetTextBounds(TEXTBOX, bounds);
    int textAlignment = textWidth >= textBounds.width ? TEXT_ALIGN_RIGHT : GuiGetStyle(TEXTBOX, TEXT_ALIGNMENT);

    Rectangle cursor = {bounds.x + GuiGetStyle(TEXTBOX, TEXT_PADDING) + GetTextWidth(text) + 2, bounds.y + bounds.height / 2 - GuiGetStyle(DEFAULT, TEXT_SIZE), 4, (float)GuiGetStyle(DEFAULT, TEXT_SIZE) * 2};

    // Update control
    //--------------------------------------------------------------------
    if ((state != STATE_DISABLED) && !guiLocked) {
        Vector2 mousePoint = GetMousePosition();

        if (editMode) {
            state = STATE_PRESSED;

            int key = GetCharPressed();  // Returns codepoint as Unicode
            int keyCount = (int)strlen(text);
            int byteSize = 0;
            const char* textUTF8 = CodepointToUTF8(key, &byteSize);

            // Only allow keys in range [32..125]
            if ((keyCount + byteSize) < bufferSize) {
                float maxWidth = (bounds.width - (GuiGetStyle(TEXTBOX, TEXT_INNER_PADDING) * 2));

                if ((key >= 32)) {
                    for (int i = 0; i < byteSize; i++) {
                        text[keyCount] = textUTF8[i];
                        keyCount++;
                    }

                    text[keyCount] = '\0';
                }
            }

            // Delete text
            if (keyCount > 0) {
                if (IsKeyPressed(KEY_BACKSPACE)) {
                    while ((keyCount > 0) && ((text[--keyCount] & 0xc0) == 0x80))
                        ;
                    text[keyCount] = '\0';
                }
            }

            if (IsKeyPressed(KEY_ENTER) || (!CheckCollisionPointRec(mousePoint, bounds) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)))
                pressed = true;

            // Check text alignment to position cursor properly
            if (textAlignment == TEXT_ALIGN_CENTER)
                cursor.x = bounds.x + textWidth / 2 + bounds.width / 2 + 1;
            else if (textAlignment == TEXT_ALIGN_RIGHT)
                cursor.x = bounds.x + bounds.width - GuiGetStyle(TEXTBOX, TEXT_INNER_PADDING);
        }
        else {
            if (CheckCollisionPointRec(mousePoint, bounds)) {
                state = STATE_FOCUSED;
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
                    pressed = true;
            }
        }
    }
    //--------------------------------------------------------------------

    // Draw control
    //--------------------------------------------------------------------
    if (state == STATE_PRESSED) {
        GuiDrawRectangle(bounds, GuiGetStyle(TEXTBOX, BORDER_WIDTH), Fade(GetColor(GuiGetStyle(TEXTBOX, BORDER + (state * 3))), guiAlpha), Fade(GetColor(GuiGetStyle(TEXTBOX, BASE_COLOR_PRESSED)), guiAlpha));
    }
    else if (state == STATE_DISABLED) {
        GuiDrawRectangle(bounds, GuiGetStyle(TEXTBOX, BORDER_WIDTH), Fade(GetColor(GuiGetStyle(TEXTBOX, BORDER + (state * 3))), guiAlpha), Fade(GetColor(GuiGetStyle(TEXTBOX, BASE_COLOR_DISABLED)), guiAlpha));
    }
    else
        GuiDrawRectangle(bounds, 1, Fade(GetColor(GuiGetStyle(TEXTBOX, BORDER + (state * 3))), guiAlpha), BLANK);

    BeginScissorMode(textBounds.x, textBounds.y, textBounds.width, textBounds.height);
    GuiDrawText(text, textBounds, textAlignment, Fade(GetColor(GuiGetStyle(TEXTBOX, TEXT + (state * 3))), guiAlpha));
    EndScissorMode();

    // Draw cursor
    if (editMode) {
        if (cursor.x >= bounds.x + bounds.width - 4)
            cursor.x = bounds.x + bounds.width - 5;
        if (cursor.height >= bounds.height)
            cursor.height = bounds.height - GuiGetStyle(TEXTBOX, BORDER_WIDTH) * 2;
        if (cursor.y < (bounds.y + GuiGetStyle(TEXTBOX, BORDER_WIDTH)))
            cursor.y = bounds.y + GuiGetStyle(TEXTBOX, BORDER_WIDTH);

        GuiDrawRectangle(cursor, 0, BLANK, Fade(GetColor(GuiGetStyle(TEXTBOX, BORDER_COLOR_PRESSED)), guiAlpha));
    }
    //--------------------------------------------------------------------

    return pressed;
}

}  // namespace detail

bool TextBox(char* text, int textSize)
{
    return detail::editableWidget(detail::TextBoxImpl, (void*)text, text, textSize);
}

bool TextBox(std::string& text, int textSize)
{
    text.resize(textSize + 1);
    auto rc = detail::editableWidget(detail::TextBoxImpl, (void*)&text, text.data(), textSize);
    text.resize(std::strlen(text.data()));
    return rc;
}

bool TextBoxMulti(float height, char* text, int textSize)
{
    auto& ctx = detail::context();
    auto size = ctx.standardSize(height);
    auto editMode = HasKeyboardFocus((void*)text);
    auto rc = GuiTextBoxMulti({ctx.currentPos.x + ctx.scrollOffset.x, ctx.currentPos.y + ctx.scrollOffset.y, size.x, size.y}, text, textSize, editMode);
    if (rc) {
        SetKeyboardFocus(editMode ? nullptr : (void*)text);
    }
    ctx.increment(size);
    return rc;
}

int ListView(float height, const char* text, int* scrollIndex, int active)
{
    auto& ctx = detail::context();
    auto size = ctx.standardSize(height);
    auto rc = GuiListView({ctx.currentPos.x + ctx.scrollOffset.x, ctx.currentPos.y + ctx.scrollOffset.y, size.x, size.y}, text, scrollIndex, active);
    ctx.increment(size);
    return rc;
}

int ListViewEx(float height, const char** text, int count, int* focus, int* scrollIndex, int active)
{
    auto& ctx = detail::context();
    auto size = ctx.standardSize(height);
    auto rc = GuiListViewEx({ctx.currentPos.x + ctx.scrollOffset.x, ctx.currentPos.y + ctx.scrollOffset.y, size.x, size.y}, text, count, focus, scrollIndex, active);
    ctx.increment(size);
    return rc;
}

Color ColorPicker(Color color)
{
    auto& ctx = detail::context();
    auto size = ctx.standardSize();
    size.y = size.x;
    auto barSpace = GetStyle(COLORPICKER, HUEBAR_PADDING) + GuiGetStyle(COLORPICKER, HUEBAR_WIDTH) + GetStyle(COLORPICKER, HUEBAR_SELECTOR_OVERFLOW);
    auto rc = GuiColorPicker({ctx.currentPos.x + ctx.scrollOffset.x, ctx.currentPos.y + ctx.scrollOffset.y, size.x - barSpace, size.y - barSpace}, nullptr, color);
    ctx.increment({size.x, size.y - barSpace});
    return rc;
}

Color ColorPanel(const char* text, Color color)
{
    auto& ctx = detail::context();
    auto size = ctx.standardSize();
    size.y = size.x;
    auto rc = GuiColorPicker({ctx.currentPos.x + ctx.scrollOffset.x, ctx.currentPos.y + ctx.scrollOffset.y, size.x, size.y}, nullptr, color);
    ctx.increment({size.x, size.y});
    return rc;
}

float ColorBarAlpha(const char* text, float alpha)
{
    auto& ctx = detail::context();
    auto size = ctx.standardSize();
    auto rc = GuiColorBarAlpha({ctx.currentPos.x + ctx.scrollOffset.x, ctx.currentPos.y + ctx.scrollOffset.y, size.x, size.y}, text, alpha);
    ctx.increment(size);
    return rc;
}

float ColorBarHue(const char* text, float value)
{
    return 0;
}

float Slider(const char* textLeft, const char* textRight, float value, float minValue, float maxValue)
{
    auto& ctx = detail::context();
    auto size = ctx.standardSize();
    auto leftWidth = GetTextWidth(textLeft);
    auto rightWidth = GetTextWidth(textRight);
    auto leftOffset = ctx.currentPos.x - ctx.content.x >= leftWidth ? 0 : leftWidth + (leftWidth ? 4 : 0);
    auto rightSpace = ctx.reserve >= rightWidth ? ctx.reserve : rightWidth + (rightWidth ? 4 : 0);
    auto rc = GuiSlider({ctx.currentPos.x + leftOffset + ctx.scrollOffset.x, ctx.currentPos.y + ctx.scrollOffset.y, size.x - rightSpace - leftOffset, size.y}, textLeft, textRight, value, minValue, maxValue);
    ctx.increment(size);
    return rc;
}

float Slider(const char* textLeft, float value, float minValue, float maxValue)
{
    return Slider(textLeft, TextFormat("%.2f", value), value, minValue, maxValue);
}

float SliderBar(const char* textLeft, const char* textRight, float value, float minValue, float maxValue)
{
    auto& ctx = detail::context();
    auto size = ctx.standardSize();
    auto leftWidth = GetTextWidth(textLeft);
    auto rightWidth = GetTextWidth(textRight);
    auto leftOffset = ctx.currentPos.x - ctx.content.x >= leftWidth ? 0 : leftWidth + (leftWidth ? 4 : 0);
    auto rightSpace = ctx.reserve >= rightWidth ? ctx.reserve : rightWidth + (rightWidth ? 4 : 0);
    auto rc = GuiSliderBar({ctx.currentPos.x + leftOffset + ctx.scrollOffset.x, ctx.currentPos.y + ctx.scrollOffset.y, size.x - rightSpace - leftOffset, size.y}, textLeft, textRight, value, minValue, maxValue);
    ctx.increment(size);
    return rc;
}

float SliderBar(const char* textLeft, float value, float minValue, float maxValue)
{
    return SliderBar(textLeft, TextFormat("%.2f", value), value, minValue, maxValue);
}

float ProgressBar(const char* textLeft, const char* textRight, float value, float minValue, float maxValue)
{
    auto& ctx = detail::context();
    auto size = ctx.standardSize();
    auto leftWidth = GetTextWidth(textLeft);
    auto rightWidth = GetTextWidth(textRight);
    auto leftOffset = ctx.currentPos.x - ctx.content.x >= leftWidth ? 0 : leftWidth + (leftWidth ? 4 : 0);
    auto rightSpace = ctx.reserve >= rightWidth ? ctx.reserve : rightWidth + (rightWidth ? 4 : 0);
    auto rc = GuiProgressBar({ctx.currentPos.x + leftOffset + ctx.scrollOffset.x, ctx.currentPos.y + ctx.scrollOffset.y, size.x - rightSpace - leftOffset, size.y}, textLeft, textRight, value, minValue, maxValue);
    ctx.increment(size);
    return rc;
}

Vector2 Grid(float height, float spacing, int subdivs)
{
    auto& ctx = detail::context();
    auto size = ctx.standardSize(height);
    auto rc = GuiGrid({ctx.currentPos.x + ctx.scrollOffset.x, ctx.currentPos.y + ctx.scrollOffset.y, size.x, size.y}, nullptr, spacing, subdivs);
    ctx.increment(size);
    return rc;
}

void StatusBar(const char* text)
{
    auto& ctx = detail::context();
    GuiStatusBar({ctx.area.x, ctx.area.y + ctx.area.height - ctx.rowHeight, ctx.area.width, ctx.rowHeight}, text);
    ctx.content.height -= ctx.rowHeight;
}

void StatusBar(std::initializer_list<std::pair<float, const char*>> fields)
{
    auto& ctx = detail::context();
    auto absSum = 0.0f;
    auto padding = GetStyle(STATUSBAR, TEXT_PADDING);
    for (auto& [width, text] : fields) {
        if (width > 1.0f) {
            absSum += width;
        }
    }
    auto totalWidth = ctx.area.width;
    auto avail = totalWidth - absSum;
    auto x = ctx.area.x;
    auto count = 0;
    for (auto& [width, text] : fields) {
        bool lastField = count == fields.size() - 1;
        float fieldWidth = lastField ? totalWidth : width > 1.0f ? std::floor(width) : std::floor(avail * width);
        GuiStatusBar({x, ctx.area.y + ctx.area.height - ctx.rowHeight, lastField ? totalWidth : fieldWidth + 1, ctx.rowHeight}, text);
        totalWidth -= fieldWidth;
        x += fieldWidth;
        ++count;
    }
    ctx.content.height -= ctx.rowHeight;
}

const char* IconText(int iconId, const char* text)
{
    static char buffer[1024];
#ifdef WIN32
    ::sprintf_s(buffer, 1023, "#%03i#", iconId);
#else
    ::snprintf(buffer, 1023, "#%03i#%s", iconId, text);
#endif
    return buffer;
}

bool BeginMenuBar()
{
    auto& ctxParent = detail::context();
    if (ctxParent.type != GuiContext::ctROOT && ctxParent.type != GuiContext::ctPOPUP) {
        throw std::runtime_error("BeginMenuBar is only allowed following a BeginGui or a BeginWindowBox");
    }
    auto id = std::holds_alternative<RenderTexture*>(ctxParent.contextData) ? (void*)std::get<RenderTexture*>(ctxParent.contextData) : nullptr;
    auto& menuBar = MenuBar::getMenuBar(id);
    g_contextStack.push(ctxParent);
    auto& ctx = detail::context();
    auto size = ctx.standardSize();
    GuiStatusBar({ctxParent.currentPos.x, ctxParent.currentPos.y, size.x, ctx.rowHeight}, nullptr);
    ctx.type = GuiContext::ctMENUBAR;
    ctx.initialPos = {ctx.content.x + 5, ctx.content.y};
    ctx.currentPos = ctx.initialPos;
    ctx.horizontal = false;
    ctx.bordered = true;
    ctx.level++;
    ctx.groupName = "";
    ctx.nextWidth = ctx.nextHeight = -1;
    ctx.maxSize = 0;
    ctx.padding = {5, 0};
    if (GetState() == STATE_DISABLED) {
        return false;
    }
    return false;
}

void EndMenuBar()
{
    g_contextStack.pop();
    auto& ctx = detail::context();
    ctx.currentPos.y += ctx.rowHeight;
}

bool BeginMenu(const char* text)
{
    MenuContext& mctx = MenuContext::getContext(text);
    auto& ctxParent = detail::context();
    auto size = ctxParent.standardSize();
    auto textSize = GetTextWidth(text);
    auto oldState = GetState();
    SetState(mctx.isOpen ? STATE_FOCUSED : oldState);
    auto oldBorder = GetStyle(BUTTON, BORDER_WIDTH);
    SetStyle(BUTTON, BORDER_WIDTH, 0);
    if (GuiButton({ctxParent.currentPos.x, ctxParent.currentPos.y + 1, textSize + 10.0f, size.y - 2}, text)) {
        mctx.isOpen = !mctx.isOpen;
    }
    else if (IsMouseButtonPressed(0) && !CheckCollisionPointRec(GetMousePosition(), mctx.area) && !CheckCollisionPointRec(GetMousePosition(), {ctxParent.currentPos.x, ctxParent.currentPos.y, textSize + 10.0f, size.y})) {
        mctx.isOpen = false;
    }
    SetStyle(BUTTON, BORDER_WIDTH, oldBorder);
    SetState(oldState);
    auto leftEdge = ctxParent.currentPos.x;
    ctxParent.currentPos.x += textSize + 12.0f;
    if (!mctx.isOpen || GetState() == STATE_DISABLED) {
        return false;
    }
    if (mctx.height > 0 && mctx.maxWidth > 0) {
        mctx.area = {leftEdge, ctxParent.currentPos.y + ctxParent.rowHeight - 1, mctx.maxWidth + 10, mctx.height + ctxParent.spacingV * 2};
        BeginPopup(mctx.area, &mctx.isOpen);
        GuiStatusBar({0, 0, mctx.area.width, mctx.area.height}, nullptr);
    }
    g_contextStack.push(g_contextStack.top());
    auto& ctx = detail::context();
    ctx.type = GuiContext::ctMENU;
    ctx.content = {ctx.content.x + 5, ctx.content.y + ctx.spacingV / 2, ctx.content.width - 10, ctx.content.height - ctx.spacingV};
    ctx.initialPos = {ctx.content.x, ctx.content.y};
    ctx.currentPos = ctx.initialPos;
    ctx.horizontal = false;
    ctx.bordered = true;
    ctx.level++;
    ctx.groupName = "";
    ctx.nextWidth = ctx.nextHeight = -1;
    ctx.maxSize = 0;
    ctx.padding = {5, 0};
    ctx.contextData = &mctx;
    return true;
}

void EndMenu()
{
    auto& ctx = detail::context();
    if (ctx.type != GuiContext::ctMENU) {
        throw std::runtime_error("EndMenu without matching BeginMenu");
    }
    g_contextStack.pop();
    if (g_contextStack.top().type == GuiContext::ctPOPUP) {
        EndPopup();
    }
}

bool MenuItem(const char* text, uint32_t shortcut, bool* selected)
{
    auto& ctx = detail::context();
    if (ctx.type != GuiContext::ctMENU) {
        throw std::runtime_error("MenuItem outside BeginMenu/EndMenu");
    }
    auto& mctx = *std::get<MenuContext*>(ctx.contextData);
    if (text) {
        auto width = GetTextWidth(text);
        if (mctx.area.height > mctx.height) {
            auto oldBorder = GetStyle(BUTTON, BORDER_WIDTH);
            auto oldPadding = GetStyle(BUTTON, TEXT_PADDING);
            auto oldAlign = GetStyle(BUTTON, TEXT_ALIGNMENT);
            SetStyle(BUTTON, BORDER_WIDTH, 0);
            SetStyle(BUTTON, TEXT_PADDING, ctx.spacingH / 2);
            SetStyle(BUTTON, TEXT_ALIGNMENT, TEXT_ALIGN_LEFT);
            auto rc = Button(text);
            SetStyle(BUTTON, BORDER_WIDTH, oldBorder);
            SetStyle(BUTTON, TEXT_PADDING, oldPadding);
            SetStyle(BUTTON, TEXT_ALIGNMENT, oldAlign);
            if (rc) {
                mctx.isOpen = false;
            }
            return rc;
        }
        else {
            auto size = ctx.standardSize();
            mctx.maxWidth = std::max(mctx.maxWidth, (float)width + ctx.spacingH);
            mctx.height += size.y + (mctx.height > 0 ? ctx.spacingV : 0);
        }
    }
    else {
        // separator
        if (mctx.area.height > mctx.height) {
            Separator();
        }
        else {
            mctx.height += -ctx.spacingV;
        }
    }
    return false;
}

int BeginPopupMenu(Vector2 position, const char* items)
{
    return -1;
}

void EndPopupMenu() {}

}  // namespace gui

#endif  // RLGUIPP_IMPLEMENTATION
