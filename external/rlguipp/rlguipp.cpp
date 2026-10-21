//
// Created by schuemann on 19.12.24.
//
//-----------------------------------------------------------------------------
// IMPLEMENTATION
//-----------------------------------------------------------------------------

#define RAYGUI_IMPLEMENTATION
#define RLGUIPP_IMPLEMENTATION
#include <rlguipp/rlguipp.hpp>
#include <ghc/fs_fwd.hpp>
#include <ghc/utf8.hpp>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <deque>
#include <stack>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

// Called on misuse of the api, like unbalanced Begin*/End* or PushId/PopId calls. Release builds log the
// error and repair the state instead. Define RLGUIPP_ASSERT (e.g. empty) to test that in a debug build.
#ifndef RLGUIPP_ASSERT
#define RLGUIPP_ASSERT(expr) assert(expr)
#endif

namespace gui {


int GuiDropupBox(Rectangle bounds, const char *text, int *active, bool editMode);

extern "C" {
Rectangle clipRectangle(const Rectangle& clipRect, const Rectangle& rect);
void clipRectangles(const Rectangle& destClipRect, Rectangle& srcRect, Rectangle& dstRect);
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
uint64_t fnv_64a_buf(const void* buf, size_t len, uint64_t hval)
{
    auto* bp = (const unsigned char*)buf; /* start of buffer */
    auto* be = bp + len;                  /* beyond end of buffer */

    /*
     * FNV-1a hash each octet of the buffer
     */
    while (bp < be) {
        /* xor the bottom with the current octet */
        hval ^= (uint64_t)*bp++;

        /* multiply by the 64 bit FNV magic prime mod 2^64 */
        hval *= ((uint64_t)0x100000001b3ULL);
    }
    /* return our new hash value */
    return hval;
}
//---------------------------------------------------------------------------

}

namespace PopupMenu {

namespace detail {

struct OpenMenu {
    Vector2 position{0, 0};
    Menu const* items = nullptr;
    int hoveredIndex = -1;
    float hoverTime = 0.0f;
    int width = 0;                       // cached calculated width
};

} // namespace detail

// The popup menu system, there is one instance in GuiContext, updated in BeginGui() and drawn in EndGui()
class System {
public:
    Style style;

    // Open a popup menu at the given position, the menu is copied
    void open(Vector2 position, const Menu& menu);

    // Close all menus
    void close();

    // Check if any menu is currently open
    [[nodiscard]] bool isOpen() const;

    // Check if the open menu was opened from `menu`
    [[nodiscard]] bool isMenuOpen(const Menu& menu) const;

    // Update menu state - call once per frame
    // Returns true if a menu action was triggered this frame
    bool update(float dt);

    // Draw menus - call after drawing your scene (menus draw on top)
    void render();

private:
    Menu _menu;
    const Menu* _openedFrom{nullptr};   // only used for comparing, never dereferenced
    std::vector<detail::OpenMenu> _menuChain;
    float _closeGraceTimer = 0.0f;
    int _closeGraceLevel = -1;
    bool _needsUnlock = false;

    // Font size and item height, derived from the raygui style if not set in `style`
    [[nodiscard]] int fontSize() const;
    [[nodiscard]] int itemHeight() const;

    // Calculate bounds for a menu panel
    [[nodiscard]] Rectangle getMenuBounds(const detail::OpenMenu& menu) const;

    // Calculate the width needed for a menu
    [[nodiscard]] int calculateMenuWidth(const Menu& items) const;

    // Calculate the text width for gui font
    [[nodiscard]] int calculateTextWidth(const std::string& text) const;

    // Calculate total height of a menu
    [[nodiscard]] int calculateMenuHeight(const Menu& items) const;

    // Get item height (separators are shorter)
    [[nodiscard]] int getItemHeight(const Item& item) const;

    // Get which item index is at a Y position within a menu, or -1
    [[nodiscard]] int getItemAtYPos(const detail::OpenMenu& menu, float y) const;

    // Open a submenu for the given parent level and item index
    void openSubmenu(int parent_level, int item_index);

    // Trigger an item's action
    void triggerItem(const Item& item);

    // Draw a single menu panel
    void drawMenu(const detail::OpenMenu& menu) const;

    // Draw a single item within a menu
    void drawItem(const Item& item, Rectangle bounds, bool hovered) const;
};

} // namespace PopupMenu

struct PopupContext
{
    explicit PopupContext(Rectangle rect, bool* isOpen);
    PopupContext(const PopupContext&) = delete;
    PopupContext(PopupContext&& other) noexcept
        : _level(other._level)
        , _position(other._position)
        , _content(std::exchange(other._content, RenderTexture{0}))
        , _lastUpdate(other._lastUpdate)
        , _flags(other._flags)
        , _isOpen(other._isOpen)
        , _inDrag(other._inDrag)
        , _visible(other._visible)
    {
    }
    ~PopupContext() { UnloadRenderTexture(_content); }
    int level() const { return _level; }
    Vector2 position() const { return {std::round(_position.x), std::round(_position.y)}; }
    Rectangle bounds() const { return {std::round(_position.x), std::round(_position.y), (float)_content.texture.width, (float)_content.texture.height}; }
    RenderTexture& texture() { return _content; }
    void move(float dx, float dy) { _position.x += dx, _position.y += dy; }
    void render() const
    {
        DrawRectangle(position().x + 4, position().y + 4, _content.texture.width, _content.texture.height, {0, 0, 0, 96});
        DrawTextureRec(_content.texture, {0, 0, (float)_content.texture.width, -(float)_content.texture.height}, position(), ::WHITE);
    }
    PopupContext& operator=(const PopupContext&) = delete;
    PopupContext& operator=(PopupContext&& other) noexcept
    {
        if (this != &other) {
            UnloadRenderTexture(_content);
            _level = other._level;
            _position = other._position;
            _content = std::exchange(other._content, RenderTexture{0});
            _lastUpdate = other._lastUpdate;
            _flags = other._flags;
            _isOpen = other._isOpen;
            _inDrag = other._inDrag;
            _visible = other._visible;
        }
        return *this;
    }

    static void renderPopups();
    static PopupContext* find(Id id);

    int _level{0};
    Vector2 _position{0, 0};
    RenderTexture _content{};
    int64_t _lastUpdate{0};
    WindowBoxFlags _flags{WBF_NONE};
    bool* _isOpen;          // only valid in the frame given in _lastUpdate, never dereferenced later
    bool _inDrag{false};
    bool _visible{false};   // drawn at the end of the last frame
};

// Only used while the tab view is described, it keeps no state between frames
struct TabViewContext
{
    int *activeTab{nullptr};
    int currentTab{0};
    float tabOffset{0.0f};
    float incX{0};
    float incY{0};
};

struct TableContext
{
    bool lockedGui{false};
    std::vector<float> columnWidth;
    float curWidth{0};
    float curHeight{0};
    float curRowHeight{0};
    int curRow{0};
    int curColumn{0};
    Vector2* scroll{nullptr};
    int64_t lastUpdate{0};
    static TableContext& getContext(Id id);
};

struct GuiContext
{
    enum Type { ctROOT, ctGROUP, ctCOLUMNS, ctPANEL, ctGROUPBOX, ctTABVIEW, ctTAB, ctSCROLLPANEL, ctTABLEVIEW, ctPOPUP, ctWINDOWBOX };
    using ContextData = std::variant<std::monostate, RenderTexture*, TableContext*, TabViewContext*>;
    Type type;
    Vector2 initialPos{};
    Vector2 currentPos{};
    Rectangle area{};
    Rectangle content{};
    Rectangle lastWidgetRect{};
    bool horizontal{false};
    bool bordered{false};
    int level{0};
    Vector2 maxSize{};
    float rowHeight{DEFAULT_ROW_HEIGHT};
    float nextWidth{-1};
    float nextHeight{-1};
    float defaultWidth{-1};
    float spacingH{15};
    float spacingV{4};
    float indent{0};
    float reserve{0};
    Vector2 padding{0, 0};
    Vector2 mouseOffset{0, 0};
    Vector2 scrollOffset{0, 0};
    std::string groupName;
    ContextData contextData;
    size_t idStackSize{0};    // size of the id stack inside this context, restored by its End*()
    size_t clipStackSize{0};  // size of the clipping stack inside this context, restored by its End*()

    GuiContext& newContext() const;
    void captureStackSizes();

    void increment(Vector2 size)
    {
        auto x = currentPos.x;
        auto y = currentPos.y;
        maxSize.x = std::max(maxSize.x, size.x);
        maxSize.y = std::max(maxSize.y, size.y);
        if (horizontal) {
            currentPos.x += size.x + spacingH;
        }
        else {
            currentPos.y += size.y + spacingV;
        }
        nextWidth = -1;
        nextHeight = -1;
        lastWidgetRect = {x, y, size.x, size.y};
        lastWidgetEditFinished = false;
    }
    Vector2 standardSize(float height = -1) const
    {
        float width = nextWidth > 0 ? nextWidth : (defaultWidth > 0 ? defaultWidth : content.width - currentPos.x + content.x);
        return {width, height > 0 ? height : (nextHeight > 0 ? nextHeight : rowHeight)};
    }
    inline static Vector2 positionLimits{};
    inline static int64_t frameId{0};  // Note: even at 1000fps this would have just overflown when started around the start of the permian (about 300Ma) ;-)
    inline static Vector2 guiScale{1.0f, 1.0f};
    inline static bool lastWidgetEditFinished{false};
    inline static PopupMenu::System popup;
    struct DropdownInfo
    {
        Rectangle rect{};
        int* active{nullptr};       // only valid in the frame given in lastUpdate
        bool directionUp{false};
        bool changed{false};
        std::string text;
        bool editMode{false};
        int64_t lastUpdate{0};
        int64_t lastDraw{0};
        int state{0};
        bool guiDisabled{false};
        unsigned int style[RAYGUI_MAX_PROPS_BASE + RAYGUI_MAX_PROPS_EXTENDED];
    };
    inline static std::unordered_map<Id, DropdownInfo> dropdownBoxes;
    inline static Id openDropdownboxId{0};
    inline static Id editFocusId{0};
    inline static int64_t editFocusFrame{0};  // last frame the focused widget was shown or asked for the focus
    inline static GuiContext* rootContext{nullptr};
    inline static std::string tooltipText;
    inline static Rectangle tooltipParentRect{};
    inline static float tooltipTimer{};
    static bool deferDropdownBox(Rectangle rect, const char* text, int* active, bool directionUp, Id key)
    {
        auto iter = dropdownBoxes.find(key);
        if (iter != dropdownBoxes.end()) {
            iter->second.lastUpdate = frameId;
            iter->second.rect = rect;
            iter->second.active = active;
            for (int i = 0; i < RAYGUI_MAX_PROPS_BASE + RAYGUI_MAX_PROPS_EXTENDED; ++i) {
                iter->second.style[i] = GetStyle(DROPDOWNBOX, i);
            }
            iter->second.guiDisabled = GuiGetState() == STATE_DISABLED;
            return std::exchange(iter->second.changed, false);
        }
        else {
            iter = dropdownBoxes.emplace(key, DropdownInfo{rect, active, directionUp, false, std::string(text), false, frameId, 0, GetState()}).first;
            for (int i = 0; i < RAYGUI_MAX_PROPS_BASE + RAYGUI_MAX_PROPS_EXTENDED; ++i) {
                iter->second.style[i] = GetStyle(DROPDOWNBOX, i);
            }
            iter->second.guiDisabled = GuiGetState() == STATE_DISABLED;
            return false;
        }
    }
    static void closeOpenDropdownBox()
    {
        if (openDropdownboxId) {
            auto iter = dropdownBoxes.find(openDropdownboxId);
            if (iter != dropdownBoxes.end()) {
                iter->second.editMode = false;
                openDropdownboxId = 0;
            }
        }
    }
    static void drawTooltip(Rectangle controlRec, const char* tooltipText)
    {
        if (!guiLocked && (tooltipText != nullptr) && !guiControlExclusiveMode)
        {
            Vector2 textSize = GetTextSize(tooltipText);
            textSize.x += 16;

            controlRec.x -= (textSize.x - controlRec.width) / 2;
            if (controlRec.x < 0) {
                controlRec.x = 0;
            }
            if ((controlRec.x + textSize.x) > positionLimits.x)
                controlRec.x = positionLimits.x - textSize.x;

            DrawRectangleRec({controlRec.x + 3, controlRec.y + controlRec.height + 5, textSize.x, GuiGetStyle(DEFAULT, TEXT_SIZE) + 8.f }, {0, 0, 0, 64});
            GuiPanel(RAYGUI_CLITERAL(Rectangle){ controlRec.x, controlRec.y + controlRec.height + 2, textSize.x, GuiGetStyle(DEFAULT, TEXT_SIZE) + 8.f }, NULL);

            int textPadding = GuiGetStyle(LABEL, TEXT_PADDING);
            int textAlignment = GuiGetStyle(LABEL, TEXT_ALIGNMENT);
            GuiSetStyle(LABEL, TEXT_PADDING, 0);
            GuiSetStyle(LABEL, TEXT_ALIGNMENT, TEXT_ALIGN_CENTER);
            GuiLabel(RAYGUI_CLITERAL(Rectangle){ controlRec.x, controlRec.y + controlRec.height + 2, textSize.x, GuiGetStyle(DEFAULT, TEXT_SIZE) + 8.f }, tooltipText);
            GuiSetStyle(LABEL, TEXT_ALIGNMENT, textAlignment);
            GuiSetStyle(LABEL, TEXT_PADDING, textPadding);
        }
    }
    static void handleDeferredDropBox(uint64_t key, DropdownInfo& info)
    {
        if (info.lastDraw < info.lastUpdate && info.lastUpdate == frameId) {
            for (int i = 0; i < RAYGUI_MAX_PROPS_BASE + RAYGUI_MAX_PROPS_EXTENDED; ++i) {
                SetStyle(DROPDOWNBOX, i, info.style[i]);
            }
            auto oldState = GuiGetState();
            auto oldActive = *info.active;
            if(info.guiDisabled)
                GuiDisable();
            if (info.directionUp ? GuiDropupBox(info.rect, info.text.c_str(), info.active, info.editMode) : GuiDropdownBox(info.rect, info.text.c_str(), info.active, info.editMode)) {
                if (openDropdownboxId != key) {
                    closeOpenDropdownBox();
                }
                info.editMode = !info.editMode;
                openDropdownboxId = info.editMode ? key : 0;
            }
            // reported by the next DropdownBox() call, as the box is drawn deferred at EndGui()
            info.changed = info.changed || *info.active != oldActive;
            GuiSetState(oldState);
            info.lastDraw = info.lastUpdate;
        }
    }
    static void handleDeferredDropBoxes()
    {
        for (auto& [key, info] : dropdownBoxes) {
            if(!info.editMode) {
                handleDeferredDropBox(key, info);
            }
        }
        for (auto& [key, info] : dropdownBoxes) {
            if(info.editMode) {
                handleDeferredDropBox(key, info);
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
            if(GuiContext::tooltipTimer >= 0.0f) {
                drawTooltip(tooltipParentRect, tooltipText.c_str());
            }
        }
    }
};

// The state kept between frames is stored by id, an entry not used in a frame is removed in the next BeginGui()
static std::deque<GuiContext> g_contextStack;
static std::unordered_map<Id, PopupContext> g_popupMap;
static PopupContext* g_popupUnderMouse{nullptr};
static std::unordered_map<Id, TableContext> g_tableContextMap;

struct MenuBarState
{
    int openAtFrameStart{-1};   // index of the entry whose menu was open at the end of the last frame
    int64_t lastUpdate{0};
};
static std::unordered_map<Id, MenuBarState> g_menuBarMap;
static std::deque<TabViewContext> g_tabViewStack;
static std::vector<Id> g_idStack;
static std::stack<Rectangle> g_clippingStack;
static std::vector<RenderTexture*> g_renderTargets;

namespace detail {

static constexpr Id ROOT_ID = 0xcbf29ce484222325ULL;  // FNV-1a 64 bit offset basis

Id idScope()
{
    return g_idStack.empty() ? ROOT_ID : g_idStack.back();
}

// 0 is never returned, it means "no id" (e.g. no keyboard focus)
Id hashId(const void* data, size_t size, Id seed)
{
    auto hash = fnv_64a_buf(data, size, seed);
    return hash ? hash : 1;
}

// The terminating zero is hashed too, so the scopes "a" + "b" don't give the same id as "ab"
Id hashId(const char* str, Id seed)
{
    if (!str)
        str = "";
    return hashId(str, std::strlen(str) + 1, seed);
}

// Api misuse asserts in debug builds, in release builds it is logged once per message and the caller repairs the state
void reportError(const char* message)
{
    static std::unordered_set<std::string> reported;
    if (reported.insert(message).second) {
        TraceLog(LOG_ERROR, "RLGUIPP: %s (only reported once)", message);
    }
    RLGUIPP_ASSERT(!"rlguipp api misuse, see the logged error");
}

// The names of the Begin*()/End*() functions of a context type, for error messages
std::pair<const char*, const char*> functionNames(GuiContext::Type type)
{
    switch (type) {
        case GuiContext::ctROOT: return {"BeginGui", "EndGui"};
        case GuiContext::ctGROUP: return {"Begin", "End"};
        case GuiContext::ctCOLUMNS: return {"BeginColumns", "EndColumns"};
        case GuiContext::ctPANEL: return {"BeginPanel", "EndPanel"};
        case GuiContext::ctGROUPBOX: return {"BeginGroupBox", "EndGroupBox"};
        case GuiContext::ctTABVIEW: return {"BeginTabView", "EndTabView"};
        case GuiContext::ctTAB: return {"BeginTab", "EndTab"};
        case GuiContext::ctSCROLLPANEL: return {"BeginScrollPanel", "EndScrollPanel"};
        case GuiContext::ctTABLEVIEW: return {"BeginTableView", "EndTableView"};
        case GuiContext::ctPOPUP: return {"BeginPopup", "EndPopup"};
        case GuiContext::ctWINDOWBOX: return {"BeginWindowBox", "EndWindowBox"};
    }
    return {"?", "?"};
}

// Closing a context restores the id and clipping stacks to their size at its start
void repairStacks(const GuiContext& ctx)
{
    const auto* endName = functionNames(ctx.type).second;
    if (g_idStack.size() != ctx.idStackSize) {
        reportError(TextFormat("PopId() missing before %s()", endName));
        g_idStack.resize(ctx.idStackSize);
    }
    if (g_clippingStack.size() != ctx.clipStackSize) {
        reportError(TextFormat("EndClipping() missing before %s()", endName));
        while (g_clippingStack.size() > ctx.clipStackSize) {
            g_clippingStack.pop();
        }
    }
}

void closeContext();
void endContext(GuiContext::Type type);

// A label that is also an id can have a part that is not shown: "Text##id" shows "Text" and
// uses the whole label as id, "Text###id" uses only "###id", so the text can change without changing the id
class LabelId
{
public:
    explicit LabelId(const char* label)
    {
        const char* separator = label ? std::strstr(label, "##") : nullptr;
        if (separator) {
            _storage.assign(label, separator);
            _text = _storage.c_str();
            _id = hashId(separator[2] == '#' ? separator : label, idScope());
        }
        else {
            _text = label;
            _id = hashId(label, idScope());
        }
    }
    LabelId(const LabelId&) = delete;
    LabelId& operator=(const LabelId&) = delete;
    Id id() const { return _id; }
    const char* text() const { return _text; }

private:
    std::string _storage;
    const char* _text{nullptr};
    Id _id{0};
};

}  // namespace detail

inline void GuiContext::captureStackSizes()
{
    idStackSize = g_idStack.size();
    clipStackSize = g_clippingStack.size();
}

inline GuiContext& GuiContext::newContext() const
{
    g_contextStack.push_back(*this);
    auto& ctx = g_contextStack.back();
    ctx.maxSize = {};
    ctx.captureStackSizes();
    return ctx;
}

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
    // Popups are now drawn ordered by level (map order within a level)
    std::vector<PopupContext*> visible;
    for (auto& [id, popup] : g_popupMap) {
        // _isOpen may only be dereferenced in the frame it was given
        popup._visible = popup._lastUpdate >= GuiContext::frameId && *popup._isOpen;
        if (popup._visible) {
            visible.push_back(&popup);
        }
    }
    std::ranges::stable_sort(visible, {}, &PopupContext::level);
    for (auto* popup : visible) {
        if((popup->_flags & WBF_MODAL) && GuiContext::rootContext)
            DrawRectangle(GuiContext::rootContext->area.x, GuiContext::rootContext->area.y, GuiContext::rootContext->area.width, GuiContext::rootContext->area.height, {0,0,0,128});
        popup->render();
    }
}

PopupContext* PopupContext::find(Id id)
{
    auto iter = g_popupMap.find(id);
    if (iter != g_popupMap.end()) {
        iter->second._lastUpdate = GuiContext::frameId;
        return &iter->second;
    }
    return nullptr;
}

TableContext& TableContext::getContext(Id id)
{
    auto iter = g_tableContextMap.find(id);
    if (iter == g_tableContextMap.end()) {
        iter = g_tableContextMap.emplace(std::make_pair(id, TableContext())).first;
    }
    return iter->second;
}

namespace detail {

// Remove the state of everything that was not used in the last frame, the render textures of popups are unloaded with it
void pruneUnusedState(RenderTexture* renderTexture)
{
    const auto lastFrame = GuiContext::frameId;
    if (!g_popupMap.empty()) {
        if (renderTexture)
            EndTextureMode();
        std::erase_if(g_popupMap, [lastFrame](const auto& item) { return item.second._lastUpdate < lastFrame; });
        if (renderTexture)
            BeginTextureMode(*renderTexture);
    }
    std::erase_if(g_tableContextMap, [lastFrame](const auto& item) { return item.second.lastUpdate < lastFrame; });
    std::erase_if(g_menuBarMap, [lastFrame](const auto& item) { return item.second.lastUpdate < lastFrame; });
    std::erase_if(GuiContext::dropdownBoxes, [lastFrame](const auto& item) { return item.second.lastUpdate < lastFrame; });
    if (GuiContext::openDropdownboxId && !GuiContext::dropdownBoxes.contains(GuiContext::openDropdownboxId)) {
        GuiContext::openDropdownboxId = 0;
    }
    if (GuiContext::editFocusId && GuiContext::editFocusFrame < lastFrame) {
        GuiContext::editFocusId = 0;
    }
}

void updatePopupUnderMouse()
{
    g_popupUnderMouse = nullptr;
    if (!g_popupMap.empty()) {
        auto mouse = GetMousePosition();
        for (auto& [id, popup] : g_popupMap) {
            if (popup._visible) {
                if ((popup._flags & WBF_MODAL) || CheckCollisionPointRec(mouse, popup.bounds())) {
                    if (!g_popupUnderMouse || g_popupUnderMouse->level() <= popup.level()) {
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
        reportError("gui functions must be called between BeginGui() and EndGui()");
        static GuiContext dummy{GuiContext::ctROOT};
        dummy = GuiContext{GuiContext::ctROOT};
        return dummy;
    }
    return g_contextStack.back();
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
typename std::invoke_result<Func, Rectangle, Args..., bool>::type editableWidget(Func fp, Id id, Args&&... args)
{
    auto& ctx = detail::context();
    auto size = ctx.standardSize();
    auto editMode = HasKeyboardFocus(id);
    auto rc = (*fp)({ctx.currentPos.x + ctx.scrollOffset.x, ctx.currentPos.y + ctx.scrollOffset.y, size.x, size.y}, std::forward<Args>(args)..., editMode);
    if (rc) {
        SetKeyboardFocus(editMode ? 0 : id);
    }
    ctx.increment(size);
    GuiContext::lastWidgetEditFinished = rc && editMode;
    return rc;
}

}  // namespace detail

void SetKeyboardFocus(Id id)
{
    GuiContext::editFocusId = id;
    GuiContext::editFocusFrame = GuiContext::frameId;
}

bool HasKeyboardFocus(Id id)
{
    if (id && id == GuiContext::editFocusId) {
        GuiContext::editFocusFrame = GuiContext::frameId;
        return true;
    }
    return false;
}

void BeginGui(Rectangle area, RenderTexture* renderTexture, Vector2 mouseOffset, Vector2 guiScale)
{
    GuiContext::positionLimits = {area.x + area.width, area.y + area.height};
    if (area.width <= 0) {
        area.width = renderTexture ? renderTexture->texture.width : GetScreenWidth();
        GuiContext::positionLimits.x = area.width;
    }
    if (area.height <= 0) {
        area.height = renderTexture ? renderTexture->texture.height : GetScreenHeight();
        GuiContext::positionLimits.y = area.height;
    }

    if (!g_contextStack.empty()) {
        detail::reportError("BeginGui() without EndGui() of the last one");
        while (g_contextStack.size() > 1) {
            detail::closeContext();
        }
        g_contextStack.clear();
    }
    if (!g_idStack.empty()) {
        detail::reportError("PopId() missing before BeginGui()");
        g_idStack.clear();
    }
    g_contextStack.push_back({GuiContext::ctROOT, {area.x, area.y}, {area.x, area.y}, area, area});
    g_contextStack.back().mouseOffset = mouseOffset;
    g_contextStack.back().captureStackSizes();
    g_renderTargets.assign(1, renderTexture);
    SetMouseOffset(mouseOffset.x, mouseOffset.y);

    g_tabViewStack.clear();
    detail::pruneUnusedState(renderTexture);
    detail::updatePopupUnderMouse();
    if ((GuiContext::openDropdownboxId || g_popupUnderMouse) && !GuiIsLocked()) {
        TraceLog(LOG_DEBUG, "GUI is locked!");
        GuiLock();
    }
    else if (!(GuiContext::openDropdownboxId || g_popupUnderMouse) && GuiIsLocked()) {
        TraceLog(LOG_DEBUG, "GUI is unlocked!");
        GuiUnlock();
    }
    g_contextStack.back().contextData = renderTexture;
    GuiContext::tooltipText.clear();
    ++GuiContext::frameId;
    GuiContext::guiScale = guiScale;
    GuiContext::popup.update(GetFrameTime());
    GuiContext::rootContext = &g_contextStack.back();
}

void EndGui()
{
    if (g_contextStack.empty()) {
        detail::reportError("EndGui() without matching BeginGui()");
        return;
    }
    while (g_contextStack.size() > 1) {
        detail::reportError(TextFormat("%s() missing before EndGui()", detail::functionNames(g_contextStack.back().type).second));
        detail::closeContext();
    }
    detail::repairStacks(g_contextStack.back());
    GuiContext::handleDeferredDropBoxes();
    PopupContext::renderPopups();
    GuiContext::popup.render();
    g_contextStack.clear();
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
    g_popupMap.clear();
    g_popupUnderMouse = nullptr;
    g_tableContextMap.clear();
    GuiContext::dropdownBoxes.clear();
    GuiContext::openDropdownboxId = 0;
    GuiContext::editFocusId = 0;
}

void PushId(const char* str)
{
    g_idStack.push_back(detail::hashId(str, detail::idScope()));
}

void PushId(int value)
{
    g_idStack.push_back(detail::hashId(&value, sizeof(value), detail::idScope()));
}

void PushId(const void* ptr)
{
    g_idStack.push_back(detail::hashId(&ptr, sizeof(ptr), detail::idScope()));
}

void PopId()
{
    // an id pushed outside of the current context can only be popped after its End*()
    auto minSize = g_contextStack.empty() ? 0 : g_contextStack.back().idStackSize;
    if (g_idStack.size() <= minSize) {
        detail::reportError("PopId() without matching PushId()");
        return;
    }
    g_idStack.pop_back();
}

Id GetId(const char* label)
{
    return detail::hashId(label, detail::idScope());
}

void Begin()
{
    auto& ctxParent = detail::context();
    auto& ctx = ctxParent.newContext();
    ctx.type = GuiContext::ctGROUP;
    ctx.area = {ctxParent.currentPos.x, ctxParent.currentPos.y, ctx.standardSize().x, ctxParent.content.height + ctxParent.content.y - ctxParent.currentPos.y};
    ctx.content = ctx.area;
    ctx.initialPos = ctx.currentPos;
    ctx.horizontal = false;
    ctx.bordered = false;
    ctx.level++;
    ctx.nextWidth = ctx.nextHeight = ctx.defaultWidth = -1;
}

static void EndImpl(Vector2 size = {})
{
    auto ctxOld = g_contextStack.back();
    g_contextStack.pop_back();
    auto& ctx = detail::context();
    if (size.x > 0 && size.y > 0) {
        ctx.increment(size);
    }
    else if (ctxOld.horizontal) {
        ctx.increment({ctxOld.currentPos.x - ctxOld.area.x + ctxOld.padding.x - ctxOld.spacingH, ctxOld.maxSize.y + ctxOld.padding.y*2});
    }
    else {
        ctx.increment({ctxOld.maxSize.x + ctxOld.padding.x*2, ctxOld.currentPos.y - ctxOld.area.y + ctxOld.padding.y - ctxOld.spacingV});
    }
}

void End()
{
    detail::endContext(GuiContext::ctGROUP);
}

void BeginColumns()
{
    Begin();
    detail::context().type = GuiContext::ctCOLUMNS;
    detail::context().horizontal = true;
}

static void EndColumnsImpl()
{
    auto& ctx = g_contextStack.back();
    EndImpl({ctx.currentPos.x - ctx.area.x, ctx.maxSize.y});
}

void EndColumns()
{
    detail::endContext(GuiContext::ctCOLUMNS);
}

void BeginPanel(const char* text, Vector2 padding)
{
    auto& ctxParent = detail::context();
    auto& ctx = ctxParent.newContext();
    auto size = ctx.standardSize();
    ctx.type = GuiContext::ctPANEL;
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
    ctx.nextWidth = ctx.nextHeight = ctx.defaultWidth = -1;
    ctx.padding = padding;
}

static void EndPanelImpl()
{
    auto& ctx = g_contextStack.back();
    if (ctx.level > 1) {
        GuiDrawRectangle({ctx.area.x, ctx.area.y, ctx.area.width, ctx.currentPos.y - ctx.area.y + ctx.padding.y - ctx.spacingV}, 1, Fade(GetColor(gui::GetStyle(DEFAULT, LINE_COLOR)), guiAlpha), {0, 0, 0, 0});
    }
    else {
        GuiDrawRectangle({ctx.area.x, ctx.area.y, ctx.area.width, ctx.area.height}, 1, Fade(GetColor(gui::GetStyle(DEFAULT, LINE_COLOR)), guiAlpha), {0, 0, 0, 0});
    }
    EndImpl();
}

void EndPanel()
{
    detail::endContext(GuiContext::ctPANEL);
}

void BeginTabView(int *activeTab)
{
    auto& ctxParent = detail::context();
    auto& ctx = ctxParent.newContext();
    auto size = ctx.standardSize();
    ctx.type = GuiContext::ctTABVIEW;
    auto& tvc = g_tabViewStack.emplace_back();
    tvc.activeTab = activeTab;
    tvc.currentTab = 0;
    tvc.tabOffset = 2.0f;
    // the size of the tab bar alone, used if there is no active tab
    tvc.incX = size.x;
    tvc.incY = ctx.rowHeight;
    ctx.contextData = &tvc;
    GuiStatusBar({ctxParent.currentPos.x, ctxParent.currentPos.y, size.x, ctx.rowHeight}, " ");
}

static void EndTabViewImpl()
{
    auto ctxOld = g_contextStack.back();
    g_contextStack.pop_back();
    auto& ctxNew = detail::context();
    auto& tvc = *std::get<TabViewContext*>(ctxOld.contextData);
    ctxNew.increment({tvc.incX, tvc.incY});
    g_tabViewStack.pop_back();
}

void EndTabView()
{
    detail::endContext(GuiContext::ctTABVIEW);
}

bool BeginTab(const char* label, Vector2 padding)
{
    auto& ctxParent = detail::context();
    if (ctxParent.type != GuiContext::ctTABVIEW) {
        detail::reportError("BeginTab() outside of BeginTabView()/EndTabView()");
        return false;
    }
    auto& tvc = *std::get<TabViewContext*>(ctxParent.contextData);
    detail::LabelId labelId(label);
    auto* text = labelId.text();
    auto labelSize = GetTextSize(text);
    bool isActive = *tvc.activeTab == tvc.currentTab;
    bool hoversOverTab = !GuiIsLocked() && CheckCollisionPointRec(GetMousePosition(), {ctxParent.currentPos.x + tvc.tabOffset + 1, ctxParent.currentPos.y + 3, labelSize.x + 4, ctxParent.rowHeight - 4});
    // Inactive tab text is derived from the normal text colour at half opacity
    // rather than TEXT_COLOR_DISABLED: raygui propagates DEFAULT base-property
    // writes to all controls, so any per-TEXTBOX override is unreliable.
    Color textcolBase = GetColor(gui::GetStyle(TEXTBOX, TEXT + (guiState * 3)));
    auto textcol = (isActive || hoversOverTab)
        ? Fade(textcolBase, guiAlpha)
        : Fade(textcolBase, guiAlpha * 0.45f);
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
    auto& ctx = ctxParent.newContext();
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
    ctx.nextWidth = ctx.nextHeight = ctx.defaultWidth = -1;
    ctx.padding = padding;
    g_idStack.push_back(labelId.id());
    ctx.captureStackSizes();
    return true;
}

static void EndTabImpl()
{
    auto& ctx = g_contextStack.back();
    auto ctxOld = ctx;
    if (ctx.level > 1) {
        GuiDrawRectangle({ctx.area.x, ctx.area.y, ctx.area.width, ctx.currentPos.y - ctx.area.y + ctx.padding.y}, 1, Fade(GetColor(gui::GetStyle(DEFAULT, LINE_COLOR)), guiAlpha), {0, 0, 0, 0});
    }
    else {
        GuiDrawRectangle({ctx.area.x, ctx.area.y, ctx.area.width, ctx.area.height}, 1, Fade(GetColor(gui::GetStyle(DEFAULT, LINE_COLOR)), guiAlpha), {0, 0, 0, 0});
    }
    g_contextStack.pop_back();
    g_idStack.pop_back();
    auto& ctxParent = detail::context();
    ctxOld.maxSize = {ctxOld.area.width, ctxOld.area.height};
    auto& tvc = *std::get<TabViewContext*>(ctxParent.contextData);
    if (ctxOld.horizontal) {
        tvc.incX = ctxOld.currentPos.x - ctxOld.area.x;
        tvc.incY = ctxOld.maxSize.y;
    }
    else {
        tvc.incX = ctxOld.maxSize.x;
        tvc.incY = ctxOld.currentPos.y - ctxOld.area.y;
    }
}

void EndTab()
{
    detail::endContext(GuiContext::ctTAB);
}

void BeginScrollPanel(float height, Rectangle content, Vector2 *scroll)
{
    auto& ctxParent = detail::context();
    auto& ctx = ctxParent.newContext();
    auto size = ctx.standardSize();
    ctx.type = GuiContext::ctSCROLLPANEL;
    ctx.area = {ctxParent.currentPos.x, ctxParent.currentPos.y, size.x, height > 0 ? height : ctxParent.content.height + ctxParent.content.y - ctxParent.currentPos.y};
    ctx.content = {0, 0, content.width, content.height};
    ctx.initialPos = {5, 5};
    ctx.currentPos = ctx.initialPos;
    ctx.horizontal = false;
    ctx.bordered = false;
    ctx.level = 0;
    ctx.nextWidth = ctx.nextHeight = ctx.defaultWidth = -1;
    ctx.padding = {0, 0};

    Rectangle view;
    GuiScrollPanel(ctx.area, NULL, ctx.content, scroll, &view);
    ctx.scrollOffset = {ctx.area.x + scroll->x, ctx.area.y + scroll->y};
    bool hasVerticalScrollBar = (content.height > ctx.area.height - 2*GuiGetStyle(DEFAULT, BORDER_WIDTH))? true : false;
    view = {
        ctx.area.x + GuiGetStyle(DEFAULT, BORDER_WIDTH),
        ctx.area.y + GuiGetStyle(DEFAULT, BORDER_WIDTH),
        ctx.area.width - 2*GuiGetStyle(DEFAULT, BORDER_WIDTH) - (hasVerticalScrollBar ? GuiGetStyle(LISTVIEW, SCROLLBAR_WIDTH) : 0.0f),
        ctx.area.height - 2*GuiGetStyle(DEFAULT, BORDER_WIDTH)
    };
    BeginClipping(view);
    ctx.captureStackSizes();
    ctx.mouseOffset = ctxParent.mouseOffset;
}

static void EndScrollPanelImpl()
{
    g_clippingStack.pop();
    auto& ctx = g_contextStack.back();
    EndImpl({ctx.area.width, ctx.area.height});
}

void EndScrollPanel()
{
    detail::endContext(GuiContext::ctSCROLLPANEL);
}

void BeginTableView(float height, const char* id, std::initializer_list<TableColumn> columns, Vector2 *scroll)
{
    auto tableId = GetId(id);
    auto& tc = TableContext::getContext(tableId);
    auto& ctxParent = detail::context();
    auto areaWidth = ctxParent.standardSize().x;
    auto areaHeight = height > 0 ? height : ctxParent.content.height + ctxParent.content.y - ctxParent.currentPos.y;
    auto contentHeight = tc.curHeight;
    if (!tc.lastUpdate && scroll->y < 0) {
        // The content height is only known after the first frame, until then make it large enough
        // for the scroll position, so the scroll panel doesn't reset it
        contentHeight = -scroll->y + areaHeight;
    }
    // Stretch columns share the width of the scroll panel's view that the fixed columns leave,
    // the view is reduced by the vertical scrollbar the same way BeginScrollPanel() does it
    auto borderWidth = (float)GuiGetStyle(DEFAULT, BORDER_WIDTH);
    bool hasVerticalScrollBar = contentHeight + 4 > areaHeight - 2 * borderWidth;
    auto viewWidth = areaWidth - 2 * borderWidth - (hasVerticalScrollBar ? (float)GuiGetStyle(LISTVIEW, SCROLLBAR_WIDTH) : 0.0f);
    float fixedWidth = 0;
    float stretchWeight = 0;
    for (const auto& column : columns) {
        if (column.kind == TableColumn::FIXED)
            fixedWidth += column.value;
        else
            stretchWeight += column.value;
    }
    auto stretchWidth = std::max(0.0f, viewWidth - fixedWidth);
    tc.columnWidth.clear();
    float contentWidth = 0;
    float stretchUsed = 0;
    for (const auto& column : columns) {
        float width = column.value;
        if (column.kind == TableColumn::STRETCH && stretchWeight > 0) {
            stretchUsed += column.value;
            // the rounding difference goes to the next stretch column, so together they fill the width exactly
            width = std::floor(stretchWidth * stretchUsed / stretchWeight) - std::floor(stretchWidth * (stretchUsed - column.value) / stretchWeight);
        }
        tc.columnWidth.push_back(width);
        contentWidth += width;
    }
    if (!tc.lastUpdate && scroll->x < 0) {
        contentWidth = std::max(contentWidth, -scroll->x + areaWidth);
    }
    tc.lastUpdate = GuiContext::frameId;
    tc.scroll = scroll;
    tc.curRow = -1;
    tc.curColumn = -1;
    tc.curRowHeight = 0;
    tc.curWidth = 0;
    tc.curHeight = 0;
    BeginScrollPanel(height, {0, 0, contentWidth, contentHeight+4}, scroll);
    detail::context().type = GuiContext::ctTABLEVIEW;
    detail::context().contextData = &tc;
    if(!CheckCollisionPointRec(GetMousePosition(), detail::context().area) && !GuiIsLocked()) {
        GuiLock();
        tc.lockedGui = true;
    }
    // TableView has its own scroll handling for cells, so reset scroll offset
    detail::context().scrollOffset = {0,0};
    g_idStack.push_back(tableId);
    detail::context().captureStackSizes();
}

bool TableNextRow(float height, Color background)
{
    auto& ctx = detail::context();
    if(!std::holds_alternative<gui::TableContext*>(ctx.contextData)) {
        detail::reportError("TableNextRow() outside of BeginTableView()/EndTableView()");
        return false;
    }
    auto& tc = *std::get<TableContext*>(ctx.contextData);
    ++tc.curRow;
    tc.curColumn = -1;
    tc.curRowHeight = height;
    tc.curHeight += height;
    tc.curWidth = 0;
    ctx.content = {ctx.area.x, ctx.area.y + 2 + tc.curHeight - tc.curRowHeight + tc.scroll->y, ctx.area.width, tc.curRowHeight};
    bool result = CheckCollisionRecs(ctx.area, ctx.content);
    if(result && background.a) {
        DrawRectangleClipped(ctx.area.x, ctx.area.y + 2 + tc.curHeight - tc.curRowHeight + tc.scroll->y, ctx.area.width, tc.curRowHeight, background);
    }
    return result;
}

bool TableNextRow(float height, const std::function<void(Rectangle rect)>& handler)
{
    auto result = TableNextRow(height);
    if (result && handler) {
        auto& ctx = detail::context();
        handler(ctx.content);
    }
    return result;
}

bool TableNextColumn()
{
    auto& ctx = detail::context();
    if(!std::holds_alternative<gui::TableContext*>(ctx.contextData)) {
        detail::reportError("TableNextColumn() outside of BeginTableView()/EndTableView()");
        return false;
    }
    auto& tc = *std::get<TableContext*>(ctx.contextData);
    if (tc.curColumn + 1 >= (int)tc.columnWidth.size()) {
        detail::reportError("TableNextColumn() called more often than the table has columns");
        return false;
    }
    auto width = tc.columnWidth[++tc.curColumn];
    tc.curWidth += width;
    ctx.content = {ctx.area.x + tc.curWidth - width + tc.scroll->x, ctx.area.y + 2 + tc.curHeight - tc.curRowHeight + tc.scroll->y, width, tc.curRowHeight};
    ctx.initialPos = ctx.currentPos = {ctx.content.x + 2, ctx.content.y + 2};
    return CheckCollisionRecs(ctx.area, ctx.content);
}

void TableNextColumn(const std::function<void(Rectangle rect)>& handler)
{
    if (TableNextColumn()) {
        auto& ctx = detail::context();
        BeginClipping(ctx.content);
        handler(ctx.content);
        EndClipping();
    }
}

static void EndTableViewImpl()
{
    auto& tc = *std::get<TableContext*>(g_contextStack.back().contextData);
    g_idStack.pop_back();
    EndScrollPanelImpl();
    if(tc.lockedGui) {
        tc.lockedGui = false;
        GuiUnlock();
    }
}

void EndTableView()
{
    detail::endContext(GuiContext::ctTABLEVIEW);
}

void BeginGroupBox(const char* text)
{
    auto& ctxParent = detail::context();
    auto& ctx = ctxParent.newContext();
    auto size = ctx.standardSize();
    ctx.type = GuiContext::ctGROUPBOX;
    ctx.area = {ctxParent.currentPos.x, ctxParent.currentPos.y, size.x, ctxParent.area.height + ctxParent.area.y - ctxParent.currentPos.y};
    ctx.content = {ctx.area.x + 5, ctx.area.y + ctx.rowHeight * 2 / 3, ctx.area.width - 10, ctx.area.height - ctx.rowHeight * 2 / 3};
    ctx.initialPos = {ctx.content.x, ctx.content.y};
    ctx.currentPos = ctx.initialPos;
    ctx.horizontal = false;
    ctx.bordered = true;
    ctx.level++;
    ctx.groupName = text ? text : "";
    ctx.nextWidth = ctx.nextHeight = ctx.defaultWidth = -1;
    ctx.padding = {0, 0};
}

static void EndGroupBoxImpl()
{
    auto& ctx = g_contextStack.back();
    GuiGroupBox({ctx.area.x, ctx.area.y + 8, ctx.area.width, ctx.currentPos.y - ctx.area.y - 4}, ctx.groupName.c_str());
    ctx.increment({0, ctx.spacingV});
    EndImpl();
}

void EndGroupBox()
{
    detail::endContext(GuiContext::ctGROUPBOX);
}

namespace detail {

// Returns the state of the popup if it is open, its content is described until the matching EndPopup()
PopupContext* beginPopup(Rectangle area, Id id, bool* isOpen)
{
    if (!*isOpen) {
        return nullptr;
    }
    if(GuiContext::rootContext) {
        const auto* root = GuiContext::rootContext;
        if(area.x < 0)
            area.x = (root->area.width - area.width) / 2.0f;
        if(area.y < 0)
            area.y = (root->area.height - area.height) / 2.0f;
    }
    auto& ctxParent = detail::context();
    auto& ctx = ctxParent.newContext();
    ctx.type = GuiContext::ctPOPUP;
    ctx.area = {0, 0, area.width, area.height};
    ctx.content = ctx.area;
    ctx.initialPos = {0, 0};
    ctx.currentPos = ctx.initialPos;
    ctx.horizontal = false;
    ctx.bordered = false;
    ctx.level = 0;
    ctx.nextWidth = ctx.nextHeight = ctx.defaultWidth = -1;
    ctx.padding = {0, 0};

    if (g_renderTargets.back()) {
        EndTextureMode();
    }
    auto* popup = PopupContext::find(id);
    if (!popup) {
        popup = &g_popupMap.emplace(id, PopupContext{area, isOpen}).first->second;
    }
    popup->_isOpen = isOpen;
    ctx.mouseOffset = {ctxParent.mouseOffset.x - popup->position().x * GuiContext::guiScale.x, ctxParent.mouseOffset.y - popup->position().y * GuiContext::guiScale.y};
    ctx.contextData = &popup->texture();
    g_renderTargets.push_back(&popup->texture());
    BeginTextureMode(popup->texture());
    GuiDrawRectangle({0, 0, area.width, area.height}, 1, GetColor(GetStyle(DEFAULT, BORDER_COLOR_NORMAL)), GetColor(GetStyle(DEFAULT, BACKGROUND_COLOR)));
    SetMouseOffset((int)ctx.mouseOffset.x, (int)ctx.mouseOffset.y);
    if (g_popupUnderMouse == popup && GuiIsLocked()) {
        GuiUnlock();
    }
    g_idStack.push_back(id);
    ctx.captureStackSizes();
    return popup;
}

}  // namespace detail

bool BeginPopup(Rectangle area, const char* id, bool* isOpen)
{
    return detail::beginPopup(area, GetId(id), isOpen) != nullptr;
}

static void EndPopupImpl()
{
    if (g_popupUnderMouse && !GuiIsLocked()) {
        GuiLock();
    }
    EndTextureMode();
    g_contextStack.pop_back();
    g_idStack.pop_back();
    g_renderTargets.pop_back();
    auto& ctx = detail::context();
    SetMouseOffset((int)ctx.mouseOffset.x, (int)ctx.mouseOffset.y);
    if (g_renderTargets.back()) {
        BeginTextureMode(*g_renderTargets.back());
    }
}

void EndPopup()
{
    detail::endContext(GuiContext::ctPOPUP);
}

bool BeginWindowBox(Rectangle area, const char* label, bool* isOpen, WindowBoxFlags flags)
{
    auto mouseParent = detail::context().mouseOffset;
    detail::LabelId labelId(label);
    auto* title = labelId.text();
    auto* popup = detail::beginPopup(area, labelId.id(), isOpen);
    if (!popup) {
        return false;
    }
    popup->_flags = flags;
    auto& ctx = detail::context();
    ctx.type = GuiContext::ctWINDOWBOX;
    if (flags & WBF_CLOSABLE) {
        if (GuiWindowBox({0, 0, area.width, area.height}, title)) {
            *isOpen = false;
            EndPopupImpl();
            return false;
        }
    }
    else {
        // same frame as GuiWindowBox, just without the close button
        GuiStatusBar({0, 0, area.width, RAYGUI_WINDOWBOX_STATUSBAR_HEIGHT}, title);
        GuiPanel({0, RAYGUI_WINDOWBOX_STATUSBAR_HEIGHT - 1, area.width, area.height - RAYGUI_WINDOWBOX_STATUSBAR_HEIGHT + 1}, nullptr);
    }
    if ((flags & WBF_MOVABLE) && !GuiIsLocked() && IsMouseButtonPressed(0) && CheckCollisionPointRec(GetMousePosition(), {0, 0, area.width, RAYGUI_WINDOWBOX_STATUSBAR_HEIGHT})) {
        popup->_inDrag = true;
    }
    else if (popup->_inDrag) {
        auto delta = GetMouseDelta();
        popup->move(delta.x / GuiContext::guiScale.x, delta.y / GuiContext::guiScale.y);
        ctx.mouseOffset = {mouseParent.x - popup->position().x * GuiContext::guiScale.x, mouseParent.y - popup->position().y * GuiContext::guiScale.y};
        SetMouseOffset((int)ctx.mouseOffset.x, (int)ctx.mouseOffset.y);
        if (IsMouseButtonReleased(0)) {
            popup->_inDrag = false;
        }
    }
    ctx.content = {ctx.area.x + 5, ctx.area.y + RAYGUI_WINDOWBOX_STATUSBAR_HEIGHT + 5, ctx.area.width - 10, ctx.area.height - RAYGUI_WINDOWBOX_STATUSBAR_HEIGHT - 10};
    ctx.initialPos = {ctx.content.x, ctx.content.y};
    ctx.currentPos = ctx.initialPos;
    return true;
}

void EndWindowBox()
{
    detail::endContext(GuiContext::ctWINDOWBOX);
}

namespace detail {

// Closes the context on top of the stack, as its End*() would do
void closeContext()
{
    repairStacks(g_contextStack.back());
    switch (g_contextStack.back().type) {
        case GuiContext::ctROOT: break;  // only closed by EndGui()
        case GuiContext::ctGROUP: EndImpl(); break;
        case GuiContext::ctCOLUMNS: EndColumnsImpl(); break;
        case GuiContext::ctPANEL: EndPanelImpl(); break;
        case GuiContext::ctGROUPBOX: EndGroupBoxImpl(); break;
        case GuiContext::ctTABVIEW: EndTabViewImpl(); break;
        case GuiContext::ctTAB: EndTabImpl(); break;
        case GuiContext::ctSCROLLPANEL: EndScrollPanelImpl(); break;
        case GuiContext::ctTABLEVIEW: EndTableViewImpl(); break;
        case GuiContext::ctPOPUP:
        case GuiContext::ctWINDOWBOX: EndPopupImpl(); break;
    }
}

// Called by the End*() functions: closes the innermost context of the given type, contexts that were
// opened inside of it and not closed are closed first, an End*() without matching Begin*() is ignored
void endContext(GuiContext::Type type)
{
    const auto* endName = functionNames(type).second;
    if (std::ranges::none_of(g_contextStack, [type](const GuiContext& ctx) { return ctx.type == type; })) {
        reportError(TextFormat("%s() without matching %s()", endName, functionNames(type).first));
        return;
    }
    while (g_contextStack.back().type != type) {
        reportError(TextFormat("%s() missing before %s()", functionNames(g_contextStack.back().type).second, endName));
        closeContext();
    }
    closeContext();
}

}  // namespace detail

void BeginClipping(const Rectangle& clipArea)
{
    g_clippingStack.push(g_clippingStack.empty() ? clipArea : clipRectangle(g_clippingStack.top(), clipArea));
}

void EndClipping()
{
    // a clipping area started outside of the current context can only be ended after its End*()
    auto minSize = g_contextStack.empty() ? 0 : g_contextStack.back().clipStackSize;
    if (g_clippingStack.size() <= minSize) {
        detail::reportError("EndClipping() without matching BeginClipping()");
        return;
    }
    g_clippingStack.pop();
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
    detail::context().nextWidth = width > 0 ? width : -1;
}

void SetNextRelativeWidth(float fraction)
{
    auto& ctx = detail::context();
    ctx.nextWidth = fraction > 0 ? std::floor(ctx.content.width * fraction) : -1;
}

void SetDefaultWidth(float width)
{
    detail::context().defaultWidth = width > 0 ? width : -1;
}

void SetNextHeight(float height)
{
    detail::context().nextHeight = height > 0 ? height : -1;
}

void SetNextRelativeHeight(float fraction)
{
    auto& ctx = detail::context();
    ctx.nextHeight = fraction > 0 ? std::floor(ctx.content.height * fraction) : -1;
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

std::optional<MouseButton> ButtonMulti(const char* text)
{
    auto& ctx = detail::context();
    auto size = ctx.standardSize();
    Rectangle bounds{ctx.currentPos.x + ctx.scrollOffset.x, ctx.currentPos.y + ctx.scrollOffset.y, size.x, size.y};
    ctx.increment(size);
    std::optional<MouseButton> result{};
    GuiState state = static_cast<GuiState>(GetState());
    if ((state != STATE_DISABLED) && !guiLocked && !guiControlExclusiveMode)
    {
        Vector2 mousePoint = GetMousePosition();
        if (CheckCollisionPointRec(mousePoint, bounds))
        {
            if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) result = MOUSE_BUTTON_LEFT;
            if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT)) result = MOUSE_BUTTON_RIGHT;
            if (IsMouseButtonReleased(MOUSE_BUTTON_MIDDLE)) result = MOUSE_BUTTON_MIDDLE;
            if (result) state = STATE_PRESSED;
            else state = STATE_FOCUSED;
        }
    }
    GuiDrawRectangle(bounds, GuiGetStyle(BUTTON, BORDER_WIDTH), GetColor(GuiGetStyle(BUTTON, BORDER + (state*3))), GetColor(GuiGetStyle(BUTTON, BASE + (state*3))));
    GuiDrawText(text, GetTextBounds(BUTTON, bounds), GuiGetStyle(BUTTON, TEXT_ALIGNMENT), GetColor(GuiGetStyle(BUTTON, TEXT + (state*3))));
    if (state == STATE_FOCUSED) GuiTooltip(bounds);
    return result;
}

bool LabelButton(const char* text)
{
    return detail::defaultWidget(GuiLabelButton, text);
}

bool Toggle(const char* text, bool* active)
{
    auto oldActive = *active;
    detail::defaultWidget(GuiToggle, text, active);
    return *active != oldActive;
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

bool ToggleGroup(const char* text, int* active)
{
    auto& ctx = detail::context();
    short rows = 0;
    short cols = 0;
    CountGuiTextItems(text, rows, cols);
    auto size = ctx.standardSize();
    auto oldActive = *active;
    GuiToggleGroup({ctx.currentPos.x + ctx.scrollOffset.x, ctx.currentPos.y + ctx.scrollOffset.y, size.x, size.y}, text, active);
    ctx.increment({cols * size.x + (cols - 1) * (float)GuiGetStyle(TOGGLE, GROUP_PADDING), rows * size.y + (rows - 1) * (float)GuiGetStyle(TOGGLE, GROUP_PADDING)});
    return *active != oldActive;
}

bool CheckBox(const char* text, bool* checked)
{
    auto& ctx = detail::context();
    auto size = ctx.standardSize();
    auto oldChecked = *checked;
    GuiCheckBox({ctx.currentPos.x + ctx.scrollOffset.x, ctx.currentPos.y + ctx.scrollOffset.y + (ctx.rowHeight - 15) / 2, 15, 15}, text, checked);
    ctx.increment(size);
    return *checked != oldChecked;
}

bool ComboBox(const char* text, int* active)
{
    auto oldActive = *active;
    detail::defaultWidget(GuiComboBox, text, active);
    return *active != oldActive;
}

int GuiDropupBox(Rectangle bounds, const char *text, int *active, bool editMode)
{
    int result = 0;
    GuiState state = guiState;

    int itemSelected = *active;
    int itemFocused = -1;

    // Get substrings items from text (items pointers, lengths and count)
    int itemCount = 0;
    const char **items = GuiTextSplit(text, ';', &itemCount, NULL);

    Rectangle boundsOpen = bounds;
    boundsOpen.height = (itemCount + 1)*(bounds.height + GuiGetStyle(DROPDOWNBOX, DROPDOWN_ITEMS_SPACING));
    boundsOpen.y -= itemCount*(bounds.height + GuiGetStyle(DROPDOWNBOX, DROPDOWN_ITEMS_SPACING));

    Rectangle itemBounds = bounds;

    // Update control
    //--------------------------------------------------------------------
    if ((state != STATE_DISABLED) && (editMode || !guiLocked) && (itemCount > 1) && !guiControlExclusiveMode)
    {
        Vector2 mousePoint = GetMousePosition();

        if (editMode)
        {
            state = STATE_PRESSED;

            // Check if mouse has been pressed or released outside limits
            if (!CheckCollisionPointRec(mousePoint, boundsOpen))
            {
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) || IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) result = 1;
            }

            // Check if already selected item has been pressed again
            if (CheckCollisionPointRec(mousePoint, bounds) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) result = 1;

            // Check focused and selected item
            for (int i = 0; i < itemCount; i++)
            {
                // Update item rectangle y position for next item
                itemBounds.y -= (bounds.height + GuiGetStyle(DROPDOWNBOX, DROPDOWN_ITEMS_SPACING));

                if (CheckCollisionPointRec(mousePoint, itemBounds))
                {
                    itemFocused = i;
                    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON))
                    {
                        itemSelected = i;
                        result = 1;         // Item selected
                    }
                    break;
                }
            }

            itemBounds = bounds;
        }
        else
        {
            if (CheckCollisionPointRec(mousePoint, bounds))
            {
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
                {
                    result = 1;
                    state = STATE_PRESSED;
                }
                else state = STATE_FOCUSED;
            }
        }
    }
    //--------------------------------------------------------------------

    // Draw control
    //--------------------------------------------------------------------
    if (editMode) GuiPanel(boundsOpen, NULL);

    GuiDrawRectangle(bounds, GuiGetStyle(DROPDOWNBOX, BORDER_WIDTH), GetColor(GuiGetStyle(DROPDOWNBOX, BORDER + state*3)), GetColor(GuiGetStyle(DROPDOWNBOX, BASE + state*3)));
    GuiDrawText(items[itemSelected], GetTextBounds(DROPDOWNBOX, bounds), GuiGetStyle(DROPDOWNBOX, TEXT_ALIGNMENT), GetColor(GuiGetStyle(DROPDOWNBOX, TEXT + state*3)));

    if (editMode)
    {
        // Draw visible items
        for (int i = 0; i < itemCount; i++)
        {
            // Update item rectangle y position for next item
            itemBounds.y -= (bounds.height + GuiGetStyle(DROPDOWNBOX, DROPDOWN_ITEMS_SPACING));

            if (i == itemSelected)
            {
                GuiDrawRectangle(itemBounds, GuiGetStyle(DROPDOWNBOX, BORDER_WIDTH), GetColor(GuiGetStyle(DROPDOWNBOX, BORDER_COLOR_PRESSED)), GetColor(GuiGetStyle(DROPDOWNBOX, BASE_COLOR_PRESSED)));
                GuiDrawText(items[i], GetTextBounds(DROPDOWNBOX, itemBounds), GuiGetStyle(DROPDOWNBOX, TEXT_ALIGNMENT), GetColor(GuiGetStyle(DROPDOWNBOX, TEXT_COLOR_PRESSED)));
            }
            else if (i == itemFocused)
            {
                GuiDrawRectangle(itemBounds, GuiGetStyle(DROPDOWNBOX, BORDER_WIDTH), GetColor(GuiGetStyle(DROPDOWNBOX, BORDER_COLOR_FOCUSED)), GetColor(GuiGetStyle(DROPDOWNBOX, BASE_COLOR_FOCUSED)));
                GuiDrawText(items[i], GetTextBounds(DROPDOWNBOX, itemBounds), GuiGetStyle(DROPDOWNBOX, TEXT_ALIGNMENT), GetColor(GuiGetStyle(DROPDOWNBOX, TEXT_COLOR_FOCUSED)));
            }
            else GuiDrawText(items[i], GetTextBounds(DROPDOWNBOX, itemBounds), GuiGetStyle(DROPDOWNBOX, TEXT_ALIGNMENT), GetColor(GuiGetStyle(DROPDOWNBOX, TEXT_COLOR_NORMAL)));
        }
    }

    // Draw arrows (using icon if available)
#if defined(RAYGUI_NO_ICONS)
    GuiDrawText("^", RAYGUI_CLITERAL(Rectangle){ bounds.x + bounds.width - GuiGetStyle(DROPDOWNBOX, ARROW_PADDING), bounds.y + bounds.height/2 - 2, 10, 10 },
                TEXT_ALIGN_CENTER, GetColor(GuiGetStyle(DROPDOWNBOX, TEXT + (state*3))));
#else
    GuiDrawText("#121#", RAYGUI_CLITERAL(Rectangle){ bounds.x + bounds.width - GuiGetStyle(DROPDOWNBOX, ARROW_PADDING), bounds.y + bounds.height/2 - 4, 10, 10 },
                TEXT_ALIGN_CENTER, GetColor(GuiGetStyle(DROPDOWNBOX, TEXT + (state*3))));   // ICON_ARROW_DOWN_FILL
#endif
    //--------------------------------------------------------------------

    *active = itemSelected;

    // TODO: Use result to return more internal states: mouse-press out-of-bounds, mouse-press over selected-item...
    return result;   // Mouse click: result = 1
}

bool DropdownBox(const char* label, int* active, bool directionUp)
{
    detail::LabelId labelId(label);
    return detail::defaultWidget(GuiContext::deferDropdownBox, labelId.text(), active, directionUp, labelId.id());
}

bool Spinner(const char* label, int* value, int minValue, int maxValue)
{
    detail::LabelId labelId(label);
    auto oldValue = *value;
    detail::editableWidget(GuiSpinner, labelId.id(), labelId.text(), value, minValue, maxValue);
    *value = std::clamp(*value, minValue, maxValue);
    return *value != oldValue;
}

bool ValueBox(const char* label, int* value, int minValue, int maxValue)
{
    detail::LabelId labelId(label);
    auto oldValue = *value;
    detail::editableWidget(GuiValueBox, labelId.id(), labelId.text(), value, minValue, maxValue);
    return *value != oldValue;
}

bool TextBox(const char* id, char* text, int textSize)
{
    auto textId = GetId(id);
    // raygui only changes the text in edit mode, so only then a copy is needed
    bool editing = HasKeyboardFocus(textId);
    std::string oldText = editing ? text : "";
    detail::editableWidget(GuiTextBox, textId, text, textSize);
    return editing && oldText != text;
}

bool TextBox(const char* id, std::string& text, int textSize)
{
    auto textId = GetId(id);
    bool editing = HasKeyboardFocus(textId);
    std::string oldText = editing ? text : std::string();
    text.resize(textSize + 1);
    detail::editableWidget(GuiTextBox, textId, text.data(), textSize);
    text.resize(std::strlen(text.data()));
    return editing && oldText != text;
}

bool IsLastWidgetEditFinished()
{
    return GuiContext::lastWidgetEditFinished;
}
bool ListView(float height, const char* text, int* scrollIndex, int* active)
{
    auto& ctx = detail::context();
    auto size = ctx.standardSize(height);
    auto oldActive = *active;
    GuiListView({ctx.currentPos.x + ctx.scrollOffset.x, ctx.currentPos.y + ctx.scrollOffset.y, size.x, size.y}, text, scrollIndex, active);
    ctx.increment(size);
    return *active != oldActive;
}

bool ListViewEx(float height, const char** text, int count, int* scrollIndex, int* active, int* focus)
{
    auto& ctx = detail::context();
    auto size = ctx.standardSize(height);
    auto oldActive = *active;
    GuiListViewEx({ctx.currentPos.x + ctx.scrollOffset.x, ctx.currentPos.y + ctx.scrollOffset.y, size.x, size.y}, text, count, scrollIndex, active, focus);
    ctx.increment(size);
    return *active != oldActive;
}

static bool isSameColor(Color a, Color b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

bool ColorPicker(Color* color)
{
    auto& ctx = detail::context();
    auto size = ctx.standardSize();
    size.y = size.x;
    auto barSpace = GetStyle(COLORPICKER, HUEBAR_PADDING) + GuiGetStyle(COLORPICKER, HUEBAR_WIDTH) + GetStyle(COLORPICKER, HUEBAR_SELECTOR_OVERFLOW);
    auto oldColor = *color;
    GuiColorPicker({ctx.currentPos.x + ctx.scrollOffset.x, ctx.currentPos.y + ctx.scrollOffset.y, size.x - barSpace, size.y - barSpace}, nullptr, color);
    ctx.increment({size.x, size.y - barSpace});
    return !isSameColor(*color, oldColor);
}

bool ColorPanel(const char* text, Color* color)
{
    auto& ctx = detail::context();
    auto size = ctx.standardSize();
    size.y = size.x;
    auto oldColor = *color;
    GuiColorPanel({ctx.currentPos.x + ctx.scrollOffset.x, ctx.currentPos.y + ctx.scrollOffset.y, size.x, size.y}, text, color);
    ctx.increment({size.x, size.y});
    return !isSameColor(*color, oldColor);
}

bool ColorBarAlpha(const char* text, float* alpha)
{
    auto& ctx = detail::context();
    auto size = ctx.standardSize();
    auto oldAlpha = *alpha;
    GuiColorBarAlpha({ctx.currentPos.x + ctx.scrollOffset.x, ctx.currentPos.y + ctx.scrollOffset.y, size.x, size.y}, text, alpha);
    ctx.increment(size);
    return *alpha != oldAlpha;
}

bool ColorBarHue(const char* text, float* value)
{
    auto& ctx = detail::context();
    auto size = ctx.standardSize();
    auto oldValue = *value;
    GuiColorBarHue({ctx.currentPos.x + ctx.scrollOffset.x, ctx.currentPos.y + ctx.scrollOffset.y, size.x, size.y}, text, value);
    ctx.increment(size);
    return *value != oldValue;
}

namespace detail {

// Slider, SliderBar and ProgressBar place their left label in the indent if it fits, and their right label in the reserve
template <typename Func>
bool barWidget(Func fp, const char* textLeft, const char* textRight, float* value, float minValue, float maxValue)
{
    auto& ctx = detail::context();
    auto size = ctx.standardSize();
    auto leftWidth = GetTextWidth(textLeft);
    auto rightWidth = GetTextWidth(textRight);
    auto leftOffset = ctx.currentPos.x - ctx.content.x >= leftWidth ? 0 : leftWidth + (leftWidth ? 4 : 0);
    auto rightSpace = ctx.reserve >= rightWidth ? ctx.reserve : rightWidth + (rightWidth ? 4 : 0);
    auto oldValue = *value;
    fp({ctx.currentPos.x + leftOffset + ctx.scrollOffset.x, ctx.currentPos.y + ctx.scrollOffset.y, size.x - rightSpace - leftOffset, size.y}, textLeft, textRight, value, minValue, maxValue);
    ctx.increment(size);
    return *value != oldValue;
}

}  // namespace detail

bool Slider(const char* textLeft, const char* textRight, float* value, float minValue, float maxValue)
{
    return detail::barWidget(GuiSlider, textLeft, textRight, value, minValue, maxValue);
}

bool Slider(const char* textLeft, float* value, float minValue, float maxValue)
{
    return Slider(textLeft, TextFormat("%.2f", *value), value, minValue, maxValue);
}

bool SliderBar(const char* textLeft, const char* textRight, float* value, float minValue, float maxValue)
{
    return detail::barWidget(GuiSliderBar, textLeft, textRight, value, minValue, maxValue);
}

bool SliderBar(const char* textLeft, float* value, float minValue, float maxValue)
{
    return SliderBar(textLeft, TextFormat("%.2f", *value), value, minValue, maxValue);
}

void ProgressBar(const char* textLeft, const char* textRight, float value, float minValue, float maxValue)
{
    detail::barWidget(GuiProgressBar, textLeft, textRight, &value, minValue, maxValue);
}

Vector2 Grid(float height, float spacing, int subdivs)
{
    auto& ctx = detail::context();
    auto size = ctx.standardSize(height);
    Vector2 mouseCell{};
    GuiGrid({ctx.currentPos.x + ctx.scrollOffset.x, ctx.currentPos.y + ctx.scrollOffset.y, size.x, size.y}, nullptr, spacing, subdivs, &mouseCell);
    ctx.increment(size);
    return mouseCell;
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
    ::snprintf(buffer, sizeof(buffer), "#%03i#%s", iconId, text ? text : "");
    return buffer;
}

bool SimpleListView(float height, const char* id, const std::vector<std::string>& items, Vector2* scrollPos, int* active)
{
    Begin();
    BeginTableView(height, id, {TableColumn::Stretch()}, scrollPos);
    auto font = GuiGetFont();
    auto oldActive = *active;
    int count = 0;
    bool selected = false;
    for (const auto& item : items) {
        TableNextRow(15, [&font, &item, &count, &active, &selected](Rectangle rect) {
            if (count == *active) {
                DrawRectangleClipped(rect.x, rect.y, rect.width, rect.height, GetColor(GetStyle(LISTVIEW, BASE_COLOR_PRESSED)));
            }
            if (CheckCollisionPointRec(GetMousePosition(), rect)) {
                if (count != *active) {
                    DrawRectangleClipped(rect.x, rect.y, rect.width, rect.height, GetColor(GetStyle(LISTVIEW, BASE_COLOR_NORMAL)));
                }
                if (IsMouseButtonPressed(0)) {
                    *active = count;
                    selected = true;
                }
            }
            DrawTextClipped(font, item.c_str(), {rect.x + 4, rect.y + 3}, GetColor(GuiGetStyle(LISTVIEW, count == *active ? TEXT_COLOR_PRESSED : TEXT_COLOR_NORMAL)));
        });
        count++;
    }
    EndTableView();
    if (!selected && IsMouseButtonPressed(0) && CheckCollisionPointRec(GetMousePosition(), GetLastWidgetRect())) {
        *active = -1;
    }
    End();
    return *active != oldActive;
}

Rectangle AllocateWidget(Vector2 naturalSize)
{
    auto& ctx = detail::context();
    auto size = ctx.standardSize();
    if (ctx.nextWidth <= 0 && naturalSize.x > 0)
        size.x = naturalSize.x;
    if (ctx.nextHeight <= 0 && naturalSize.y > 0)
        size.y = naturalSize.y;
    Rectangle bounds{ctx.currentPos.x + ctx.scrollOffset.x, ctx.currentPos.y + ctx.scrollOffset.y, size.x, size.y};
    ctx.increment(size);
    return bounds;
}

float GetAlpha()
{
    return guiAlpha;
}

Vector2 GetTextSize(const char* text)
{
    return MeasureTextEx(GuiGetFont(), text, (float)GuiGetStyle(DEFAULT, TEXT_SIZE), (float)GuiGetStyle(DEFAULT, TEXT_SPACING));
}

bool IsSysKeyDown()
{
#ifdef __APPLE__
    return IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER);
#elif defined(__EMSCRIPTEN__)
    static bool macOS = EM_ASM_INT({
        if (navigator.userAgent.indexOf("Mac") != -1)
            return 1;
        return 0;
    }) != 0;
    if(macOS)
        return IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER);
    return IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
#else
    return IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
#endif
}

Color ColorFromHsv(Vector3 hsv)
{
    Vector3 rgbHue = ConvertHSVtoRGB(hsv);
    Color col = { (unsigned char)(255.0f*rgbHue.x), (unsigned char)(255.0f*rgbHue.y), (unsigned char)(255.0f*rgbHue.z), 255 };
    return col;
}

Vector3 HsvFromColor(Color col)
{
    Vector3 vcolor = { (float)col.r/255.0f, (float)col.g/255.0f, (float)col.b/255.0f };
    return ConvertRGBtoHSV(vcolor);
}

namespace PopupMenu {

void System::open(Vector2 position, const Menu& menu)
{
    close();

    if (menu.empty()) return;

    _menu = menu;
    _openedFrom = &menu;
    detail::OpenMenu root;
    root.items = &_menu;
    root.width = calculateMenuWidth(_menu);

    // Adjust position to keep menu on screen
    int height = calculateMenuHeight(_menu);
    int guiWidth = GuiContext::positionLimits.x;
    int guiHeight = GuiContext::positionLimits.y;

    if (position.x + root.width > guiWidth) {
        position.x = static_cast<float>(guiWidth - root.width - style.shadowOffset);
    }
    if (position.x < 0) {
        position.x = 0;
    }
    if (position.y + height > guiHeight) {
        position.y = static_cast<float>(guiHeight - height - style.shadowOffset);
    }
    if (position.y < 0) {
        position.y = 0;
    }

    root.position = position;
    _menuChain.push_back(root);
}

void System::close()
{
    _menuChain.clear();
    _closeGraceTimer = 0.0f;
    _closeGraceLevel = -1;
}

bool System::isOpen() const
{
    return !_menuChain.empty();
}

bool System::isMenuOpen(const Menu& menu) const
{
    return !_menuChain.empty() && _openedFrom == &menu;
}

bool System::update(float dt)
{
    if (_menuChain.empty())
        return false;

    GuiUnlock();
    _needsUnlock = true;

    Vector2 mouse = GetMousePosition();
    bool actionTriggered = false;

    // Update grace timer
    if (_closeGraceTimer > 0) {
        _closeGraceTimer -= dt;
    }

    // Find which menu (if any) the mouse is over
    // Process from deepest to root - deeper menus have priority
    int hitLevel = -1;
    int hitItem = -1;

    for (int level = static_cast<int>(_menuChain.size()) - 1; level >= 0; --level) {
        Rectangle bounds = getMenuBounds(_menuChain[level]);

        if (CheckCollisionPointRec(mouse, bounds)) {
            hitLevel = level;
            hitItem = getItemAtYPos(_menuChain[level], mouse.y);
            break;
        }
    }

    // Handle hover state changes
    if (hitLevel >= 0) {
        auto& menu = _menuChain[hitLevel];
        auto const& items = *menu.items;

        // Skip separators for hover
        if (hitItem >= 0 && items[hitItem].type == Item::Type::Separator) {
            hitItem = -1;
        }

        if (hitItem != menu.hoveredIndex) {
            // Hover changed
            int oldHover = menu.hoveredIndex;
            menu.hoveredIndex = hitItem;
            menu.hoverTime = 0.0f;

            // If we moved to a different item at this level, close deeper submenus
            // But give a grace period if we might be moving toward an open submenu
            if (hitLevel < static_cast<int>(_menuChain.size()) - 1) {
                // There's a submenu open - check if old hover was its parent
                if (oldHover >= 0 && items[oldHover].hasSubmenu()) {
                    // Start grace period
                    _closeGraceTimer = style.submenuCloseDelay;
                    _closeGraceLevel = hitLevel;
                } else {
                    // Close submenus immediately
                    _menuChain.resize(hitLevel + 1);
                }
            }
        } else {
            // Same item - accumulate hover time
            menu.hoverTime += dt;
        }

        // Clear hover on shallower menus (but keep their selection for visual)
        // Actually, we want to keep parent highlights when in submenu
        // So only clear hover time
        for (int i = hitLevel + 1; i < static_cast<int>(_menuChain.size()); ++i) {
            _menuChain[i].hoverTime = 0.0f;
        }

        // Check if grace period expired - close submenus
        if (_closeGraceTimer <= 0 && _closeGraceLevel == hitLevel) {
            if (hitLevel < static_cast<int>(_menuChain.size()) - 1) {
                // Only close if we're not hovering the item that opened the submenu
                auto const& parentItems = *_menuChain[hitLevel].items;
                int submenuParent = -1;
                if (hitLevel + 1 < static_cast<int>(_menuChain.size())) {
                    // Find which item opened the next submenu
                    for (int i = 0; i < static_cast<int>(parentItems.size()); ++i) {
                        if (parentItems[i].hasSubmenu() &&
                            &parentItems[i].children == _menuChain[hitLevel + 1].items) {
                            submenuParent = i;
                            break;
                        }
                    }
                }
                if (menu.hoveredIndex != submenuParent) {
                    _menuChain.resize(hitLevel + 1);
                }
            }
            _closeGraceLevel = -1;
        }

        // Open submenu after delay
        if (menu.hoveredIndex >= 0 && menu.hoverTime >= style.submenuOpenDelay) {
            auto const& item = items[menu.hoveredIndex];
            if (item.hasSubmenu() && item.enabled) {
                // Check if this submenu is already open
                bool alreadyOpen = (hitLevel + 1 < static_cast<int>(_menuChain.size()) &&
                                    _menuChain[hitLevel + 1].items == &item.children);
                if (!alreadyOpen) {
                    openSubmenu(hitLevel, menu.hoveredIndex);
                }
            }
        }
    } else {
        // Mouse not over any menu
        // Check grace period for moving between menus
        if (_closeGraceTimer <= 0 && _closeGraceLevel >= 0) {
            // Grace expired outside menus - might want to close deeper levels
            // But let's be lenient and only close if clicking outside
            _closeGraceLevel = -1;
        }
    }

    // Handle clicks
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        if (hitLevel >= 0 && hitItem >= 0) {
            auto const& item = (*_menuChain[hitLevel].items)[hitItem];
            if (item.canReact()) {
                triggerItem(item);
                actionTriggered = true;
                close();
            } else if (item.type == Item::Type::Toggle && item.enabled) {
                // Toggle items
                if (item.toggleValue) {
                    *item.toggleValue = !(*item.toggleValue);
                }
                if (item.action) {
                    auto action = item.action;  // the action might replace the menu
                    action();
                }
                actionTriggered = true;
                // Don't close menu for toggles - let user toggle multiple
            }
            // Clicking on submenu item does nothing (opens on hover)
        } else {
            // Clicked outside - close
            close();
        }
    }

    // Handle right-click outside to close (optional, common UX pattern)
    if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON) && hitLevel < 0) {
        close();
    }

    // Escape key closes menus
    if (IsKeyPressed(KEY_ESCAPE)) {
        close();
    }

    GuiLock();

    return actionTriggered;
}

void System::render()
{
    if (_menuChain.empty()) {
        if (_needsUnlock) {
            GuiUnlock();
            _needsUnlock = false;
        }
        return;
    }

    GuiUnlock();
    // Draw from root to deepest (so submenus appear on top)
    for (auto const& menu : _menuChain) {
        drawMenu(menu);
    }
    if (!_menuChain.empty())
        GuiLock();
}

Rectangle System::getMenuBounds(detail::OpenMenu const& menu) const
{
    return Rectangle{
        menu.position.x,
        menu.position.y,
        static_cast<float>(menu.width),
        static_cast<float>(calculateMenuHeight(*menu.items))
    };
}

int System::calculateMenuWidth(Menu const& items) const
{
    int maxLabelWidth = 0;
    int maxShortcutWidth = 0;

    for (auto const& item : items) {
        if (item.type == Item::Type::Separator) continue;

        int labelWidth = calculateTextWidth(item.label);
        maxLabelWidth = std::max(maxLabelWidth, labelWidth);

        if (!item.shortcut.empty()) {
            int shortcutWidth = calculateTextWidth(item.shortcut);
            maxShortcutWidth = std::max(maxShortcutWidth, shortcutWidth);
        }
    }

    int width = style.iconWidth + style.itemPaddingX + maxLabelWidth;
    if (maxShortcutWidth > 0) {
        width += 20 + maxShortcutWidth;  // gap + shortcut
    }
    width += style.itemPaddingRight;

    return std::max(width, style.minWidth);
}

int System::fontSize() const
{
    return style.fontSize > 0 ? style.fontSize : GuiGetStyle(DEFAULT, TEXT_SIZE);
}

int System::itemHeight() const
{
    return style.itemHeight > 0 ? style.itemHeight : fontSize() + 3;
}

int System::calculateTextWidth(const std::string& text) const
{
    return std::round(MeasureTextEx(GuiGetFont(), text.c_str(), fontSize(), 0).x);
}

int System::calculateMenuHeight(Menu const& items) const {
    int height = style.borderWidth * 2 + style.menuPaddingY * 2;  // top and bottom border
    for (auto const& item : items) {
        height += getItemHeight(item);
    }
    return height;
}

int System::getItemHeight(Item const& item) const {
    return (item.type == Item::Type::Separator) ? style.separatorHeight : itemHeight();
}

int System::getItemAtYPos(detail::OpenMenu const& menu, float y) const {
    float localYPos = y - menu.position.y - style.borderWidth - style.menuPaddingY;
    if (localYPos < 0) return -1;

    float accumulated = 0;
    for (int i = 0; i < static_cast<int>(menu.items->size()); ++i) {
        float itemHeight = static_cast<float>(getItemHeight((*menu.items)[i]));
        if (localYPos < accumulated + itemHeight) {
            return i;
        }
        accumulated += itemHeight;
    }
    return -1;
}

void System::openSubmenu(int parent_level, int item_index) {
    auto const& parent = _menuChain[parent_level];
    auto const& item = (*parent.items)[item_index];

    if (!item.hasSubmenu()) return;

    // Calculate position of the item that spawned this submenu
    float itemYPos = parent.position.y + style.borderWidth + style.menuPaddingY;
    for (int i = 0; i < item_index; ++i) {
        itemYPos += getItemHeight((*parent.items)[i]);
    }

    detail::OpenMenu submenu;
    submenu.items = &item.children;
    submenu.width = calculateMenuWidth(item.children);

    int submenuHeight = calculateMenuHeight(item.children);
    int guiWidth = GuiContext::positionLimits.x;
    int guiHeight = GuiContext::positionLimits.y;

    Rectangle parentBounds = getMenuBounds(parent);

    // Try to position to the right
    float x = parentBounds.x + parentBounds.width - 2;  // slight overlap
    if (x + submenu.width > guiWidth) {
        // Flip to left
        x = parentBounds.x - submenu.width + 2;
    }
    if (x < 0) x = 0;

    // Vertical positioning - align with parent item, adjust if off screen
    float subYPos = itemYPos - style.borderWidth - style.menuPaddingY;
    if (subYPos + submenuHeight > guiHeight) {
        subYPos = static_cast<float>(guiHeight - submenuHeight - style.shadowOffset);
    }
    if (subYPos < 0) subYPos = 0;

    submenu.position = {x, subYPos};

    // Close any existing deeper submenus and add this one
    _menuChain.resize(parent_level + 1);
    _menuChain.push_back(submenu);
}

void System::triggerItem(Item const& item) {
    if (item.action) {
        auto action = item.action;  // the action might replace the menu
        action();
    }
}

void System::drawMenu(detail::OpenMenu const& menu) const {
    Rectangle bounds = getMenuBounds(menu);

    // Shadow
    DrawRectangle(
        static_cast<int>(bounds.x) + style.shadowOffset,
        static_cast<int>(bounds.y) + style.shadowOffset,
        static_cast<int>(bounds.width),
        static_cast<int>(bounds.height),
        style.shadow
    );

    // Background
    DrawRectangleRec(bounds, GetColor(GetStyle(DEFAULT, BACKGROUND_COLOR)));

    // Border
    DrawRectangleLinesEx(bounds, static_cast<float>(style.borderWidth), GetColor(GetStyle(DEFAULT, BORDER_COLOR_NORMAL)));

    // Items
    float y = bounds.y + style.borderWidth + style.menuPaddingY;
    for (int i = 0; i < static_cast<int>(menu.items->size()); ++i) {
        auto const& item = (*menu.items)[i];
        int itemHeight = getItemHeight(item);

        Rectangle itemBounds = {bounds.x + style.borderWidth, y, bounds.width - style.borderWidth * 2, static_cast<float>(itemHeight)};
        bool hovered = (i == menu.hoveredIndex);
        drawItem(item, itemBounds, hovered);

        y += itemHeight;
    }
}

struct FilenameResult {
    std::string path;
    std::string filename;
};

FilenameResult truncatePathFilenameWithEllipsis(std::string_view text, std::size_t maxSize) {
    if (maxSize == 0) {
        return {};
    }
    ghc::filesystem::path fsPath{text};
    std::string filename = fsPath.filename().string();
    std::string parentPath = fsPath.parent_path().string();
    auto filenameLength = static_cast<std::size_t>(ghc::utf8::length(filename));
    auto parentLength = static_cast<std::size_t>(ghc::utf8::length(parentPath));
    bool hasParent = !parentPath.empty();
    std::size_t totalLength = hasParent ? (parentLength + 1 + filenameLength) : filenameLength;
    if (totalLength <= maxSize) {
        if (hasParent) {
            parentPath += ghc::filesystem::path::preferred_separator;
        }
        return {std::move(parentPath), std::move(filename)};
    }
    if (filenameLength > maxSize) {
        return {{}, ghc::utf8::truncateWithEllipsis(filename, maxSize)};
    }
    std::size_t pathBudget = maxSize - filenameLength;
    if (pathBudget < 2) {
        return {{}, std::move(filename)};
    }
    std::string truncatedPath = ghc::utf8::truncateWithEllipsis(parentPath, pathBudget - 1);
    truncatedPath += ghc::filesystem::path::preferred_separator;
    return {std::move(truncatedPath), std::move(filename)};
}

void System::drawItem(Item const& item, Rectangle bounds, bool hovered) const {
    // Separator
    if (item.type == Item::Type::Separator) {
        float y = bounds.y + bounds.height / 2;
        DrawLine(
            static_cast<int>(bounds.x + 4),
            static_cast<int>(y),
            static_cast<int>(bounds.x + bounds.width - 4),
            static_cast<int>(y),
            GetColor(GetStyle(DEFAULT, BORDER_COLOR_DISABLED))
        );
        return;
    }

    Color textCol =  GetColor(GetStyle(LABEL, item.enabled ? TEXT_COLOR_NORMAL : TEXT_COLOR_DISABLED));
    Color highlightCol = GetColor(GetStyle(LABEL, TEXT_COLOR_FOCUSED));
    Color shortcutCol = textCol;

    // Hover highlight
    if (hovered && item.enabled) {
        DrawRectangleRec(bounds, GetColor(GetStyle(DEFAULT, BASE_COLOR_FOCUSED)));
        textCol = GetColor(GetStyle(LABEL, TEXT_COLOR_FOCUSED));
        highlightCol = GetColor(GetStyle(LABEL, BASE_COLOR_NORMAL));
        shortcutCol = textCol;
    }

    int textYPos = static_cast<int>(bounds.y + (bounds.height - fontSize()) / 2) + 1;
    auto const& font = GuiGetFont();

    // Checkmark for toggle items
    if (item.type == Item::Type::Toggle && item.toggleValue && *item.toggleValue) {
        int checkXPos = static_cast<int>(bounds.x + style.iconWidth / 2 - 3);
        DrawTextClipped(font, "✓", Vector2(checkXPos, textYPos), textCol);
    }

    // Label
    int labelXPos = static_cast<int>(bounds.x + style.iconWidth);
    if (item.type == Item::Type::Filename) {
        auto parts = truncatePathFilenameWithEllipsis(item.label, 64);
        if (!parts.path.empty()) {
            DrawTextClipped(font, parts.path.c_str(), Vector2(labelXPos, textYPos), textCol);
            labelXPos += calculateTextWidth(parts.path);
        }
        DrawTextClipped(font, parts.filename.c_str(), Vector2(labelXPos, textYPos), highlightCol);
    }
    else {
        DrawTextClipped(font, item.label.c_str(), Vector2(labelXPos, textYPos), textCol);
    }
    // Shortcut hint
    if (!item.shortcut.empty()) {
        int shortcutWidth = calculateTextWidth(item.shortcut);
        int shortcutXPos = static_cast<int>(bounds.x + bounds.width - style.itemPaddingRight - shortcutWidth);
        DrawTextClipped(font, item.shortcut.c_str(), Vector2(shortcutXPos, textYPos), shortcutCol);
    }

    // Submenu arrow
    if (item.hasSubmenu()) {
        int arrowXPos = static_cast<int>(bounds.x + bounds.width - style.submenuArrowOffset);
        DrawTextClipped(font, "›", Vector2(arrowXPos, textYPos), textCol);
    }
}

void Open(Vector2 position, const Menu& menu)
{
    GuiContext::popup.open(position, menu);
}

void Close()
{
    GuiContext::popup.close();
}

bool IsOpen()
{
    return GuiContext::popup.isOpen();
}

bool IsMenuOpen(const Menu& menu)
{
    return GuiContext::popup.isMenuOpen(menu);
}

void SetMenuStyle(const Style& style)
{
    GuiContext::popup.style = style;
}

const Style& GetMenuStyle()
{
    return GuiContext::popup.style;
}

} // namespace PopupMenu

void MenuBar(const char* id, const PopupMenu::Bar& bar)
{
    auto& state = g_menuBarMap[GetId(id)];
    state.lastUpdate = GuiContext::frameId;
    auto rect = AllocateWidget({0, 0});
    GuiStatusBar(rect, nullptr);

    int openIndex = -1;
    for (int i = 0; i < (int)bar.size(); ++i) {
        if (PopupMenu::IsMenuOpen(bar[i].menu))
            openIndex = i;
    }
    // While a menu is open the popup menu system locks the gui, the bar still has to react then. A click on
    // a title already closed the open menu in BeginGui(), so openAtFrameStart tells if it was open before.
    bool interactive = GuiGetState() != STATE_DISABLED && (openIndex >= 0 || state.openAtFrameStart >= 0 || !GuiIsLocked());
    auto mouse = GetMousePosition();
    const float padding = 6;
    float x = rect.x;
    for (int i = 0; i < (int)bar.size(); ++i) {
        const auto& entry = bar[i];
        Rectangle titleRect{x, rect.y + 1, GetTextSize(entry.title.c_str()).x + 2 * padding, rect.height - 2};
        x += titleRect.width;
        bool hovered = interactive && CheckCollisionPointRec(mouse, titleRect);
        if (hovered) {
            Vector2 menuPos{titleRect.x, rect.y + rect.height};
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                if (state.openAtFrameStart == i)
                    PopupMenu::Close();
                else
                    PopupMenu::Open(menuPos, entry.menu);
            }
            else if (openIndex >= 0 && openIndex != i) {
                PopupMenu::Open(menuPos, entry.menu);
            }
        }
        bool highlighted = PopupMenu::IsMenuOpen(entry.menu) || (hovered && openIndex < 0);
        int textColor = GuiGetState() == STATE_DISABLED ? TEXT_COLOR_DISABLED : (highlighted ? TEXT_COLOR_FOCUSED : TEXT_COLOR_NORMAL);
        if (highlighted)
            GuiDrawRectangle(titleRect, 0, BLANK, GetColor(GuiGetStyle(DEFAULT, BASE_COLOR_FOCUSED)));
        GuiDrawText(entry.title.c_str(), titleRect, TEXT_ALIGN_CENTER, GetColor(GuiGetStyle(LABEL, textColor)));
    }
    state.openAtFrameStart = -1;
    for (int i = 0; i < (int)bar.size(); ++i) {
        if (PopupMenu::IsMenuOpen(bar[i].menu))
            state.openAtFrameStart = i;
    }
}


}  // namespace gui

extern "C" {

// Returns the intersection of 'rect' with 'clipRect'.
// If there is no overlap, the resulting rectangle will have width or height = 0.
Rectangle clipRectangle(const Rectangle& clipRect, const Rectangle& rect)
{
    // Compute the left and top edges as the max of both rects' left/top.
    float newX = std::max(clipRect.x, rect.x);
    float newY = std::max(clipRect.y, rect.y);

    // Compute the right and bottom edges as the min of both rects' right/bottom.
    float right  = std::min(clipRect.x + clipRect.width,  rect.x + rect.width);
    float bottom = std::min(clipRect.y + clipRect.height, rect.y + rect.height);

    // Determine the width and height.
    float newWidth  = right  - newX;
    float newHeight = bottom - newY;

    // If there's no intersection, ensure width/height aren't negative.
    if (newWidth < 0.0f)  newWidth  = 0.0f;
    if (newHeight < 0.0f) newHeight = 0.0f;

    // Return the clipped rectangle.
    return Rectangle {
        newX,
        newY,
        newWidth,
        newHeight
    };
}

/**
 * Clips dstRect against destClipRect, and updates srcRect accordingly
 * so that the same portion of the source image is drawn into the newly
 * clipped destination.
 *
 * @param destClipRect  The clip region in destination space (the visible region).
 * @param srcRect       [in,out] The source rectangle (in source-image coordinates).
 * @param dstRect       [in,out] The destination rectangle (in screen or world coordinates).
 */
void clipRectangles(const Rectangle& destClipRect, Rectangle& srcRect, Rectangle& dstRect)
{
    // --- [Early-out #1]: If dstRect is fully outside the clip area (no intersection),
    //                     the intersection logic below will handle it anyway.
    //                     But we can also do it explicitly at the end after computing the intersection.

    // --- [Early-out #2]: If dstRect is already fully within the clipping area,
    //                     there's no need to modify either rectangle.
    //                     We check:
    //                        1. dstRect fully inside horizontally
    //                        2. dstRect fully inside vertically
    if ((dstRect.x >= destClipRect.x) &&
        (dstRect.y >= destClipRect.y) &&
        (dstRect.x + dstRect.width  <= destClipRect.x + destClipRect.width) &&
        (dstRect.y + dstRect.height <= destClipRect.y + destClipRect.height))
    {
        // No clipping is needed, so simply return.
        return;
    }

    // 1. Compute intersection of dstRect with the clipping rect.
    float newDstX      = std::max(destClipRect.x,                      dstRect.x);
    float newDstY      = std::max(destClipRect.y,                      dstRect.y);
    float destRight    = std::min(destClipRect.x + destClipRect.width, dstRect.x + dstRect.width);
    float destBottom   = std::min(destClipRect.y + destClipRect.height, dstRect.y + dstRect.height);

    float newDstWidth  = destRight  - newDstX;
    float newDstHeight = destBottom - newDstY;

    // 2. If there's no intersection, set everything to zero and return.
    if (newDstWidth <= 0.0f || newDstHeight <= 0.0f)
    {
        dstRect.width  = 0.0f;
        dstRect.height = 0.0f;
        srcRect.width  = 0.0f;
        srcRect.height = 0.0f;
        return;
    }

    // 3. Figure out how much was clipped from the left and top in the destination space.
    //    We'll need this to shift the source rectangle accordingly.
    float clippedLeftDst = newDstX - dstRect.x;
    float clippedTopDst  = newDstY - dstRect.y;

    // 4. Compute scaling factors (destination size / source size).
    //    We assume srcRect.width/height != 0 in normal usage.
    float scaleX = dstRect.width  / srcRect.width;
    float scaleY = dstRect.height / srcRect.height;

    // 5. Convert the clipped offsets in destination space to source space.
    float clippedLeftSrc = clippedLeftDst / scaleX;
    float clippedTopSrc  = clippedTopDst  / scaleY;

    // 6. Adjust the source rectangle's top-left corner by the same fraction that got clipped.
    srcRect.x += clippedLeftSrc;
    srcRect.y += clippedTopSrc;

    // 7. Compute how big the source rectangle should be after clipping in destination space.
    //    The new destination width/height is (newDstWidth, newDstHeight).
    //    In source coordinates, that corresponds to (newDstWidth / scaleX, newDstHeight / scaleY).
    float newSrcWidth  = newDstWidth  / scaleX;
    float newSrcHeight = newDstHeight / scaleY;

    // 8. Clamp newSrcWidth/newSrcHeight if we're near the edge of the source.
    float remainingSrcWidth  = srcRect.width  - clippedLeftSrc;
    float remainingSrcHeight = srcRect.height - clippedTopSrc;
    if (newSrcWidth > remainingSrcWidth)
    {
        newSrcWidth  = remainingSrcWidth;
        // Adjust the destination width accordingly to keep scale consistent.
        newDstWidth  = newSrcWidth * scaleX;
    }
    if (newSrcHeight > remainingSrcHeight)
    {
        newSrcHeight = remainingSrcHeight;
        // Adjust the destination height accordingly to keep scale consistent.
        newDstHeight = newSrcHeight * scaleY;
    }

    // 9. Write back final clipped values
    dstRect.x      = newDstX;
    dstRect.y      = newDstY;
    dstRect.width  = newDstWidth;
    dstRect.height = newDstHeight;

    srcRect.width  = newSrcWidth;
    srcRect.height = newSrcHeight;
}

void DrawTextCodepointClipped(Font font, int codepoint, Vector2 position, float fontSize, Color tint)
{
    // Character index position in sprite font
    // NOTE: In case a codepoint is not available in the font, index returned points to '?'
    int index = GetGlyphIndex(font, codepoint);
    float scaleFactor = fontSize/font.baseSize;     // Character quad scaling factor

    // Character destination rectangle on screen
    // NOTE: We consider glyphPadding on drawing
    Rectangle dstRec = { position.x + font.glyphs[index].offsetX*scaleFactor - (float)font.glyphPadding*scaleFactor,
                      position.y + font.glyphs[index].offsetY*scaleFactor - (float)font.glyphPadding*scaleFactor,
                      (font.recs[index].width + 2.0f*font.glyphPadding)*scaleFactor,
                      (font.recs[index].height + 2.0f*font.glyphPadding)*scaleFactor };

    // Character source rectangle from font texture atlas
    // NOTE: We consider chars padding when drawing, it could be required for outline/glow shader effects
    Rectangle srcRec = { font.recs[index].x - (float)font.glyphPadding, font.recs[index].y - (float)font.glyphPadding,
                         font.recs[index].width + 2.0f*font.glyphPadding, font.recs[index].height + 2.0f*font.glyphPadding };

    if (!gui::g_clippingStack.empty()) {
        clipRectangles(gui::g_clippingStack.top(), srcRec, dstRec);
        if (dstRec.width > 0.0f) {
            // Draw the character texture on the screen
            DrawTexturePro(font.texture, srcRec, dstRec, (Vector2){ 0, 0 }, 0.0f, tint);
        }
    }
    else {
        // Draw the character texture on the screen
        DrawTexturePro(font.texture, srcRec, dstRec, (Vector2){ 0, 0 }, 0.0f, tint);
    }
}

void DrawTextClipped(Font font, const char *text, Vector2 position, Color tint)
{
    if (font.texture.id == 0) font = GetFontDefault();  // Security check in case of not valid font

    int size = TextLength(text);    // Total size in bytes of the text, scanned by codepoints in loop

    float fontSize = font.baseSize;
    float spacing = 0;
    float textLineSpacing = fontSize + 3;
    float textOffsetY = 0;          // Offset between lines (on linebreak '\n')
    float textOffsetX = 0.0f;       // Offset X to next character to draw

    float scaleFactor = fontSize/font.baseSize;         // Character quad scaling factor

    for (int i = 0; i < size;)
    {
        // Get next codepoint from byte string and glyph index in font
        int codepointByteCount = 0;
        int codepoint = GetCodepointNext(&text[i], &codepointByteCount);
        int index = GetGlyphIndex(font, codepoint);

        if (codepoint == '\n')
        {
            // NOTE: Line spacing is a global variable, use SetTextLineSpacing() to setup
            textOffsetY += (fontSize + textLineSpacing);
            textOffsetX = 0.0f;
        }
        else
        {
            if ((codepoint != ' ') && (codepoint != '\t'))
            {
                DrawTextCodepointClipped(font, codepoint, (Vector2){ position.x + textOffsetX, position.y + textOffsetY }, fontSize, tint);
            }

            if (font.glyphs[index].advanceX == 0) textOffsetX += ((float)font.recs[index].width*scaleFactor + spacing);
            else textOffsetX += ((float)font.glyphs[index].advanceX*scaleFactor + spacing);
        }

        i += codepointByteCount;   // Move text bytes counter to next codepoint
    }
}

inline void drawRectClipped(Rectangle rect, Color col)
{
    if (!gui::g_clippingStack.empty()) {
        rect = clipRectangle(gui::g_clippingStack.top(), rect);
    }
    DrawRectangle((int)rect.x, (int)rect.y, (int)rect.width, (int)rect.height, col);
}

void DrawRectangleClipped(int posX, int posY, int width, int height, Color color)
{
    drawRectClipped({(float)posX, (float)posY, (float)width, (float)height}, color);
}

void DrawPanelClipped(Rectangle rec, int borderWidth, Color borderColor, Color color)
{
    if (color.a > 0)
    {
        DrawRectangleClipped((int)rec.x, (int)rec.y, (int)rec.width, (int)rec.height, GuiFade(color, guiAlpha));
    }
    if (borderWidth > 0)
    {
        DrawRectangleClipped((int)rec.x, (int)rec.y, (int)rec.width, borderWidth, GuiFade(borderColor, guiAlpha));
        DrawRectangleClipped((int)rec.x, (int)rec.y + borderWidth, borderWidth, (int)rec.height - 2*borderWidth, GuiFade(borderColor, guiAlpha));
        DrawRectangleClipped((int)rec.x + (int)rec.width - borderWidth, (int)rec.y + borderWidth, borderWidth, (int)rec.height - 2*borderWidth, GuiFade(borderColor, guiAlpha));
        DrawRectangleClipped((int)rec.x, (int)rec.y + (int)rec.height - borderWidth, (int)rec.width, borderWidth, GuiFade(borderColor, guiAlpha));
    }
}

} // extern "C"
