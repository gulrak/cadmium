//
// Created by schuemann on 15.08.22.
//

#include <raylib.h>

#include <raymath.h>

#include <cstdint>
#include <bitset>
#include <cmath>
#include <iostream>
#include <vector>

std::vector<Color> reference = {
    {0x55, 0x55, 0x55, 0xFF},
    {0xFF, 0xFF, 0xFF, 0xFF},
    {0xAA, 0xAA, 0xAA, 0xFF},
    {0x00, 0x00, 0x00, 0xFF},
    {0xFF, 0x00, 0x00, 0xFF},
    {0x00, 0xFF, 0x00, 0xFF},
    {0x00, 0x00, 0xFF, 0xFF},
    {0xFF, 0xFF, 0x00, 0xFF},
    {0x88, 0x00, 0x00, 0xFF},
    {0x00, 0x88, 0x00, 0xFF},
    {0x00, 0x00, 0x88, 0xFF},
    {0x88, 0x88, 0x00, 0xFF},
    {0xFF, 0x00, 0xFF, 0xFF},
    {0x00, 0xFF, 0xFF, 0xFF},
    {0x88, 0x00, 0x88, 0xFF},
    {0x00, 0x88, 0x88, 0xFF}
};

// sweetie-16
std::vector<Color> toSortSweet16 = {
    {0x1a, 0x1c, 0x2c, 0xff},
    {0x5d, 0x27, 0x5d, 0xff},
    {0xb1, 0x3e, 0x53, 0xff},
    {0xef, 0x7d, 0x57, 0xff},
    {0xff, 0xcd, 0x75, 0xff},
    {0xa7, 0xf0, 0x70, 0xff},
    {0x38, 0xb7, 0x64, 0xff},
    {0x25, 0x71, 0x79, 0xff},
    {0x29, 0x36, 0x6f, 0xff},
    {0x3b, 0x5d, 0xc9, 0xff},
    {0x41, 0xa6, 0xf6, 0xff},
    {0x73, 0xef, 0xf7, 0xff},
    {0xf4, 0xf4, 0xf4, 0xff},
    {0x94, 0xb0, 0xc2, 0xff},
    {0x56, 0x6c, 0x86, 0xff},
    {0x33, 0x3c, 0x57, 0xff}
};

// pico-8
std::vector<Color> toSortPico8 = {
    {0x00, 0x00, 0x00, 0xff},
    {0x1D, 0x2B, 0x53, 0xff},
    {0x7E, 0x25, 0x53, 0xff},
    {0x00, 0x87, 0x51, 0xff},
    {0xAB, 0x52, 0x36, 0xff},
    {0x5F, 0x57, 0x4F, 0xff},
    {0xC2, 0xC3, 0xC7, 0xff},
    {0xFF, 0xF1, 0xE8, 0xff},
    {0xFF, 0x00, 0x4D, 0xff},
    {0xFF, 0xA3, 0x00, 0xff},
    {0xFF, 0xEC, 0x27, 0xff},
    {0x00, 0xE4, 0x36, 0xff},
    {0x29, 0xAD, 0xFF, 0xff},
    {0x83, 0x76, 0x9C, 0xff},
    {0xFF, 0x77, 0xA8, 0xff},
    {0xFF, 0xCC, 0xAA, 0xff}
};

// c64
std::vector<Color> toSortC64 = {
    {0x00, 0x00, 0x00, 0xff},
    {0x62, 0x62, 0x62, 0xff},
    {0x89, 0x89, 0x89, 0xff},
    {0xad, 0xad, 0xad, 0xff},
    {0xff, 0xff, 0xff, 0xff},
    {0x9f, 0x4e, 0x44, 0xff},
    {0xcb, 0x7e, 0x75, 0xff},
    {0x6d, 0x54, 0x12, 0xff},
    {0xa1, 0x68, 0x3c, 0xff},
    {0xc9, 0xd4, 0x87, 0xff},
    {0x9a, 0xe2, 0x9b, 0xff},
    {0x5c, 0xab, 0x5e, 0xff},
    {0x6a, 0xbf, 0xc6, 0xff},
    {0x88, 0x7e, 0xcb, 0xff},
    {0x50, 0x45, 0x9b, 0xff},
    {0xa0, 0x57, 0xa3, 0xff},
};

// Intellivision
std::vector<Color> toSortIntelli = {
    {0x0c, 0x00, 0x05, 0xff},
    {0xa7, 0xa8, 0xa8, 0xff},
    {0xff, 0xfc, 0xff, 0xff},
    {0xff, 0x3e, 0x00, 0xff},
    {0xff, 0xa6, 0x00, 0xff},
    {0xfa, 0xea, 0x27, 0xff},
    {0x00, 0x78, 0x0f, 0xff},
    {0x00, 0xa7, 0x20, 0xff},
    {0x6c, 0xcd, 0x30, 0xff},
    {0x00, 0x2d, 0xff, 0xff},
    {0x5a, 0xcb, 0xff, 0xff},
    {0xbd, 0x95, 0xff, 0xff},
    {0xc8, 0x1a, 0x7d, 0xff},
    {0xff, 0x32, 0x76, 0xff},
    {0x3c, 0x58, 0x00, 0xff},
    {0xc9, 0xd4, 0x64, 0xff}
};

// CGA
std::vector<Color> toSortCGA = {
    {0x00, 0x00, 0x00, 0xff},
    {0x55, 0x55, 0x55, 0xff},
    {0xaa, 0xaa, 0xaa, 0xff},
    {0xff, 0xff, 0xff, 0xff},
    {0x00, 0x00, 0xaa, 0xff},
    {0x55, 0x55, 0xff, 0xff},
    {0x00, 0xaa, 0x00, 0xff},
    {0x55, 0xff, 0x55, 0xff},
    {0x00, 0xaa, 0xaa, 0xff},
    {0x55, 0xff, 0xff, 0xff},
    {0xaa, 0x00, 0x00, 0xff},
    {0xff, 0x55, 0x55, 0xff},
    {0xaa, 0x00, 0xaa, 0xff},
    {0xff, 0x55, 0xff, 0xff},
    {0xaa, 0x55, 0x00, 0xff},
    {0xff, 0xff, 0x55, 0xff}
};

// Macintosh II
std::vector<Color> toSortMacII = {
    {0xff, 0xff, 0xff, 0xff},
    {0xff, 0xff, 0x00, 0xff},
    {0xff, 0x65, 0x00, 0xff},
    {0xdc, 0x00, 0x00, 0xff},
    {0xff, 0x00, 0x97, 0xff},
    {0x36, 0x00, 0x97, 0xff},
    {0x00, 0x00, 0xca, 0xff},
    {0x00, 0x97, 0xff, 0xff},
    {0x00, 0xa8, 0x00, 0xff},
    {0x00, 0x65, 0x00, 0xff},
    {0x65, 0x36, 0x00, 0xff},
    {0x97, 0x65, 0x36, 0xff},
    {0xb9, 0xb9, 0xb9, 0xff},
    {0x86, 0x86, 0x86, 0xff},
    {0x45, 0x45, 0x45, 0xff},
    {0x00, 0x00, 0x00, 0xff}
};

// IBM PCjr
std::vector<Color> toSortIbmPCjr = {
    {0x03, 0x06, 0x25, 0xff},
    {0x00, 0x00, 0xe8, 0xff},
    {0x07, 0x7c, 0x35, 0xff},
    {0x02, 0x85, 0x66, 0xff},
    {0x9f, 0x24, 0x41, 0xff},
    {0x6b, 0x03, 0xca, 0xff},
    {0x4b, 0x74, 0x32, 0xff},
    {0x81, 0x89, 0x9e, 0xff},
    {0x1c, 0x25, 0x36, 0xff},
    {0x0e, 0x59, 0xf0, 0xff},
    {0x2c, 0xc6, 0x4e, 0xff},
    {0x0b, 0xc3, 0xa9, 0xff},
    {0xe8, 0x56, 0x85, 0xff},
    {0xc1, 0x37, 0xff, 0xff},
    {0xa7, 0xc2, 0x51, 0xff},
    {0xce, 0xd9, 0xed, 0xff}
};

// Daylight-16
std::vector<Color> toSortDaylight16 = {
    {0xf2, 0xd3, 0xac, 0xff},
    {0xe7, 0xa7, 0x6c, 0xff},
    {0xc2, 0x84, 0x62, 0xff},
    {0x90, 0x5b, 0x54, 0xff},
    {0x51, 0x3a, 0x3d, 0xff},
    {0x7a, 0x69, 0x77, 0xff},
    {0x87, 0x8c, 0x87, 0xff},
    {0xb5, 0xc6, 0x9a, 0xff},
    {0x27, 0x22, 0x23, 0xff},
    {0x60, 0x6b, 0x31, 0xff},
    {0xb1, 0x9e, 0x3f, 0xff},
    {0xf8, 0xc6, 0x5c, 0xff},
    {0xd5, 0x8b, 0x39, 0xff},
    {0x99, 0x63, 0x36, 0xff},
    {0x6a, 0x42, 0x2c, 0xff},
    {0xb5, 0x5b, 0x39, 0xff}
};

// Soul of the Sea
std::vector<Color> toSort = {
    {0x92, 0x50, 0x3f, 0xff},
    {0x70, 0x3a, 0x28, 0xff},
    {0x56, 0x45, 0x2b, 0xff},
    {0x40, 0x35, 0x21, 0xff},
    {0xcf, 0xbc, 0x95, 0xff},
    {0x94, 0x95, 0x76, 0xff},
    {0x81, 0x78, 0x4d, 0xff},
    {0x60, 0x5f, 0x33, 0xff},
    {0x7a, 0x7e, 0x67, 0xff},
    {0x93, 0xa3, 0x99, 0xff},
    {0x51, 0x67, 0x5a, 0xff},
    {0x2f, 0x48, 0x45, 0xff},
    {0x42, 0x59, 0x61, 0xff},
    {0x46, 0x7e, 0x73, 0xff},
    {0x01, 0x14, 0x1a, 0xff},
    {0x20, 0x36, 0x33, 0xff},
};

std::vector<Color> result;

static Vector3 rgbToXyz(Color c)
{
    float x, y, z, r, g, b;

    r = c.r / 255.0f; g = c.g / 255.0f; b = c.b / 255.0f;

    if (r > 0.04045f)
        r = std::pow(((r + 0.055f) / 1.055f), 2.4f);
    else r /= 12.92;

    if (g > 0.04045f)
        g = std::pow(((g + 0.055f) / 1.055f), 2.4f);
    else g /= 12.92;

    if (b > 0.04045f)
        b = std::pow(((b + 0.055f) / 1.055f), 2.4f);
    else b /= 12.92f;

    r *= 100; g *= 100; b *= 100;

    x = r * 0.4124f + g * 0.3576f + b * 0.1805f;
    y = r * 0.2126f + g * 0.7152f + b * 0.0722f;
    z = r * 0.0193f + g * 0.1192f + b * 0.9505f;

    return {x, y, z};
}

static Vector3 xyzToCIELAB(Vector3 c)
{
    float x, y, z, l, a, b;
    const float refX = 95.047f, refY = 100.0f, refZ = 108.883f;

    x = c.x / refX; y = c.y / refY; z = c.z / refZ;

    if (x > 0.008856f)
        x = powf(x, 1 / 3.0f);
    else x = (7.787f * x) + (16.0f / 116.0f);

    if (y > 0.008856f)
        y = powf(y, 1 / 3.0);
    else y = (7.787f * y) + (16.0f / 116.0f);

    if (z > 0.008856f)
        z = powf(z, 1 / 3.0);
    else z = (7.787f * z) + (16.0f / 116.0f);

    l = 116 * y - 16;
    a = 500 * (x - y);
    b = 200 * (y - z);

    return {l, a, b};
}

static float getColorDeltaE(Color c1, Color c2)
{
    Vector3 xyzC1 = rgbToXyz(c1), xyzC2 = rgbToXyz(c2);
    Vector3 labC1 = xyzToCIELAB(xyzC1), labC2 = xyzToCIELAB(xyzC2);
    return Vector3Distance(labC1, labC2);;
}

static float paletteDifference(const std::vector<Color>& p1, const std::vector<Color>& p2)
{
    float diff = 0;
    for(int i = 0; i < p1.size(); ++i) {
        diff += std::pow(getColorDeltaE(p1[i], p2[i]), 1.0f);
    }
    return diff;
}

std::vector<Color> sortColors(std::vector<Color>& ref, std::vector<Color> input)
{
    std::vector<Color> output;
    for(const auto& rc : ref) {
        int index = -1;
        float minDist;
        for(int i = 0; i < input.size(); ++i) {
            if(index < 0) {
                index = i;
                minDist = getColorDeltaE(rc, input[i]);
            }
            else if(getColorDeltaE(rc, input[i]) < minDist) {
                index = i;
                minDist = getColorDeltaE(rc, input[i]);
            }
        }
        output.push_back(input[index]);
        input.erase(input.begin() + index);
    }
    return output;
}

std::vector<Color> findBetter(std::vector<Color>& ref, std::vector<Color> input)
{
    float dRef = paletteDifference(ref, input);
    float dBest = 1.0E30f;
    for(int i = 0; i < 10000; ++i) {
        auto c1 = GetRandomValue(0,15);
        auto c2 = GetRandomValue(0,15);
        if(c1 != c2) {
            auto t = input;
            std::swap(t[c1], t[c2]);
            auto diff = paletteDifference(ref, t);
            if(diff < dBest) dBest = diff;
            if(diff < dRef) {
                input = t;
                dRef = diff;
            }
        }
    }
    TraceLog(LOG_INFO, "findBetter: %.3f / %.3f", dRef, dBest);
    return input;
}

void dumpPalette(const std::vector<Color>& p)
{
    std::cout << "{";
    for(int i = 0; i < p.size(); ++i) {
        std::cout << TextFormat("%s0x%06x", (i?", ":""), (uint32_t)ColorToInt(p[i])>>8);
    }
    std::cout << "}" << std::endl;
}

struct Palette {
    std::string name;
    uint32_t colors[16];
};

std::vector<Palette> palettes = {
    {"Silicon-8 1.0", 0x000000, 0xffffff, 0xaaaaaa, 0x555555, 0xff0000, 0x00ff00, 0x0000ff, 0xffff00, 0x880000, 0x008800, 0x000088, 0x888800, 0xff00ff, 0x00ffff, 0x880088, 0x008888},
    {"SWEETIE-16", 0x1a1c2c, 0xf4f4f4, 0x94b0c2, 0x333c57, 0xef7d57, 0xa7f070, 0x3b5dc9, 0xffcd75, 0xb13e53, 0x38b764, 0x29366f, 0x566c86, 0x41a6f6, 0x73eff7, 0x5d275d, 0x257179},
    {"PICO-8", 0x000000, 0xfff1e8, 0xc2c3c7, 0x5f574f, 0xff004d, 0x00e436, 0x29adff, 0xffec27, 0xab5236, 0x008751, 0x1d2b53, 0xffa300, 0xff77a8, 0xffccaa, 0x7e2553, 0x83769c},
    {"C64", 0x000000, 0xffffff, 0xadadad, 0x626262, 0xa1683c, 0x9ae29b, 0x887ecb, 0xc9d487, 0x9f4e44, 0x5cab5e, 0x50459b, 0x6d5412, 0xcb7e75, 0x6abfc6, 0xa057a3, 0x898989},
    {"Intellivision", 0x0c0005, 0xfffcff, 0xa7a8a8, 0x3c5800, 0xff3e00, 0x6ccd30, 0x002dff, 0xfaea27, 0xffa600, 0x00a720, 0xbd95ff, 0xc9d464, 0xff3276, 0x5acbff, 0xc81a7d, 0x00780f},
    {"CGA", 0x000000, 0xffffff, 0xaaaaaa, 0x555555, 0xff5555, 0x55ff55, 0x5555ff, 0xffff55, 0xaa0000, 0x00aa00, 0x0000aa, 0xaa5500, 0xff55ff, 0x55ffff, 0xaa00aa, 0x00aaaa},
    {"CGAb", 0x000000, 0xffffff, 0xaaaaaa, 0x555555, 0xff0000, 0x00ff00, 0x0000ff, 0xffff00, 0xaa0000, 0x00aa00, 0x0000aa, 0xaa5500, 0xff00ff, 0x00ffff, 0xaa00aa, 0x00aaaa},
    {"Macintosh II", 0x000000, 0xffffff, 0xb9b9b9, 0x454545, 0xdc0000, 0x00a800, 0x0000ca, 0xffff00, 0xff6500, 0x006500, 0x360097, 0x976536, 0xff0097, 0x0097ff, 0x653600, 0x868686},
    {"IBM PCjr", 0x1c2536, 0xced9ed, 0x81899e, 0x030625, 0xe85685, 0x2cc64e, 0x0000e8, 0xa7c251, 0x9f2441, 0x077c35, 0x0e59f0, 0x4b7432, 0xc137ff, 0x0bc3a9, 0x6b03ca, 0x028566},
    {"Daylight-16", 0x272223, 0xf2d3ac, 0xe7a76c, 0x6a422c, 0xb55b39, 0xb19e3f, 0x7a6977, 0xf8c65c, 0x996336, 0x606b31, 0x513a3d, 0xd58b39, 0xc28462, 0xb5c69a, 0x905b54, 0x878c87},
    {"Soul of the Sea", 0x01141a, 0xcfbc95, 0x93a399, 0x2f4845, 0x92503f, 0x949576, 0x425961, 0x81784d, 0x703a28, 0x7a7e67, 0x203633, 0x605f33, 0x56452b, 0x467e73, 0x403521, 0x51675a}
};

Color quantize(Color c)
{
    static uint8_t b3[] = {0, 0x20, 0x40, 0x60, 0x80, 0xA0, 0xC0, 0xff};
    static uint8_t b2[] = {0, 0x40, 0x80, 0xff};
    return {b3[c.r>>5], b3[c.g>>5], b2[c.b>>6], c.a};
    //return {uint8_t(c.r&0xf8), uint8_t(c.g&0xfc), uint8_t(c.b&0xf8), 0xff};
}

inline uint32_t rgb332To888(uint8_t c)
{
    if(c == 0b01001001) return 0x404040;
    if(c == 0b10010010) return 0x808080;
    if(c == 0b10110110) return 0xaaaaaa;
    if(c == 255) return 0xffffff;
    int r = (c & 0xe0) >> 5;
    int g = (c & 0x1c) >> 2;
    int b = (c & 3) << 1;
    return (r << 21) | (g << 13) | (b<<5);
}

inline uint32_t rgb332To888b(uint8_t c)
{
    static uint8_t b3[] = {0, 0x20, 0x40, 0x60, 0x80, 0xAf, 0xC0, 0xff};
    static uint8_t b2[] = {0, 0x43, 0x84, 0xff};
    if(c == 0b01001001) return 0x404040;
    if(c == 0b10010010) return 0x808080;
    if(c == 0b10110110) return 0xaaaaaa;
    if(c == 255) return 0xffffff;
    int r = (c & 0xe0) >> 5;
    int g = (c & 0x1c) >> 2;
    int b = (c & 3);// << 1;
    return (b3[r] << 16) | (b3[g] << 8) | (b2[b]);
}

inline uint32_t rgb332To888c(uint8_t c)
{
    if(c == 0) return 0;
    if(c == 0b01001001) return 0x404040;
    if(c == 0b01101101) return 0x606060;
    if(c == 0b10010010) return 0x808080;
    if(c == 0b10110110) return 0xaaaaaa;
    if(c == 255) return 0xffffff;
    return ((((c & 0xe0) >> 5) * 36 + 3)<<16) | ((((c & 0x1c) >> 2) * 36 + 3)<<8) | ((c & 3) * 85);
}

inline uint32_t rgb332To888d(uint8_t c)
{
    return (((c & 0xe0)<<16) | ((c & 0x1c) << 3)<<8) | ((c & 3) << 6);
}

uint32_t rgb332To888e(uint8_t c)
{
    //return (uint32_t(((c & 0xe0) >> 5) * 36.4286f)<<16) | (uint32_t(((c & 0x1c) >> 2) * 36.4286f)<<8) | ((c & 3) * 85);
    return (uint32_t((c & 0xe0) * 1.1384f)<<16) | (uint32_t((c & 0x1c) * 9.1074f)<<8) | ((c & 3) * 85);
}

inline uint32_t rgb332To888f(uint8_t c)
{
    static uint8_t b3[] = {0, 0x20, 0x40, 0x60, 0x80, 0xA0, 0xC0, 0xff};
    static uint8_t b2[] = {0, 0x60, 0xA0, 0xff};
    return (b3[(c & 0xe0) >> 5] << 16) | (b3[(c & 0x1c) >> 2] << 8) | (b2[c & 3]);
}

inline bool isGray(Color c, bool hard = false)
{
    if(hard)
        return c.r == c.g && c.r == c.b;
    return ColorToHSV(c).y < 0.1f;
}

inline Color quantizeRgb332(Color c)
{
    Color best = BLACK;
    float dBest = 1e30f;
    for(int i = 0; i < 256; ++i) {
        auto col = GetColor((rgb332To888(i) << 8) | 0xff);
        auto dist = getColorDeltaE(c, col);
        if(dist < dBest && isGray(c) == isGray(col, true)) {
            dBest = dist;
            best = col;
        }
    }
    return best;
}

inline Color quantizeRgb332b(Color c)
{
    Color best = BLACK;
    float dBest = 1e30f;
    for(int i = 0; i < 256; ++i) {
        auto col = GetColor((rgb332To888b(i) << 8) | 0xff);
        auto dist = getColorDeltaE(c, col);
        if(dist < dBest && isGray(c) == isGray(col, true)) {
            dBest = dist;
            best = col;
        }
    }
    return best;
}

inline std::pair<int,Color> quantizeRgb332c(Color c)
{
    Color best = BLACK;
    int bestRgb322 = 0;
    float dBest = 1e30f;
    for(int i = 0; i < 256; ++i) {
        auto col = GetColor((rgb332To888c(i) << 8) | 0xff);
        auto dist = getColorDeltaE(c, col);
        if(dist < dBest && isGray(c) == isGray(col, true)) {
            dBest = dist;
            best = col;
            bestRgb322 = i;
        }
    }
    return {bestRgb322, best};
}

inline std::pair<int,Color> quantizeRgb332f(Color c)
{
    Color best = BLACK;
    int bestRgb322 = 0;
    float dBest = 1e30f;
    for(int i = 0; i < 256; ++i) {
        auto col = GetColor((rgb332To888f(i) << 8) | 0xff);
        auto dist = getColorDeltaE(c, col);
        if(dist < dBest && isGray(c) == isGray(col, true)) {
            dBest = dist;
            best = col;
            bestRgb322 = i;
        }
    }
    return {bestRgb322, best};
}


int main(void)
{
    auto [ci,  c] = quantizeRgb332c(GetColor(0xaaaaaaff));
    auto colpair = quantizeRgb332f({0x99, 0x66, 0x00, 0xff});
    std::clog << "0: " << TextFormat("0x%08x (%d)", ColorToInt(colpair.second), colpair.first) << std::endl;
    colpair = quantizeRgb332f({0xFF, 0xCC, 0x00, 0xff});
    std::clog << "1: " << TextFormat("0x%08x (%d)", ColorToInt(colpair.second), colpair.first) << std::endl;
    colpair = quantizeRgb332f({0xFF, 0x66, 0x00, 0xff});
    std::clog << "2: " << TextFormat("0x%08x (%d)", ColorToInt(colpair.second), colpair.first) << std::endl;
    colpair = quantizeRgb332f({0x66, 0x22, 0x00, 0xff});
    std::clog << "3: " << TextFormat("0x%08x (%d)", ColorToInt(colpair.second), colpair.first) << std::endl;
    auto hsv = ColorToHSV(GetColor(0xfff1e8ff));
    for(int i = 0; i < 256; ++i) {
        std::cout << TextFormat("0x%06x%s", rgb332To888e(i), i!=255 ? ", " : "");
        if(i && i%8 == 7) {
            std::cout << std::endl;
        }
    }
    std::cout << std::endl;
    InitWindow(800, 800, "Color-Sort Test");
    SetTargetFPS(30);
    result.resize(16);
    result = sortColors(reference, toSort);
    dumpPalette(reference);
    dumpPalette(result);
    bool first = true;
    while (!WindowShouldClose())
    {
        if(IsKeyPressed(KEY_SPACE)) {
            result = findBetter(reference, result);
            dumpPalette(result);
        }
        BeginDrawing();
        ClearBackground(DARKGRAY);
        for(int i = 0; i < 16; ++i) {
            DrawRectangle((i/4)*32+8, (i%4)*32+8, 32, 32, reference[i]);
            DrawRectangle((i/4)*32+24+32*4*2, (i%4)*32+8, 32, 32, toSort[i]);
            DrawRectangle((i/4)*32+16+32*4, (i%4)*32+8, 32, 32, result[i]);
            DrawText(TextFormat("%.3f", paletteDifference(reference, result)), 16+32*4, 16 + 32*4, 20, WHITE);
        }

#if 1
        for(int i = 0; i < palettes.size(); ++i) {
            DrawText(palettes[i].name.c_str(), 8, 188 + i*40, 20, WHITE);
            if(first) {
                std::clog << "{\"" << palettes[i].name.c_str() << "\"";
            }
            for(int j = 0; j < 16; ++j) {
                auto color = GetColor((palettes[i].colors[j] << 8) | 0xff);
                auto [rgb332, quant] = quantizeRgb332f(color);
                if(first) {
                    std::clog << ", " << rgb332;
                }
                DrawRectangle(180 + j * 32, 180 + i * 40, 32, 16, color);
                DrawRectangle(180 + j * 32, 180 + i * 40+16, 32, 16, quant);
                /*
                if(isGray(color))
                    DrawRectangleLines(180 + j * 32, 180 + i * 40, 32, 16, j ? BLACK : WHITE);
                if(isGray(quant, true))
                    DrawRectangleLines(180 + j * 32, 180 + i * 40 + 16, 32, 16, j ? BLACK : WHITE);
                */
            }
            if(first)
                std::clog << "}" << std::endl;
        }
#else
        for(int i = 0; i < 256; ++i) {
            auto rgb = GetColor((rgb332To888f(i) << 8) | 0xff);
            auto hsv = ColorToHSV(rgb);
            if(first) std::clog << i << "  " << std::bitset<32>(ColorToInt(rgb)).to_string() << std::endl;
            DrawRectangle(10 + (i&0xf) * 32, 180 + (i>>4) * 32, 32, 32, rgb);
            //if(hsv.y < 0.145f && (std::fabs(hsv.z - 0.3333f) < 0.01f || std::fabs(hsv.z - 0.6667f) < 0.01f) ) {
            if(rgb.r == rgb.g && rgb.r == rgb.b) {
                if(first) std::cout << "Col: " << i << std::endl;
                DrawRectangleLines(10 + (i&0xf) * 32, 180 + (i>>4) * 32, 32, 32,i ? BLACK : WHITE);
            }
        }
#endif
        EndDrawing();
        first = false;
    }

    CloseWindow();

    return 0;
}