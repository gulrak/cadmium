//---------------------------------------------------------------------------------------
// src/imageconv.cpp
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
#include <ghc/cli.hpp>
#include <fmt/format.h>
#include <raylib.h>
#include <iostream>
#include <fstream>
#include <set>

static bool CheckContactRects(Rectangle rec1, Rectangle rec2)
{
    bool collision = false;

    if ((rec1.x <= (rec2.x + rec2.width+2) && (rec1.x + rec1.width + 2) >= rec2.x) &&
        (rec1.y <= (rec2.y + rec2.height+2) && (rec1.y + rec1.height + 2) >= rec2.y)) collision = true;

    return collision;
}

#define COLOR_EQUAL(col1, col2) ((col1.r == col2.r)&&(col1.g == col2.g)&&(col1.b == col2.b)&&(col1.a == col2.a))

int main(int argc, char* argv[])
{
    ghc::CLI cli(argc, argv);
    std::vector<std::string> files;
    std::string output;
    std::ofstream outputStream;
    cli.option({"-o", "--output"}, output, "output file");
    cli.positional(files, "files to convert");
    cli.parse();

    if(!output.empty() && output != "-") {
        outputStream.open(output);
        if(outputStream.fail()) {
            std::cerr << "ERROR: Couldn't open output file '" << output << "'" << std::endl;
            exit(1);
        }
    }
    if(files.size() > 1) {
        std::cerr << "ERROR: Multiple source images are not supported yet!" << std::endl;
        exit(1);
    }

    std::ostream& out = output.empty() ? std::cout : outputStream;
    std::set<Color> colors;
    std::vector<Rectangle> images;
    int maxPaletteSize = 255;
    for(const auto& file : files) {
        std::clog << file << std::endl;
        int colorCount = 0;
        auto img = LoadImage(file.c_str());
        auto* palette = LoadImagePalette(img, maxPaletteSize, &colorCount);
        out << ": mc_palette # " << colorCount << " colors" << std::endl;
        for(int i = 0; i < colorCount; ++i) {
            auto col = palette[i];
            out << fmt::format("    0x{:02x} 0x{:02x} 0x{:02x} 0x{:02x}", col.a, col.r, col.g, col.b) << std::endl;
        }
        for(int y = 0; y < img.height; ++y) {
            for(int x = 0; x < img.width; ++x) {
                auto col = GetImageColor(img, x, y);
                if(col.a > 200) {
                    bool found = false;
                    for(auto& rect : images) {
                        if(y >= rect.y && y <= rect.y + rect.height && x + 1.0f >= rect.x && x <= rect.x + rect.width) {
                            rect.height = y - rect.y + 1;
                            if(x < rect.x)
                                rect.x = x;
                            else if(x == rect.x + rect.width)
                                rect.width = x - rect.x + 1;
                            found = true;
                        }
                    }
                    if(!found) {
                        images.push_back({(float)x,(float)y,1.0f,1.0f});
                    }
                }
            }
        }
        std::clog << "found " << images.size() << " rectangles" << std::endl;
        std::vector<Rectangle> result;
        for(auto& rect : images) {
            if(rect.width > 0) {
                auto newRect = rect;
                rect.width = -1;
                for(auto& r2 : images) {
                    if(r2.width > 0 && CheckContactRects(newRect, r2)) {
                        auto newX = std::min(newRect.x, r2.x);
                        auto newY = std::min(newRect.y, r2.y);
                        auto newW = std::max(newRect.x + newRect.width, r2.x + r2.width) - newX;
                        auto newH = std::max(newRect.y + newRect.height, r2.y + r2.height) - newY;
                        newRect = {newX, newY, newW, newH};
                        r2.width = -1;
                    }
                }
                result.push_back(newRect);
            }
        }
        for(auto& rect : result) {
            out << fmt::format("\n: mc_sprite_{}_{} # size {}x{}", rect.x, rect.y, rect.width, rect.height) << std::endl;
            for(int y = rect.y; y < rect.y + rect.height; ++y) {
                out << "   ";
                for(int x = rect.x; x < rect.x + rect.width; ++x) {
                    auto col = GetImageColor(img, x, y);
                    if(col.a <= 200)
                        out << " 0x00";
                    else {
                        for (int i = 0; i < colorCount; ++i) {
                            if(COLOR_EQUAL(palette[i],col)) {
                                out << fmt::format(" 0x{:02x}", i + 1);
                                break;
                            }
                        }
                    }
                }
                out << std::endl;
            }
        }
        // ExportImage(img, "image.png");
        std::clog << "found " << result.size() << " rectangles after merge" << std::endl;
        UnloadImagePalette(palette);
        UnloadImage(img);
    }
}
