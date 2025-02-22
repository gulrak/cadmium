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
#pragma once

// rlGuipp version in decimal (major * 10000 + minor * 100 + patch)
#define RLGUIPP_VERSION 200L
#define RLGUIPP_VERSION_STRING "0.2.0"

#define RLGUIPP_API

#include "icons.h"

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

#include "raygui4.5.h"
#include "rlgl.h"

#pragma GCC diagnostic pop
}

#include <cstdint>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <string>

#ifdef PLATFORM_WEB
#include <emscripten/emscripten.h>
#endif

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
RLGUIPP_API void BeginPanel(const char* text = nullptr, Vector2 padding = {5, 5});                                  // start a panel (a group with a kind of title bar, if title is given), must be closed with EndPanel()
RLGUIPP_API void EndPanel();                                                                                              // end the description of a panel group
RLGUIPP_API void BeginTabView(int *activeTab);                                                                            // start a tab view (a stack of groups with labeled tabs on top), must be closed with EndTabView()
RLGUIPP_API void EndTabView();                                                                                            // end the description of a tab view
RLGUIPP_API bool BeginTab(const char* text, Vector2 padding = {5, 5});                                              // start a tab in a tab view (a page with the given text as label top), must be closed with EndTab()
RLGUIPP_API void EndTab();                                                                                                // end the description of a Tab group
RLGUIPP_API void BeginScrollPanel(float height, Rectangle content, Vector2* scroll);                                      // start a scrollable panel with the given content size (pos is ignored), and scrolled to offset scroll
RLGUIPP_API void EndScrollPanel();                                                                                        // end the description of the scroll panel
RLGUIPP_API void BeginTableView(float height, int numColumns, Vector2 *scroll);                                           //
RLGUIPP_API void TableNextRow(float height, Color background = {0, 0, 0, 0});                                  //
RLGUIPP_API bool TableNextColumn(float width);                                                                            //
RLGUIPP_API void TableNextColumn(float width, const std::function<void(Rectangle rect)>& handler);                              //
RLGUIPP_API void EndTableView();                                                                                          //
RLGUIPP_API void BeginGroupBox(const char* text = nullptr);                                                               // start a group box, similar to panel but no title bar, title is instead in a gap of the border, must be closed with EndGroupBox()
RLGUIPP_API void EndGroupBox();                                                                                           // end the description of a group box
RLGUIPP_API void BeginPopup(Rectangle area, bool* isOpen);                                                                // show a popup at the given area, open as long as `*isOpen` is true
RLGUIPP_API void EndPopup();                                                                                              // end of the popup description
RLGUIPP_API bool BeginWindowBox(Rectangle area, const char* title, bool* isOpen, WindowBoxFlags flags = WBF_NONE);        // same as a popup, but with a title bar and a close button, optionally draggable
RLGUIPP_API void EndWindowBox();                                                                                          // end of the WindowBox
RLGUIPP_API void BeginClipping(const Rectangle& clipArea);                                                                // set a new clipping area (it will be clipped to the previous one, potentially setting an area of size 0)
RLGUIPP_API void EndClipping();                                                                                           // pop the current clipping area
RLGUIPP_API void SetState(int state);                                                                                     // same as raygui GuiSetState
RLGUIPP_API int GetState();                                                                                               // same as raygui GuiGetState
RLGUIPP_API void SetStyle(int control, int property, int value);                                                          // Set one style property
RLGUIPP_API int GetStyle(int control, int property);                                                                      // Get one style property
RLGUIPP_API void SetIndent(float width);                                                                                  // Indents all following elements in this level
RLGUIPP_API void SetReserve(float width);                                                                                 // Sets the space reserved for right side labels of bar widgets
RLGUIPP_API void SetNextWidth(float width);                                                                               // Set the width of the next element, default is the width of the parent
RLGUIPP_API void SetNextHeight(float height);                                                                             // Set the height of the next element, default is the row height
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
RLGUIPP_API bool DropdownBox(const char* text, int* active, bool directionUp = false);                                    // Dropdown Box control, returns selected item
RLGUIPP_API bool Spinner(const char* text, int* value, int minValue, int maxValue);                                       // Spinner control, returns selected value
RLGUIPP_API bool ValueBox(const char* text, int* value, int minValue, int maxValue);                                      // Value Box control, updates input text with numbers
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
RLGUIPP_API Color ColorFromHsv(Vector3 hsv);
RLGUIPP_API Vector3 HsvFromColor(Color col);

RLGUIPP_API const char* IconText(int icon, const char* text);

RLGUIPP_API bool BeginMenuBar();
RLGUIPP_API void EndMenuBar();
RLGUIPP_API bool BeginMenu(const char* text);
RLGUIPP_API void EndMenu();
#define MENU_SHORTCUT(modifier, key) static_cast<uint32_t>(((modifier) << 16) | (key))
RLGUIPP_API bool MenuItem(const char* text, uint32_t shortcut = 0, bool* selected = nullptr);
RLGUIPP_API int BeginPopupMenu(Vector2 position, const char* items);  // A popup menu with the items being given in raygui way seperated
RLGUIPP_API void EndPopupMenu();

RLGUIPP_API bool IsSysKeyDown();  // If macOS same as "IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER)" else "IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)"

}  // namespace gui

