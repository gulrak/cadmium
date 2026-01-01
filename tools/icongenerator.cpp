#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <nothings/stb_image.h>
#include <nothings/stb_image_write.h>
#include <nothings/stb_image_resize2.h>

#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>

#if (defined(__APPLE__) && defined(__MACH__))
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#define IS_MACOS
#endif

// Simple Color structure
struct Color {
    uint8_t r, g, b, a;
};

const Color WHITE = {255, 255, 255, 255};

// Rectangle structure for image operations
struct Rectangle {
    float x, y, width, height;
};

// Image structure matching raylib's basic layout
struct Image {
    uint8_t* data;
    int width;
    int height;

    Image() : data(nullptr), width(0), height(0) {}
    Image(int w, int h) : width(w), height(h) {
        data = new uint8_t[w * h * 4];
    }
};

// Load image from file (always as RGBA)
Image LoadImage(const char* fileName) {
    Image img;
    int channels;
    img.data = stbi_load(fileName, &img.width, &img.height, &channels, 4); // Force RGBA
    if (!img.data) {
        img.width = 0;
        img.height = 0;
    }
    return img;
}

// Free image memory
void UnloadImage(Image& img) {
    if (img.data) {
        stbi_image_free(img.data);
        img.data = nullptr;
    }
}

// Create a copy of an image
Image ImageCopy(const Image& src) {
    Image img;
    img.width = src.width;
    img.height = src.height;
    size_t dataSize = src.width * src.height * 4;
    img.data = new uint8_t[dataSize];
    std::memcpy(img.data, src.data, dataSize);
    return img;
}

// Generate image filled with color
Image GenImageColor(int width, int height, Color color) {
    Image img(width, height);
    uint8_t* pixel = img.data;
    for (int i = 0; i < width * height; i++) {
        pixel[0] = color.r;
        pixel[1] = color.g;
        pixel[2] = color.b;
        pixel[3] = color.a;
        pixel += 4;
    }
    return img;
}

// Resize image with bilinear filtering (high quality)
void ImageResize(Image* img, int newWidth, int newHeight) {
    uint8_t* resized = new uint8_t[newWidth * newHeight * 4];

    stbir_resize_uint8_linear(
        img->data, img->width, img->height, 0,
        resized, newWidth, newHeight, 0,
        STBIR_RGBA
    );

    delete[] img->data;
    img->data = resized;
    img->width = newWidth;
    img->height = newHeight;
}

// Resize image with nearest neighbor (pixel art)
void ImageResizeNN(Image* img, int newWidth, int newHeight) {
    uint8_t* resized = new uint8_t[newWidth * newHeight * 4];

    // Use advanced API for the true nearest neighbor (point sampling)
    stbir_resize(
        img->data, img->width, img->height, 0,
        resized, newWidth, newHeight, 0,
        STBIR_RGBA,
        STBIR_TYPE_UINT8,
        STBIR_EDGE_CLAMP,
        STBIR_FILTER_POINT_SAMPLE  // True nearest neighbor
    );

    delete[] img->data;
    img->data = resized;
    img->width = newWidth;
    img->height = newHeight;
}

// Replace a specific color in the image
void ImageColorReplace(Image* img, Color oldColor, Color newColor) {
    uint8_t* pixel = img->data;
    for (int i = 0; i < img->width * img->height; i++) {
        if (pixel[0] == oldColor.r && pixel[1] == oldColor.g &&
            pixel[2] == oldColor.b && pixel[3] == oldColor.a) {
            pixel[0] = newColor.r;
            pixel[1] = newColor.g;
            pixel[2] = newColor.b;
            pixel[3] = newColor.a;
        }
        pixel += 4;
    }
}

// Alpha blending helper
inline uint8_t alphaBlend(uint8_t src, uint8_t dst, uint8_t srcAlpha, uint8_t dstAlpha) {
    if (srcAlpha == 255) return src;
    if (srcAlpha == 0) return dst;
    float srcA = srcAlpha / 255.0f;
    float dstA = dstAlpha / 255.0f;
    float outA = srcA + dstA * (1.0f - srcA);
    if (outA == 0.0f) return 0;
    return static_cast<uint8_t>((src * srcA + dst * dstA * (1.0f - srcA)) / outA);
}

// Draw one image onto another with source and destination rectangles
void ImageDraw(Image* dst, const Image& src, Rectangle srcRec, Rectangle dstRec, Color tint) {
    // Clamp source rectangle to source image bounds
    if (srcRec.x < 0) { dstRec.width += srcRec.x * (dstRec.width / srcRec.width); dstRec.x -= srcRec.x * (dstRec.width / srcRec.width); srcRec.width += srcRec.x; srcRec.x = 0; }
    if (srcRec.y < 0) { dstRec.height += srcRec.y * (dstRec.height / srcRec.height); dstRec.y -= srcRec.y * (dstRec.height / srcRec.height); srcRec.height += srcRec.y; srcRec.y = 0; }
    if (srcRec.x + srcRec.width > src.width) srcRec.width = src.width - srcRec.x;
    if (srcRec.y + srcRec.height > src.height) srcRec.height = src.height - srcRec.y;

    // Draw pixel by pixel with scaling and tinting
    for (int dy = 0; dy < (int)dstRec.height; dy++) {
        for (int dx = 0; dx < (int)dstRec.width; dx++) {
            int destX = (int)(dstRec.x + dx);
            int destY = (int)(dstRec.y + dy);

            if (destX < 0 || destX >= dst->width || destY < 0 || destY >= dst->height)
                continue;

            // Map destination pixel to source pixel
            float srcX = srcRec.x + (dx / dstRec.width) * srcRec.width;
            float srcY = srcRec.y + (dy / dstRec.height) * srcRec.height;

            int sx = (int)srcX;
            int sy = (int)srcY;

            if (sx < 0 || sx >= src.width || sy < 0 || sy >= src.height)
                continue;

            // Get source and destination pixels
            const uint8_t* srcPixel = src.data + (sy * src.width + sx) * 4;
            uint8_t* dstPixel = dst->data + (destY * dst->width + destX) * 4;

            // Apply tint
            uint8_t r = (srcPixel[0] * tint.r) / 255;
            uint8_t g = (srcPixel[1] * tint.g) / 255;
            uint8_t b = (srcPixel[2] * tint.b) / 255;
            uint8_t a = (srcPixel[3] * tint.a) / 255;

            // Alpha blend
            if (a == 255) {
                dstPixel[0] = r;
                dstPixel[1] = g;
                dstPixel[2] = b;
                dstPixel[3] = 255;
            } else if (a > 0) {
                dstPixel[0] = alphaBlend(r, dstPixel[0], a, dstPixel[3]);
                dstPixel[1] = alphaBlend(g, dstPixel[1], a, dstPixel[3]);
                dstPixel[2] = alphaBlend(b, dstPixel[2], a, dstPixel[3]);
                dstPixel[3] = std::min(255, dstPixel[3] + a);
            }
        }
    }
}

// Export image to PNG file
void ExportImage(const Image& img, const char* fileName) {
    stbi_write_png(fileName, img.width, img.height, 4, img.data, img.width * 4);
}

// Draw micro text using bitmap font
static void drawMicroText(Image& dest, const Image& font, std::string text, int x, int y, Color tint) {
    for (auto c : text) {
        if ((uint8_t)c < 128)
            ImageDraw(&dest, font, {(c % 32) * 4.0f, (c / 32) * 6.0f, 4, 6}, {(float)x, (float)y, 4, 6}, tint);
        x += 4;
    }
}

void exportMacOS(const Image& icon, int size, bool withRetina) {
    auto img = ImageCopy(icon);
    if (size < icon.width)
        ImageResize(&img, size, size);
    else
        ImageResizeNN(&img, size, size);
    ExportImage(img, ("icon_" + std::to_string(size) + "x" + std::to_string(size) + ".png").c_str());
    if (withRetina)
        ExportImage(img, ("icon_" + std::to_string(size / 2) + "x" + std::to_string(size / 2) + "@2x.png").c_str());
    UnloadImage(img);
}

struct BitmapInfoHeader {
    uint32_t biSize;
    int32_t biWidth;
    int32_t biHeight;
    uint16_t biPlanes;
    uint16_t biBitCount;
    uint32_t biCompression;
    uint32_t biSizeImage;
    int32_t biXPelsPerMeter;
    int32_t biYPelsPerMeter;
    uint32_t biClrUsed;
    uint32_t biClrImportant;
};

struct IconDirEntry {
    uint8_t bWidth;
    uint8_t bHeight;
    uint8_t bColorCount;
    uint8_t bReserved;
    uint16_t wPlanes;
    uint16_t wBitCount;
    uint32_t dwBytesInRes;
    uint32_t dwImageOffset;
};

struct IconDir {
    uint16_t idReserved;
    uint16_t idType;
    uint16_t idCount;
};

void exportWindows(uint8_t* imageData, int size) {
    int resultSize = 6 + (16 + sizeof(BitmapInfoHeader)) + 4 * size * size;
    std::vector<char> icoFileData(resultSize, 0);
    char* imageDataStart = icoFileData.data() + 6 + 16;
    auto* icondir = (IconDir*)icoFileData.data();
    icondir->idReserved = 0;
    icondir->idType = 1;
    icondir->idCount = 1;

    char* dest = imageDataStart;
    uint8_t* src = imageData;
    int bytes = size * size * 4 + sizeof(BitmapInfoHeader);
    auto* entry = (IconDirEntry*)(icoFileData.data() + 6);
    entry->bWidth = (uint8_t)size;
    entry->bHeight = (uint8_t)size;
    entry->bColorCount = 0;
    entry->bReserved = 0;
    entry->wPlanes = 1;
    entry->wBitCount = (unsigned short)(4 * 8);
    entry->dwBytesInRes = (uint32_t)bytes;

    char* imageDest = dest + sizeof(BitmapInfoHeader);
    std::memcpy(imageDest, imageData, size * size * 4);
    auto* header = (BitmapInfoHeader*)dest;
    std::memset(header, 0, sizeof(BitmapInfoHeader));
    header->biSize = sizeof(BitmapInfoHeader);
    header->biWidth = size;
    header->biHeight = size * 2;
    header->biPlanes = 1;
    header->biBitCount = (4 * 8);
    header->biSizeImage = 0;

    entry->dwImageOffset = (uint32_t)(dest - icoFileData.data());
    dest += entry->dwBytesInRes;

    std::ofstream os("cadmium.ico", std::ios::binary);
    os.write(icoFileData.data(), resultSize);
    os.close();
}

int main(int argc, char* argv[])
{
    if (argc < 4) {
        return 1;
    }

    Image title = LoadImage(argv[1]);
    Image font = LoadImage(argv[2]);

#ifndef WIN32
    bool win32 = false;
    if (argc >= 5 && std::string(argv[4]) == "--win32") {
        win32 = true;
    }
#endif
    std::string versionStr(CADMIUM_VERSION);
    drawMicroText(title, font, "v" CADMIUM_VERSION, 91 - std::strlen("v" CADMIUM_VERSION) * 4, 6, WHITE);

    if (!versionStr.empty() && (versionStr.back() & 1))
        drawMicroText(title, font, "WIP", 38, 53, WHITE);

    std::string buildDate = __DATE__;
    auto dateText = buildDate.substr(0, 3);
    bool shortDate = (buildDate[4] == ' ');
    drawMicroText(title, font, buildDate.substr(9), 83, 53, WHITE);
    drawMicroText(title, font, buildDate.substr(4, 2), 75, 52, WHITE);
    drawMicroText(title, font, buildDate.substr(0, 3), shortDate ? 67 : 63, 53, WHITE);

    ImageColorReplace(&title, {0, 0, 0, 255}, {0x1a, 0x1c, 0x2c, 0xff});
    ImageColorReplace(&title, {255, 255, 255, 255}, {0x51, 0xbf, 0xd3, 0xff});

    Image icon = GenImageColor(64, 64, {0, 0, 0, 0});
    ImageDraw(&icon, title, {34, 2, 60, 60}, {2, 2, 60, 60}, WHITE);

    ExportImage(icon, argv[3]);

#ifdef WIN32
    bool win32 = true;
#endif
    if (win32) {
        auto ico = ImageCopy(icon);
        {
            auto size = ico.width;
            std::vector<uint8_t> swappedData(size * size * 4, 0);
            int stride = size * 4;

            for (int j = 0; j < size; j++) {
                uint8_t* src_line = (uint8_t*)ico.data + j * stride;
                uint8_t* dest_line = swappedData.data() + (size - 1 - j) * stride;

                uint8_t* src = src_line;
                uint8_t* dest = dest_line;
                for (int i = 0; i < size; i++) {
                    dest[2] = src[0];
                    dest[1] = src[1];
                    dest[0] = src[2];
                    dest[3] = src[3];

                    src += 4;
                    dest += 4;
                }
            }
            exportWindows(swappedData.data(), ico.width);
        }
        UnloadImage(ico);
    }

#ifdef IS_MACOS
    mkdir("cadmium.iconset", 0700);
    chdir("cadmium.iconset");
    exportMacOS(icon, 512, true);
    exportMacOS(icon, 256, true);
    exportMacOS(icon, 128, false);
    exportMacOS(icon, 64, false);
    exportMacOS(icon, 32, true);
    exportMacOS(icon, 16, false);
#endif

    UnloadImage(icon);
    UnloadImage(font);
    UnloadImage(title);
    return 0;
}