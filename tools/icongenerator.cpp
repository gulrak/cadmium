#include <raylib.h>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#if (defined(__APPLE__) && defined(__MACH__))
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#define IS_MACOS
#endif

static void drawMicroText(Image& dest, Image font, std::string text, int x, int y, Color tint)
{
    for (auto c : text) {
        if ((uint8_t)c < 128)
            ImageDraw(&dest, font, {(c % 32) * 4.0f, (c / 32) * 6.0f, 4, 6}, {(float)x, (float)y, 4, 6}, tint);
        x += 4;
    }
}

void exportMacOS(Image icon, int size, bool withRetina)
{
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

#ifdef WIN32

struct BitmapInfoHeader
{
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

struct IconDirEntry
{
    uint8_t bWidth;
    uint8_t bHeight;
    uint8_t bColorCount;
    uint8_t bReserved;
    uint16_t wPlanes;
    uint16_t wBitCount;
    uint32_t dwBytesInRes;
    uint32_t dwImageOffset;
};

struct IconDir
{
    uint16_t idReserved;
    uint16_t idType;
    uint16_t idCount;
};

void exportWindows(uint8_t* imageData, int size)
{
    int resultSize = 6 + (16 + sizeof(BitmapInfoHeader)) + 4 * size * size;
    std::vector<char> icoFileData(resultSize, 0);
    // char* resultData = (char*)malloc(sizeof(char) * resultSize);
    char* imageDataStart = icoFileData.data() + 6 + 16;
    auto* icondir = (IconDir*)icoFileData.data();
    icondir->idReserved = 0;
    icondir->idType = 1;
    icondir->idCount = 1;

    char* dest = imageDataStart;
    char* lastDest = 0;
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

    lastDest = dest;
    dest += entry->dwBytesInRes;

    std::ofstream os("cadmium.ico");
    os.write(icoFileData.data(), resultSize);
    os.close();
}

#endif

int main(int argc, char* argv[])
{
    SetTraceLogLevel(LOG_NONE);
    Image title = LoadImage(argv[1]);
    Image font = LoadImage(argv[2]);
    std::string versionStr(CADMIUM_VERSION);
    drawMicroText(title, font, "v" CADMIUM_VERSION, 91 - std::strlen("v" CADMIUM_VERSION) * 4, 6, WHITE);
    if(!versionStr.empty() && (versionStr.back() & 1))
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
    auto ico = ImageCopy(icon);
    {
        auto size = ico.width;
        std::vector<uint8_t> swappedData(size * size * 4, 0);
        int pixels = size * size;
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
#endif

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
