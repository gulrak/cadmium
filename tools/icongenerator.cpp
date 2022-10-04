#include <raylib.h>
#include <string>
#include <cstring>

#if (defined(__APPLE__) && defined(__MACH__))
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#define IS_MACOS
#endif

static void drawMicroText(Image& dest, Image font, std::string text, int x, int y, Color tint)
{
    for(auto c : text) {
        if((uint8_t)c < 128)
            ImageDraw(&dest, font, {(c%32)*4.0f, (c/32)*6.0f, 4, 6}, {(float)x, (float)y, 4, 6}, tint);
        x += 4;
    }
}

void exportMacOS(Image icon, int size, bool withRetina)
{
    auto img = ImageCopy(icon);
    if(size < icon.width)
        ImageResize(&img, size, size);
    else
        ImageResizeNN(&img, size, size);
    ExportImage(img, ("icon_" + std::to_string(size) + "x" + std::to_string(size) + ".png").c_str());
    if(withRetina)
        ExportImage(img, ("icon_" + std::to_string(size/2) + "x" + std::to_string(size/2) + "@2x.png").c_str());
    UnloadImage(img);
}

int main(int argc, char* argv[])
{
    SetTraceLogLevel(LOG_NONE);
    Image title = LoadImage(argv[1]);
    Image font = LoadImage(argv[2]);
    drawMicroText(title, font, "v" CADMIUM_VERSION, 91 - std::strlen("v" CADMIUM_VERSION)*4, 6, WHITE);
    drawMicroText(title, font, "Beta", 38, 53, WHITE);
    std::string buildDate = __DATE__;
    auto dateText = buildDate.substr(0, 3);
    bool shortDate = (buildDate[4] == ' ');
    drawMicroText(title, font, buildDate.substr(9), 83, 53, WHITE);
    drawMicroText(title, font, buildDate.substr(4,2), 75, 52, WHITE);
    drawMicroText(title, font, buildDate.substr(0,3), shortDate ? 67 : 63, 53, WHITE);
    ImageColorReplace(&title, {0,0,0,255}, {0x1a,0x1c,0x2c,0xff});
    ImageColorReplace(&title, {255,255,255,255}, {0x51,0xbf,0xd3,0xff});
    Image icon = GenImageColor(64,64,{0,0,0,0});
    ImageDraw(&icon, title, {34,2,60,60}, {2,2,60,60}, WHITE);

    ExportImage(icon, argv[3]);

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