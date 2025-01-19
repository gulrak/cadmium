//
// Created by schuemann on 19.12.24.
//
//-----------------------------------------------------------------------------
// IMPLEMENTATION
//-----------------------------------------------------------------------------

#define RAYGUI_IMPLEMENTATION
#define RLGUIPP_IMPLEMENTATION
#include <rlguipp/rlguipp.hpp>

#include <algorithm>
#include <cmath>
#include <stack>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

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
uint64_t fnv_64a_str(const char* str, uint64_t hval, size_t cnt = 0)
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
    if(cnt) {
        hval ^= (uint64_t)cnt;
        hval *= ((uint64_t)0x100000001b3ULL);
    }
    /* return our new hash value */
    return hval;
}
//---------------------------------------------------------------------------

}

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
    float incX{0};
    float incY{0};
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
    Vector2 maxSize{};
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
    uint64_t hash{0xbeef};
    size_t childContextCount{0};
    // RenderTexture* texture{nullptr};

    GuiContext& newContext(const std::string& key) const;

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
    }
    void wrap()
    {
        if (horizontal) {
            currentPos.x = area.x;
            currentPos.y += maxSize.y;
        }
        else {
            currentPos.x += maxSize.x;
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
        int* active{nullptr};
        bool directionUp{false};
        bool clicked{false};
        std::string text;
        bool editMode{false};
        int64_t lastUpdate{0};
        int64_t lastDraw{0};
        int state{0};
        bool guiDisabled{false};
        unsigned int style[RAYGUI_MAX_PROPS_BASE + RAYGUI_MAX_PROPS_EXTENDED];
    };
    inline static std::unordered_map<uint64_t, DropdownInfo> dropdownBoxes;
    inline static uint64_t openDropdownboxId{0};
    inline static void* editFocusId{nullptr};
    inline static GuiContext* rootContext{nullptr};
    inline static std::string tooltipText;
    inline static Rectangle tooltipParentRect{};
    inline static float tooltipTimer{};
    static bool deferDropdownBox(Rectangle rect, const char* text, int* active, bool directionUp, uint64_t hash)
    {
        uint64_t key = detail::fnv_64a_str(text, hash);
        auto iter = dropdownBoxes.find(key);
        if (iter != dropdownBoxes.end()) {
            iter->second.lastUpdate = frameId;
            iter->second.rect = rect;
            iter->second.active = active;
            for (int i = 0; i < RAYGUI_MAX_PROPS_BASE + RAYGUI_MAX_PROPS_EXTENDED; ++i) {
                iter->second.style[i] = GetStyle(DROPDOWNBOX, i);
            }
            iter->second.guiDisabled = GuiGetState() == STATE_DISABLED;
            return iter->second.clicked;
        }
        else {
            iter = dropdownBoxes.emplace(key, DropdownInfo{rect, active, directionUp, false, std::string(text), false, 0, 0, GetState()}).first;
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
                iter->second.clicked = false;
                openDropdownboxId = 0;
            }
        }
    }
    static void handleDeferredDropBox(uint64_t key, DropdownInfo& info)
    {
        if (info.lastDraw < info.lastUpdate && info.lastUpdate == frameId) {
            for (int i = 0; i < RAYGUI_MAX_PROPS_BASE + RAYGUI_MAX_PROPS_EXTENDED; ++i) {
                SetStyle(DROPDOWNBOX, i, info.style[i]);
            }
            auto oldState = GuiGetState();
            if(info.guiDisabled)
                GuiDisable();
            if (info.directionUp ? GuiDropupBox(info.rect, info.text.c_str(), info.active, info.editMode) : GuiDropdownBox(info.rect, info.text.c_str(), info.active, info.editMode)) {
                if (openDropdownboxId != key) {
                    closeOpenDropdownBox();
                }
                info.clicked = info.editMode;
                info.editMode = !info.editMode;
                openDropdownboxId = info.editMode ? key : 0;
            }
            else {
                info.clicked = false;
            }
            if(info.guiDisabled)
                GuiEnable();
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
            if(GuiContext::tooltipTimer <= 0.0f) {
                auto size = MeasureTextEx(GuiGetFont(), GuiContext::tooltipText.c_str(), 8, 0);
                Rectangle tipRect{
                    GuiContext::tooltipParentRect.x + GuiContext::tooltipParentRect.width/2 - size.x/2 - 3,
                    GuiContext::tooltipParentRect.y + GuiContext::tooltipParentRect.height*3/4,
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
static std::stack<Rectangle> g_clippingStack;

inline GuiContext& GuiContext::newContext(const std::string& key) const
{
    g_contextStack.push(*this);
    auto& ctx = g_contextStack.top();
    ctx.childContextCount = 0;
    ctx.hash = detail::fnv_64a_str(key.c_str(), ctx.hash, ++g_contextStack.top().childContextCount);
    ctx.maxSize = {};
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
    auto& ctx = ctxParent.newContext("Begin");
    ctx.type = GuiContext::ctGROUP;
    ctx.area = {ctxParent.currentPos.x, ctxParent.currentPos.y, ctx.nextWidth > 0 ? ctx.nextWidth : ctxParent.content.width + ctxParent.content.x - ctxParent.currentPos.x, ctxParent.content.height + ctxParent.content.y - ctxParent.currentPos.y};
    ctx.content = ctx.area;
    ctx.initialPos = ctx.currentPos;
    ctx.horizontal = false;
    ctx.bordered = false;
    ctx.level++;
    ctx.nextWidth = ctx.nextHeight = -1;
}

static void EndImpl(Vector2 size = {})
{
    auto ctxOld = g_contextStack.top();
    g_contextStack.pop();
    auto& ctx = detail::context();
    if (size.x > 0 && size.y > 0) {
        ctx.increment(size);
    }
    else if (ctxOld.horizontal) {
        // DrawLine(ctxOld.area.x, ctxOld.area.y, ctxOld.currentPos.x, ctxOld.initialPos.y + ctxOld.maxSize, RED);
        ctx.increment({ctxOld.currentPos.x - ctxOld.area.x + ctxOld.padding.x - ctxOld.spacingH, ctxOld.maxSize.y + ctxOld.padding.y*2});
    }
    else {
        // DrawLine(ctxOld.area.x, ctxOld.area.y, ctxOld.initialPos.x + ctxOld.maxSize, ctxOld.currentPos.y, GREEN);
        ctx.increment({ctxOld.maxSize.x + ctxOld.padding.x*2, ctxOld.currentPos.y - ctxOld.area.y + ctxOld.padding.y - ctxOld.spacingV});
    }
}

void End()
{
    if (g_contextStack.size() < 2) {
        throw std::runtime_error("Unbalanced gui::Begin*/gui::End*!");
    }
    if (detail::context().horizontal) {
         throw std::runtime_error("Unbalanced gui::BeginColumns/gui::EndColumns!");
    }
    EndImpl();
}

void BeginColumns()
{
    Begin();
    detail::context().type = GuiContext::ctCOLUMNS;
    detail::context().horizontal = true;
}

void EndColumns()
{
    auto ctx = detail::context();
    if (!ctx.horizontal) {
        throw std::runtime_error("Unbalanced gui::BeginColumns/gui::EndColumns!");
    }
    // detail::context().horizontal = false;
    EndImpl({ctx.currentPos.x - ctx.area.x, ctx.maxSize.y});
}

void BeginPanel(const char* text, Vector2 padding)
{
    auto& ctxParent = detail::context();
    auto& ctx = ctxParent.newContext("BeginPanel");
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
    ctx.padding = padding;
}

void EndPanel()
{
    auto& ctx = detail::context();
    if (ctx.level > 1) {
        GuiDrawRectangle({ctx.area.x, ctx.area.y, ctx.area.width, ctx.currentPos.y - ctx.area.y + ctx.padding.y - ctx.spacingV}, 1, Fade(GetColor(gui::GetStyle(DEFAULT, LINE_COLOR)), guiAlpha), {0, 0, 0, 0});
    }
    else {
        GuiDrawRectangle({ctx.area.x, ctx.area.y, ctx.area.width, ctx.area.height}, 1, Fade(GetColor(gui::GetStyle(DEFAULT, LINE_COLOR)), guiAlpha), {0, 0, 0, 0});
    }
    // ctx.increment({0, ctx.currentPos.y - ctx.area.y + (ctx.groupName.empty() ? 10 : RAYGUI_WINDOWBOX_STATUSBAR_HEIGHT + 10)});
    //ctx.increment({1,2});
    EndImpl();
}

void BeginTabView(int *activeTab)
{
    auto& tvc = TabViewContext::getContext(activeTab);
    auto& ctxParent = detail::context();
    auto& ctx = ctxParent.newContext("BeginTabView");
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
        //GuiDrawRectangle({ctx.area.x, ctx.area.y, ctx.area.width, ctx.currentPos.y - ctx.area.y + ctx.padding.y}, 1, Fade(GetColor(gui::GetStyle(DEFAULT, LINE_COLOR)), guiAlpha), {0, 0, 0, 128});
    }
    else {
        //GuiDrawRectangle({ctx.area.x, ctx.area.y, ctx.area.width, ctx.area.height}, 1, Fade(GetColor(gui::GetStyle(DEFAULT, LINE_COLOR)), guiAlpha), {0, 0, 0, 128});
    }
    // ctx.increment({0, ctx.currentPos.y - ctx.area.y + (ctx.groupName.empty() ? 10 : RAYGUI_WINDOWBOX_STATUSBAR_HEIGHT + 10)});
    //ctx.increment({1,2});
    // ctx.maxSize = ctx.area.width;
    auto ctxOld = g_contextStack.top();
    g_contextStack.pop();
    auto& ctxNew = detail::context();
    auto& tvc = *std::get<TabViewContext*>(ctxOld.contextData);
    ctxNew.increment({tvc.incX, tvc.incY});
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
    auto& ctx = ctxParent.newContext("BeginTab");
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
    ctx.padding = padding;
    return true;
}

void EndTab()
{
    auto& ctx = detail::context();
    auto ctxOld = ctx;
    if (ctx.level > 1) {
        GuiDrawRectangle({ctx.area.x, ctx.area.y, ctx.area.width, ctx.currentPos.y - ctx.area.y + ctx.padding.y}, 1, Fade(GetColor(gui::GetStyle(DEFAULT, LINE_COLOR)), guiAlpha), {0, 0, 0, 0});
    }
    else {
        GuiDrawRectangle({ctx.area.x, ctx.area.y, ctx.area.width, ctx.area.height}, 1, Fade(GetColor(gui::GetStyle(DEFAULT, LINE_COLOR)), guiAlpha), {0, 0, 0, 0});
    }
    g_contextStack.pop();
    auto& ctxParent = detail::context();
    ctxOld.maxSize = {ctxOld.area.width, ctx.area.height};
    auto& tvc = *std::get<TabViewContext*>(ctxParent.contextData);
    if (ctxOld.horizontal) {
        // DrawLine(ctxOld.area.x, ctxOld.area.y, ctxOld.currentPos.x, ctxOld.initialPos.y + ctxOld.maxSize, RED);
        tvc.incX = ctxOld.currentPos.x - ctxOld.area.x;
        tvc.incY = ctxOld.maxSize.y;
    }
    else {
        // DrawLine(ctxOld.area.x, ctxOld.area.y, ctxOld.initialPos.x + ctxOld.maxSize, ctxOld.currentPos.y, GREEN);
        tvc.incX = ctxOld.maxSize.x;
        tvc.incY = ctxOld.currentPos.y - ctxOld.area.y;
    }
}

void BeginScrollPanel(float height, Rectangle content, Vector2 *scroll)
{
    auto& ctxParent = detail::context();
    auto& ctx = ctxParent.newContext("BeginScrollPanel");
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
    ctx.padding = {0, 0};

    Rectangle view;
    GuiScrollPanel(ctx.area, NULL, ctx.content, scroll, &view);
    ctx.scrollOffset = {ctx.area.x + scroll->x, ctx.area.y + scroll->y};
    //BeginScissorMode(view.x, view.y, view.width, view.height);
    BeginClipping(view);
    ctx.mouseOffset = ctxParent.mouseOffset;
}

void EndScrollPanel()
{
    if (detail::context().type != GuiContext::ctSCROLLPANEL) {
        throw std::runtime_error("Unbalanced gui::BeginScrollPanel/gui::EndScrollPanel!");
    }
    //EndScissorMode()
    EndClipping();
    auto& ctx = detail::context();
    EndImpl({ctx.area.width, ctx.area.height});
    //End();
    //auto ctxOld = g_contextStack.top();
    //g_contextStack.pop();
    //auto& ctx = detail::context();
    //ctx.increment({ctx.area.width, ctxOld.area.height});
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
    auto& ctx = ctxParent.newContext("BeginGroupBox");
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
    ctx.padding = {0, 0};
}

void EndGroupBox()
{
    auto& ctx = detail::context();
    GuiGroupBox({ctx.area.x, ctx.area.y + 8, ctx.area.width, ctx.currentPos.y - ctx.area.y - 4}, ctx.groupName.c_str());
    ctx.increment({0, ctx.spacingV});
    //DrawLineEx({ctx.currentPos.x, ctx.currentPos.y}, {ctx.currentPos.x + 4, ctx.currentPos.y}, 1.0f, RED);
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
    auto& ctx = ctxParent.newContext("BeginPopup" + std::to_string(reinterpret_cast<std::uintptr_t>(isOpen)));
    ctx.type = GuiContext::ctPOPUP;
    ctx.area = {0, 0, area.width, area.height};
    ctx.content = ctx.area;  //{2, 2, area.width - 4, area.height - 4};
    ctx.initialPos = {0, 0};
    ctx.currentPos = ctx.initialPos;
    ctx.horizontal = false;
    ctx.bordered = false;
    ctx.level = 0;
    ctx.nextWidth = ctx.nextHeight = -1;
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
    if (std::holds_alternative<RenderTexture*>(g_contextStack.top().contextData)) {
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

void BeginClipping(const Rectangle& clipArea)
{
    g_clippingStack.push(g_clippingStack.empty() ? clipArea : clipRectangle(g_clippingStack.top(), clipArea));
}

void EndClipping()
{
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
    auto& ctx = detail::context();
    if(width <= 1.0f)
        width = std::floor(ctx.content.width * width);
    ctx.nextWidth = width;
}

void SetNextHeight(float height)
{
    auto& ctx = detail::context();
    if(height <= 1.0f)
        height = std::floor(ctx.content.height * height);
    ctx.nextHeight = height;
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
    detail::defaultWidget(GuiToggle, text, &active);
    return active;
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
    auto rc = GuiToggleGroup({ctx.currentPos.x + ctx.scrollOffset.x, ctx.currentPos.y + ctx.scrollOffset.y, size.x, size.y}, text, &active);
    ctx.increment({cols * size.x + (cols - 1) * (float)GuiGetStyle(TOGGLE, GROUP_PADDING), rows * size.y + (rows - 1) * (float)GuiGetStyle(TOGGLE, GROUP_PADDING)});
    return active;
}

bool CheckBox(const char* text, bool checked)
{
    auto& ctx = detail::context();
    auto size = ctx.standardSize();
    auto rc = GuiCheckBox({ctx.currentPos.x + ctx.scrollOffset.x, ctx.currentPos.y + ctx.scrollOffset.y + (ctx.rowHeight - 15) / 2, 15, 15}, text, &checked);
    ctx.increment(size);
    return checked;
}

int ComboBox(const char* text, int* active)
{
    return detail::defaultWidget(GuiComboBox, text, active);
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

bool DropdownBox(const char* text, int* active, bool directionUp)
{
    return detail::defaultWidget(GuiContext::deferDropdownBox, text, active, directionUp, g_contextStack.top().hash);
}

bool Spinner(const char* text, int* value, int minValue, int maxValue)
{
    auto rc = detail::editableWidget(GuiSpinner, (void*)value, text, value, minValue, maxValue);
    *value = std::clamp(*value, minValue, maxValue);
    return rc;
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
                float maxWidth = (bounds.width - (GuiGetStyle(TEXTBOX, TEXT_PADDING) * 2));

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
                cursor.x = bounds.x + bounds.width - GuiGetStyle(TEXTBOX, TEXT_PADDING);
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
    return detail::editableWidget(GuiTextBox, (void*)text, text, textSize);
}

bool TextBox(std::string& text, int textSize)
{
    text.resize(textSize + 1);
    auto rc = detail::editableWidget(GuiTextBox, (void*)&text, text.data(), textSize);
    text.resize(std::strlen(text.data()));
    return rc;
}
/*
bool TextBoxMulti(float height, char* text, int textSize)
{
    auto& ctx = detail::context();
    auto size = ctx.standardSize(height);
    auto editMode = HasKeyboardFocus((void*)text);
    auto rc = GuiTextBoxMulti({ctx.currentPos.x + ctx.scrollOffset.x, ctx.currentPos.y + ctx.scrollOffset.y, size.x, size.y}, text, textSize, &editMode);
    if (rc) {
        SetKeyboardFocus(editMode ? nullptr : (void*)text);
    }
    ctx.increment(size);
    return rc;
}
*/
int ListView(float height, const char* text, int* scrollIndex, int active)
{
    auto& ctx = detail::context();
    auto size = ctx.standardSize(height);
    auto rc = GuiListView({ctx.currentPos.x + ctx.scrollOffset.x, ctx.currentPos.y + ctx.scrollOffset.y, size.x, size.y}, text, scrollIndex, &active);
    ctx.increment(size);
    return rc;
}

int ListViewEx(float height, const char** text, int count, int* focus, int* scrollIndex, int active)
{
    auto& ctx = detail::context();
    auto size = ctx.standardSize(height);
    auto rc = GuiListViewEx({ctx.currentPos.x + ctx.scrollOffset.x, ctx.currentPos.y + ctx.scrollOffset.y, size.x, size.y}, text, count, focus, scrollIndex, &active);
    ctx.increment(size);
    return rc;
}

Color ColorPicker(Color color)
{
    auto& ctx = detail::context();
    auto size = ctx.standardSize();
    size.y = size.x;
    auto barSpace = GetStyle(COLORPICKER, HUEBAR_PADDING) + GuiGetStyle(COLORPICKER, HUEBAR_WIDTH) + GetStyle(COLORPICKER, HUEBAR_SELECTOR_OVERFLOW);
    GuiColorPicker({ctx.currentPos.x + ctx.scrollOffset.x, ctx.currentPos.y + ctx.scrollOffset.y, size.x - barSpace, size.y - barSpace}, nullptr, &color);
    ctx.increment({size.x, size.y - barSpace});
    return color;
}

Color ColorPanel(const char* text, Color color)
{
    auto& ctx = detail::context();
    auto size = ctx.standardSize();
    size.y = size.x;
    GuiColorPicker({ctx.currentPos.x + ctx.scrollOffset.x, ctx.currentPos.y + ctx.scrollOffset.y, size.x, size.y}, nullptr, &color);
    ctx.increment({size.x, size.y});
    return color;
}

float ColorBarAlpha(const char* text, float alpha)
{
    auto& ctx = detail::context();
    auto size = ctx.standardSize();
    GuiColorBarAlpha({ctx.currentPos.x + ctx.scrollOffset.x, ctx.currentPos.y + ctx.scrollOffset.y, size.x, size.y}, text, &alpha);
    ctx.increment(size);
    return alpha;
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
    GuiSlider({ctx.currentPos.x + leftOffset + ctx.scrollOffset.x, ctx.currentPos.y + ctx.scrollOffset.y, size.x - rightSpace - leftOffset, size.y}, textLeft, textRight, &value, minValue, maxValue);
    ctx.increment(size);
    return value;
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
    GuiSliderBar({ctx.currentPos.x + leftOffset + ctx.scrollOffset.x, ctx.currentPos.y + ctx.scrollOffset.y, size.x - rightSpace - leftOffset, size.y}, textLeft, textRight, &value, minValue, maxValue);
    ctx.increment(size);
    return value;
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
    GuiProgressBar({ctx.currentPos.x + leftOffset + ctx.scrollOffset.x, ctx.currentPos.y + ctx.scrollOffset.y, size.x - rightSpace - leftOffset, size.y}, textLeft, textRight, &value, minValue, maxValue);
    ctx.increment(size);
    return value;
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
    auto& ctx = ctxParent.newContext("BeginMenuBar");
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
    auto& ctx = ctxParent.newContext("BeginMenu");
    ctx.type = GuiContext::ctMENU;
    ctx.content = {ctx.content.x + 5, ctx.content.y + ctx.spacingV / 2, ctx.content.width - 10, ctx.content.height - ctx.spacingV};
    ctx.initialPos = {ctx.content.x, ctx.content.y};
    ctx.currentPos = ctx.initialPos;
    ctx.horizontal = false;
    ctx.bordered = true;
    ctx.level++;
    ctx.groupName = "";
    ctx.nextWidth = ctx.nextHeight = -1;
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
    drawRectClipped(Rectangle(posX, posY, width, height), color);
}

}
