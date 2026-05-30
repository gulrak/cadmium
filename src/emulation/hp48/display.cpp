//
// Created by Steffen Schümann on 18.02.26.
//

#include "display.hpp"
#include "font.hpp"

#include <stdendian/stdendian.h>

namespace hp48 {

#if 0
constexpr uint32_t backgroundColor = 0xFFfbf4a5;
constexpr uint32_t foregroundOffColor = 0xFF96b0aa;
constexpr uint32_t foregroundOnColor = 0xFF0025b7;
#else
constexpr uint32_t backgroundColor = 0xfbf4a5FF;
constexpr uint32_t foregroundOffColor = 0x96b0aaFF;
constexpr uint32_t foregroundOnColor = 0x0025b7FF;
#endif

// Symbol 1: left-pointing notification badge (15px wide)
constexpr uint16_t symbol0[12] = {0x7ffc, 0xfffe, 0xf9fe, 0xf3fe, 0xe7fe, 0xc01e, 0xc00e, 0xe7ce, 0xf3ce, 0xf9ce, 0xffce, 0x7fcc};

// Symbol 0: right-pointing notification badge (15px wide)
constexpr uint16_t symbol1[12] = {0x7ffc, 0xfffe, 0xff3e, 0xff9e, 0xffce, 0xf006, 0xe006, 0xe7ce, 0xe79e, 0xe73e, 0xe7fe, 0x67fc};

// Symbol 2: alpha icon (15px wide)
constexpr uint16_t symbol2[12] = {0x07c0, 0x1822, 0x3032, 0x6034, 0xe034, 0xe038, 0xe030, 0xe030, 0xe070, 0x70b2, 0x1f1c, 0x0000};

// Symbol 3: alarm icon (15px wide)
constexpr uint16_t symbol3[12] = {0x2008, 0x4004, 0x4824, 0x9012, 0x9392, 0x9392, 0x9392, 0x9012, 0x4824, 0x4004, 0x2008, 0x0000};

// Symbol 4: busy icon (13px wide)
constexpr uint16_t symbol4[12] = {0x7ff0, 0x2020, 0x2020, 0x1fc0, 0x0f80, 0x0700, 0x0500, 0x0880, 0x1040, 0x2720, 0x2fa0, 0x7ff0};

// Symbol 5: transfer icon (15px wide)
constexpr uint16_t symbol5[12] = {0x3000, 0x7800, 0xcc30, 0x8618, 0x330c, 0x7ffe, 0x7ffe, 0x330c, 0x8618, 0xcc30, 0x7800, 0x3000};

// Widths for rendering
constexpr uint8_t symbol_widths[6] = {15, 15, 15, 15, 13, 15};

namespace {
    template <class... Ts>
    struct Overloaded : Ts... { using Ts::operator()...; };
    template <class... Ts>
    Overloaded(Ts...) -> Overloaded<Ts...>;

    void renderSymbol(ghc::span2d<uint32_t> pixelBuf, int x, int y, const uint16_t* symbol, uint8_t width, uint32_t foregroundColor)
    {
        int scale = 2;
        for (int row = 0; row < 12; ++row) {
            auto dstY = y + row * scale;
            auto symbolRow = symbol[row];
            for (int col = 0; col < width; ++col) {
                auto dstX = x + col * scale;
                if (!(symbolRow & (0x8000u >> col)))
                    continue;
                // Fill 2x2 pixels
                for (int dy = 0; dy < scale; ++dy) {
                    int const py = dstY + dy;
                    if (py < 0 || py >= int(pixelBuf.height()))
                        break;
                    for (int dx = 0; dx < scale; ++dx) {
                        int const px = dstX + dx;
                        if (px < 0 || px >= int(pixelBuf.width()))
                            continue;
                        pixelBuf(py, px) = foregroundColor;
                    }
                }
            }
        }
    }

    void renderChar(const hp48::Font& font, std::span<uint8_t> frameBuffer, int& x, int y, uint8_t c)
    {
        auto glyph = font.getGlyph(c);
        if (glyph) {
            for (int row = 0; row < font.height; ++row) {
                auto dstY = y + row;
                auto glyphRow = glyph->rows[row];
                for (int col = 0; col < glyph->width; ++col) {
                    auto dstX = x + col;
                    if (!(glyphRow & (0x80u >> col)))
                        continue;
                    auto byteIndex = dstY * 17 + (dstX >> 3);
                    auto bitMask = 0x80u >> (dstX & 7);
                    frameBuffer[byteIndex] ^= bitMask;
                }
            }
            x += glyph->width;
        }
    }

    int calcTextWidth(const hp48::Font& font, std::string_view str)
    {
        int width = 0;
        for (auto c : str) {
            auto glyph = font.getGlyph(c);
            if (glyph) {
                width += glyph->width;
            }
        }
        return width ? width - 1 : width;
    }

    void renderString(const hp48::Font& font, std::span<uint8_t> frameBuffer, int x, int y, std::string_view str)
    {
        for (auto c : str) {
            renderChar(font, frameBuffer, x, y, c);
        }
    }

    void invertPixel(std::span<uint8_t> frameBuffer, int x, int y)
    {
        auto byteIndex = y * 17 + (x >> 3);
        auto bitMask = 0x80u >> (x & 7);
        frameBuffer[byteIndex] ^= bitMask;
    }

    void invertRect(std::span<uint8_t> frameBuffer, int x, int y, int w, int h)
    {
        for (int dy = 0; dy < h; ++dy) {
            for (int dx = 0; dx < w; ++dx) {
                invertPixel(frameBuffer, x + dx, y + dy);
            }
        }
    }

    const char* formatObject(const Object& obj, size_t width = 20)
    {
        thread_local std::string buf;
        buf.clear();
        buf.reserve(width);

        std::visit(
            Overloaded{
                [&](Int v) { buf = std::to_string(v); },
                [&](Real v) {
                    char tmp[64];
                    std::snprintf(tmp, sizeof(tmp), "%.12g", v);
                    buf = tmp;
                },
                [&](const std::string& s) {
                    if (s.size() > width-2) {
                        buf = "\"" + s.substr(0, width-2) + "\x1f";
                    }
                    else {
                        buf = "\"" + s + "\"";
                    }
                },
                [&](const Symbol& s) { buf = s.name; },
                [&](const Blob& b) {
                    buf += "\"";
                    for (auto c : b.view()) {
                        if (buf.size() >= width-2) {
                            buf += "\x1f";
                            return;
                        }
                        if (c == '"' || c == '\\') {
                            buf += '\\';
                        }
                        buf += c < 32 ? '\x9e' : c;
                    }
                    buf += "\"";
                },

                // Not filled in yet:
                [&](const auto&) { buf = "<obj>"; },
            },
            obj
        );
        if (buf.size() > width) {
            buf.resize(width - 1);
            buf += "\x1f";
        }
        return buf.c_str();
    }

}  // namespace

Display::Display(std::span<uint8_t> frameBuffer, const ObjectStack& stack, const std::vector<std::string>& menuRef)
    : _frameBuffer(frameBuffer)
    , _stack(stack)
    , _menuRef(menuRef)
{
    _bgCol = be32(backgroundColor);
    _fgOnCol = be32(foregroundOnColor);
    _fgOffCol = lerpU32Bytes(_fgOnCol, _bgCol, 220);
    _shadowCol = lerpU32Bytes(_fgOnCol, _bgCol, 120);
}

void Display::setError(std::pair<std::string, std::string> error)
{
    _error = error;
}

void Display::clearError()
{
    _error = {};
}

void Display::render(ghc::span2d<uint32_t> pixelBuffer, Mode mode, int scale)
{
    constexpr int xoffset = 2;
    constexpr int yoffset = 8;



    // Fill buffer with background color
    for (size_t y = 0; y < pixelBuffer.height(); ++y) {
        for (auto& i : pixelBuffer.row(y)) {
            i = _bgCol;
        }
    }

    renderSymbol(pixelBuffer, (xoffset + 6) * scale, 4, symbol0, symbol_widths[0], _leftShift ? _fgOnCol : _fgOffCol);
    renderSymbol(pixelBuffer, (xoffset + 28) * scale, 4, symbol1, symbol_widths[1], _rightShift ? _fgOnCol : _fgOffCol);
    renderSymbol(pixelBuffer, (xoffset + 51) * scale, 6, symbol2, symbol_widths[2], _alpha ? _fgOnCol : _fgOffCol);
    renderSymbol(pixelBuffer, (xoffset + 74) * scale, 6, symbol3, symbol_widths[3], _alarm ? _fgOnCol : _fgOffCol);
    if (_busy)
        renderSymbol(pixelBuffer, (xoffset + 96) * scale+1, 4+1, symbol4, symbol_widths[4], _shadowCol);
    renderSymbol(pixelBuffer, (xoffset + 96) * scale, 4, symbol4, symbol_widths[4], _busy ? _fgOnCol : _fgOffCol);
    renderSymbol(pixelBuffer, (xoffset + 118) * scale, 4, symbol5, symbol_widths[5], _transfer ? _fgOnCol : _fgOffCol);

    if (mode == Mode::Stack) {
        std::memset(_frameBuffer.data(), 0, _frameBuffer.size());
        if (!_error.first.empty()) {
            auto font = hp48::getFont(8);
            renderString(*font, _frameBuffer, 0, 0, _error.first);
            renderString(*font, _frameBuffer, 0, 8, _error.second);
        }
        else {
            auto font = hp48::getFont(6);
            renderString(*font, _frameBuffer, 0, 7, "{ HOME }");
            for (int x = 0; x < 17; ++x) {
                _frameBuffer[14*17 + x] = 255;
            }
        }
        {
            auto font = hp48::getFont(10);
            char buf[28];
            for (size_t level = 1; level <= 4; ++level) {
                snprintf(buf, sizeof(buf), "%lu:%20s", level, level <= _stack.size() ? formatObject(_stack.level(level)):"");
                renderString(*font, _frameBuffer, 0, 56u - level * 10, buf);
            }
        }
        for (size_t x = 0; x < 6; ++x) {
            invertRect(_frameBuffer, x*22, 57, 21, 7);
            if (x < _menuRef.size()) {
                auto width = calcTextWidth(*hp48::getFont(6), _menuRef[x]);
                renderString(*hp48::getFont(6), _frameBuffer, x*22 + (21 - width)/2, 58, _menuRef[x]);
            }
        }

    }
    auto bufferWidth = static_cast<int>(pixelBuffer.width());
    for (int y = 0; y < 64; ++y) {
        for (int x = 0; x < 131; ++x) {
            auto xpos = (x + xoffset) * scale;
            auto ypos = (y + yoffset) * scale;

            bool active = _frameBuffer[y * 17 + (x >> 3)] & (0x80 >> (x & 7));
            auto color = active ? _fgOnCol : _fgOffCol;

            // Set 3x3 pixels at xpos,ypos in pixelBuffer to color
            for (int dy = 0; dy < scale; ++dy) {
                for (int dx = 0; dx < scale; ++dx) {
                    int px = xpos + dx;
                    int py = ypos + dy;
                    if (px >= 0 && px < bufferWidth && py >= 0 && py < bufferWidth) {
                        if (dx < scale - 1 && dy < scale - 1)
                            pixelBuffer(py, px) = color;
                        else if (active && dx && dy)
                            pixelBuffer(py, px) = _shadowCol;
                    }
                }
            }
        }
    }
}

uint32_t Display::lerpU32Bytes(uint32_t a, uint32_t b, uint8_t mix)
{
    auto m = static_cast<uint32_t>(mix);
    auto im = 255u - m;
    auto lo_a = a & 0x00FF00FFu;
    auto hi_a = (a >> 8) & 0x00FF00FFu;
    auto lo_b = b & 0x00FF00FFu;
    auto hi_b = (b >> 8) & 0x00FF00FFu;
    auto lo = lo_a * im + lo_b * m + 0x00800080u;
    auto hi = hi_a * im + hi_b * m + 0x00800080u;
    lo = (lo + ((lo >> 8) & 0x00FF00FFu)) >> 8;
    hi = (hi + ((hi >> 8) & 0x00FF00FFu)) >> 8;
    return (lo & 0x00FF00FFu) | ((hi & 0x00FF00FFu) << 8);
}

}  // namespace hp48
