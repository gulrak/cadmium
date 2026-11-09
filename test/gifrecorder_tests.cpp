#include <doctest/doctest.h>

#include <gifrecorder.hpp>

#include <iostream>
#include <span>
#include <string>
#include <vector>

#include <fmt/format.h>
#include <ghc/gifimage.hpp>

namespace {

constexpr uint32_t BLACK_PIXEL = 0xFF000000u;
constexpr uint32_t WHITE_PIXEL = 0xFFFFFFFFu;
constexpr uint32_t RED_PIXEL = 0xFF0000FFu;

using Pixels = std::vector<uint32_t>;

struct Recording
{
    std::vector<uint8_t> bytes;
    GifRecorder recorder;
    Recording(int width, int height)
        : recorder(width, height, [this](std::span<const uint8_t> data) {
            bytes.insert(bytes.end(), data.begin(), data.end());
            return true;
        })
    {
    }
};

// The image every frame results in, applied on top of the one before like a viewer does.
std::vector<Pixels> compose(ghc::GifImage& gif)
{
    std::vector<Pixels> result;
    Pixels canvas(static_cast<size_t>(gif.width()) * gif.height(), 0);
    for(size_t n = 0; n < gif.numFrames(); ++n) {
        const auto& frame = gif.getFrame(n);
        REQUIRE(frame._controlExtension.has_value());
        REQUIRE(frame._pixels.size() == static_cast<size_t>(frame._width) * frame._height);
        size_t source = 0;
        for(int y = 0; y < frame._height; ++y) {
            for(int x = 0; x < frame._width; ++x, ++source) {
                const auto index = frame._pixels[source];
                if(frame._controlExtension->_transparency && index == frame._controlExtension->_transparentColor)
                    continue;
                REQUIRE(index < frame._palette.size());
                const uint32_t rgb = frame._palette[index];
                canvas[static_cast<size_t>(y + frame._top) * gif.width() + x + frame._left] = BLACK_PIXEL | (rgb >> 16 & 0xFF) | (rgb & 0xFF00) | (rgb & 0xFF) << 16;
            }
        }
        result.push_back(canvas);
    }
    return result;
}

Pixels pattern(int width, int height, int seed, int colors)
{
    Pixels pixels(static_cast<size_t>(width) * height);
    uint32_t state = 12345u + seed;
    for(auto& pixel : pixels) {
        state = state * 1664525u + 1013904223u;
        const uint32_t c = (state >> 16) % colors;
        pixel = BLACK_PIXEL | c * 3 | (c * 5 & 0xFF) << 8 | (c * 7 & 0xFF) << 16;
    }
    return pixels;
}

}  // namespace

TEST_CASE("GifRecorder frames decode to exactly the recorded pixels")
{
    constexpr int width = 96, height = 64;
    std::vector<Pixels> frames;
    auto pixels = pattern(width, height, 0, 16);
    frames.push_back(pixels);
    for(int f = 1; f < 12; ++f) {
        for(int i = 0; i < 40; ++i)
            pixels[static_cast<size_t>(f * 7 + i * 3) % pixels.size()] = i & 1 ? WHITE_PIXEL : RED_PIXEL;
        frames.push_back(pixels);
    }
    Recording recording(width, height);
    for(const auto& frame : frames)
        CHECK(recording.recorder.addFrame(frame.data(), width, 3));
    CHECK(recording.recorder.finish());

    ghc::GifImage gif(recording.bytes);
    REQUIRE(gif.isValid());
    REQUIRE(gif.numFrames() == frames.size());
    CHECK(compose(gif) == frames);
}

TEST_CASE("GifRecorder only stores the changed rectangle with unchanged pixels transparent")
{
    constexpr int width = 32, height = 16;
    Pixels pixels(width * height, BLACK_PIXEL);
    Recording recording(width, height);
    recording.recorder.addFrame(pixels.data(), width, 3);
    pixels[5 * width + 10] = WHITE_PIXEL;
    pixels[7 * width + 12] = RED_PIXEL;
    recording.recorder.addFrame(pixels.data(), width, 3);
    recording.recorder.finish();

    ghc::GifImage gif(recording.bytes);
    REQUIRE(gif.isValid());
    REQUIRE(gif.numFrames() == 2);
    const auto& frame = gif.getFrame(1);
    CHECK(frame._left == 10);
    CHECK(frame._top == 5);
    CHECK(frame._width == 3);
    CHECK(frame._height == 3);
    REQUIRE(frame._controlExtension.has_value());
    CHECK(frame._controlExtension->_transparency);
    CHECK(frame._pixels[1] == frame._controlExtension->_transparentColor);
    CHECK(compose(gif).back() == pixels);
}

TEST_CASE("GifRecorder extends the previous frame when nothing changed")
{
    constexpr int width = 16, height = 8;
    Pixels pixels(width * height, BLACK_PIXEL);
    Recording recording(width, height);
    recording.recorder.addFrame(pixels.data(), width, 3);
    recording.recorder.addFrame(pixels.data(), width, 3);
    recording.recorder.addFrame(pixels.data(), width, 4);
    pixels[0] = WHITE_PIXEL;
    recording.recorder.addFrame(pixels.data(), width, 5);
    recording.recorder.finish();

    ghc::GifImage gif(recording.bytes);
    REQUIRE(gif.isValid());
    REQUIRE(gif.numFrames() == 2);
    CHECK(gif.getFrame(0)._controlExtension->_delayTime == 10);
    CHECK(gif.getFrame(1)._controlExtension->_delayTime == 5);
}

TEST_CASE("GifRecorder continues with an empty frame when the delay field is full")
{
    constexpr int width = 8, height = 8;
    Pixels pixels(width * height, BLACK_PIXEL);
    Recording recording(width, height);
    recording.recorder.addFrame(pixels.data(), width, 60000);
    recording.recorder.addFrame(pixels.data(), width, 60000);
    recording.recorder.finish();

    ghc::GifImage gif(recording.bytes);
    REQUIRE(gif.isValid());
    REQUIRE(gif.numFrames() == 2);
    CHECK(gif.getFrame(0)._controlExtension->_delayTime == 60000);
    CHECK(gif.getFrame(1)._controlExtension->_delayTime == 60000);
    CHECK(gif.getFrame(1)._width == 1);
    CHECK(compose(gif).back() == pixels);
}

TEST_CASE("GifRecorder ignores alpha and honors the stride")
{
    constexpr int width = 4, height = 2;
    const Pixels bottomUp{5, 6, 7, 8, 0xAA, 0xAA, 1, 2, 3, 4, 0xAA, 0xAA};
    Recording recording(width, height);
    recording.recorder.addFrame(bottomUp.data() + 6, -6, 3);
    recording.recorder.finish();

    ghc::GifImage gif(recording.bytes);
    REQUIRE(gif.isValid());
    const Pixels expected{BLACK_PIXEL | 1, BLACK_PIXEL | 2, BLACK_PIXEL | 3, BLACK_PIXEL | 4, BLACK_PIXEL | 5, BLACK_PIXEL | 6, BLACK_PIXEL | 7, BLACK_PIXEL | 8};
    CHECK(compose(gif).front() == expected);
}

TEST_CASE("GifRecorder stays valid when the LZW dictionary overflows")
{
    constexpr int width = 300, height = 300;
    const auto pixels = pattern(width, height, 1, 256);
    Recording recording(width, height);
    recording.recorder.addFrame(pixels.data(), width, 3);
    recording.recorder.finish();

    ghc::GifImage gif(recording.bytes);
    REQUIRE(gif.isValid());
    REQUIRE(gif.numFrames() == 1);
    CHECK(compose(gif).front() == pixels);
}

TEST_CASE("GifRecorder reduces frames with more than 256 colors")
{
    constexpr int width = 128, height = 128;
    Pixels pixels(width * height);
    for(int y = 0; y < height; ++y) {
        for(int x = 0; x < width; ++x)
            pixels[static_cast<size_t>(y) * width + x] = BLACK_PIXEL | (x * 2) | (y * 2) << 8 | ((x + y) & 0xFF) << 16;
    }
    Recording recording(width, height);
    recording.recorder.addFrame(pixels.data(), width, 3);
    recording.recorder.finish();

    ghc::GifImage gif(recording.bytes);
    REQUIRE(gif.isValid());
    REQUIRE(gif.numFrames() == 1);
    CHECK(gif.getFrame(0)._palette.size() <= 256);
    const auto decoded = compose(gif).front();
    int worst = 0;
    for(size_t i = 0; i < pixels.size(); ++i) {
        for(int shift = 0; shift < 24; shift += 8)
            worst = std::max(worst, std::abs(static_cast<int>((pixels[i] >> shift) & 0xFF) - static_cast<int>((decoded[i] >> shift) & 0xFF)));
    }
    CHECK(worst <= 85);
}

TEST_CASE("GifRecorder reports a failing sink")
{
    constexpr int width = 8, height = 8;
    Pixels pixels(width * height, WHITE_PIXEL);
    bool accept = true;
    GifRecorder recorder(width, height, [&](std::span<const uint8_t>) { return accept; });
    CHECK(recorder.addFrame(pixels.data(), width, 3));
    accept = false;
    pixels[0] = BLACK_PIXEL;
    CHECK_FALSE(recorder.addFrame(pixels.data(), width, 3));
    CHECK_FALSE(recorder.good());
    CHECK_FALSE(recorder.addFrame(pixels.data(), width, 3));
}

TEST_CASE("GifFramePacer turns 60 Hz into 30 fps with exact timing")
{
    GifFramePacer pacer;
    std::vector<int> delays;
    for(int i = 0; i < 601; ++i) {
        if(const auto delay = pacer.advance(i ? 1.0 / 60.0 : 0.0))
            delays.push_back(delay);
    }
    REQUIRE(delays.size() == 301);
    CHECK(delays[1] == 3);
    CHECK(delays[2] == 3);
    CHECK(delays[3] == 4);
    int sum = 0;
    for(size_t i = 1; i < delays.size(); ++i)
        sum += delays[i];
    CHECK(sum == 1000);
}

TEST_CASE("GifClickMarks ages clicks and drops them when their animation is over")
{
    GifClickMarks marks;
    marks.add(10.0f, 20.0f);
    marks.advance(GifClickMarks::DURATION * 0.5f);
    marks.add(30.0f, 40.0f);
    REQUIRE(marks.marks().size() == 2);
    CHECK(marks.marks()[0].x == doctest::Approx(10.0f));
    CHECK(marks.marks()[0].age == doctest::Approx(GifClickMarks::DURATION * 0.5f));
    CHECK(marks.marks()[1].age == doctest::Approx(0.0f));
    marks.advance(GifClickMarks::DURATION * 0.6f);
    REQUIRE(marks.marks().size() == 1);
    CHECK(marks.marks()[0].y == doctest::Approx(40.0f));
    marks.clear();
    CHECK(marks.marks().empty());
}

TEST_CASE("scaleNearest handles integer and non-uniform scaling")
{
    const Pixels source{1, 2, 9, 3, 4, 9};
    Pixels doubled(16);
    scaleNearest(source.data(), 2, 2, 3, doubled.data(), 4, 4);
    CHECK(doubled == Pixels{1, 1, 2, 2, 1, 1, 2, 2, 3, 3, 4, 4, 3, 3, 4, 4});
    Pixels halfHeight(2);
    scaleNearest(source.data(), 2, 2, 3, halfHeight.data(), 2, 1);
    CHECK(halfHeight == Pixels{1, 2});
}
