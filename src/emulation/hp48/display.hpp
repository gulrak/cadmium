//
// Created by Steffen Schümann on 18.02.26.
//
#pragma once
#include <cstdint>
#include <span>

#include <ghc/span2d.hpp>

#include "stack.hpp"

namespace hp48 {

class Display
{
public:
    enum Mode { Stack, FrameBuffer };
    explicit Display(std::span<uint8_t> frameBuffer, const ObjectStack& stack, const std::vector<std::string>& menuRef);

    void setError(std::pair<std::string, std::string> error);
    void clearError();
    void render(ghc::span2d<uint32_t> pixelBuffer, Mode mode, int scale = 4);

private:
    static uint32_t lerpU32Bytes(uint32_t a, uint32_t b, uint8_t mix);
    std::span<uint8_t> _frameBuffer;
    const ObjectStack& _stack;
    const std::vector<std::string>& _menuRef;
    std::pair<std::string, std::string> _error;
    uint32_t _bgCol;
    uint32_t _fgOffCol;
    uint32_t _fgOnCol;
    uint32_t _shadowCol;
    bool _leftShift = false;
    bool _rightShift = false;
    bool _alpha = false;
    bool _alarm = false;
    bool _busy = true;
    bool _transfer = false;
};

}  // namespace hp48
