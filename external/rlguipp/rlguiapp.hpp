//---------------------------------------------------------------------------------------
//
// Copyright (c) 2024, Steffen Schümann <s.schuemann@pobox.com>
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
// rlguiapp.hpp — Optional boilerplate-reduction base class for rlguipp applications.
//
// Usage:
//   1. Subclass gui::Application.
//   2. Override drawGui() with your widget code — BeginGui/EndGui are handled for you.
//   3. Call update() from your main loop.
//
//   class MyApp : public gui::Application {
//   public:
//       MyApp() : gui::Application("My App", {640, 480, 2}) { ... }
//   protected:
//       void drawGui() override { /* widget calls here */ }
//   };
//
//   int main() {
//       MyApp app;
//       while (!app.shouldClose()) app.update();
//   }
//
// This header is entirely optional — all other rlguipp interfaces are unaffected.
//---------------------------------------------------------------------------------------
#pragma once
#include <rlguipp/rlguipp.hpp>

namespace gui {

/// Configuration passed to Application's constructor. Defined outside the class
/// so that `Config cfg = {}` in the constructor declaration is valid (C++ cannot
/// reference a nested struct's default member initializers in a default argument
/// while the enclosing class is still incomplete).
struct ApplicationConfig {
    int          logicalWidth  = 640;   ///< render-texture width in logical pixels
    int          logicalHeight = 480;   ///< render-texture height in logical pixels
    int          scale         = 2;     ///< integer pixel scale (window = logical * scale)
    int          targetFps     = 60;
    unsigned int windowFlags   = 0;     ///< raylib SetConfigFlags bits
};

class Application {
public:
    using Config = ApplicationConfig;

    explicit Application(const char* title, Config cfg = {});
    virtual ~Application();

    Application(const Application&)            = delete;
    Application& operator=(const Application&) = delete;

    /// Returns true when the window's close button was pressed or requestClose() was called.
    bool shouldClose() const;

    /// Runs one frame: adjusts scale if needed, renders to texture, blits to screen.
    void update();

    /// Export the current render texture to a PNG file (for headless testing).
    void exportFrame(const char* path);

protected:
    /// Override to provide your GUI. Called every frame inside an active BeginGui/EndGui
    /// pair — you can call widget functions directly without any additional setup.
    virtual void drawGui() = 0;

    /// Signal that the application should exit on the next shouldClose() check.
    void requestClose() { _shouldClose = true; }

    int logicalWidth()  const { return _cfg.logicalWidth; }
    int logicalHeight() const { return _cfg.logicalHeight; }
    int currentScale()  const { return _scale; }

private:
    /// Keeps the window at logicalWidth * scale. Drops scale to 1 if the monitor
    /// is too small to fit the current scale.
    void adjustScale();
    void generateFont();

    Config          _cfg;
    int             _scale;
    RenderTexture2D _renderTexture{};
    Font            _font{};
    bool            _shouldClose{false};
};

} // namespace gui
