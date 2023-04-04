//---------------------------------------------------------------------------------------
// src/emulation/chip8options.hpp
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
#pragma once

#include <chiplet/chip8variants.hpp>
#include <nlohmann/json_fwd.hpp>

#include <cstdint>
#include <string>

namespace emu {

struct Chip8EmulatorOptions {
    enum SupportedPreset { eCHIP8, eCHIP10, eCHIP48, eSCHIP10, eSCHIP11, eSCHPC, eMEGACHIP, eXOCHIP, eCHIP8VIP, eCHIP8VIP_TPD, eCHIP8DREAM, eC8D68CHIPOSLO, eCHICUEYI, eNUM_PRESETS };
    SupportedPreset behaviorBase{eCHIP8};
    uint16_t startAddress{0x200};
    bool optJustShiftVx{false};
    bool optDontResetVf{false};
    bool optLoadStoreIncIByX{false};
    bool optLoadStoreDontIncI{false};
    bool optWrapSprites{false};
    bool optInstantDxyn{false};
    bool optLoresDxy0Is8x16{false};
    bool optLoresDxy0Is16x16{false};
    bool optSC11Collision{false};
    bool optJump0Bxnn{false};
    bool optAllowHires{false};
    bool optOnlyHires{false};
    bool optAllowColors{false};
    bool optHas16BitAddr{false};
    bool optXOChipSound{false};
    bool optChicueyiSound{false};
    bool optTraceLog{false};
    int instructionsPerFrame{9};
    std::shared_ptr<nlohmann::ordered_json> advanced;
    Chip8Variant presetAsVariant() const;
    //static SupportedPreset variantAsPreset(Chip8Variant variant);
    static std::string nameOfPreset(SupportedPreset preset);
    static const char* shortNameOfPreset(SupportedPreset preset);
    static SupportedPreset presetForName(const std::string& name);
    static Chip8EmulatorOptions optionsOfPreset(SupportedPreset preset);
};

void to_json(nlohmann::json& j, const Chip8EmulatorOptions& o);
void from_json(const nlohmann::json& j, Chip8EmulatorOptions& o);

}
