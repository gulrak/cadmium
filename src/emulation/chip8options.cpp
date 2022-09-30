//---------------------------------------------------------------------------------------
// src/emulation/chip8options.cpp
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

#include <emulation/chip8options.hpp>

namespace emu {

Chip8Variant Chip8EmulatorOptions::presetAsVariant() const
{
    switch(behaviorBase) {
        case eCHIP8: return Chip8Variant::CHIP_8;
        case eCHIP10: return Chip8Variant::CHIP_10;
        case eCHIP48: return Chip8Variant::CHIP_48;
        case eSCHIP10: return Chip8Variant::SCHIP_1_0;
        case eSCHIP11: return Chip8Variant::SCHIP_1_1;
        case eXOCHIP: return Chip8Variant::XO_CHIP;
        case eCHICUEYI: return Chip8Variant::XO_CHIP;
        default: return Chip8Variant::CHIP_8;
    }
}

std::string Chip8EmulatorOptions::nameOfPreset(SupportedPreset preset)
{
    switch(preset) {
        case eCHIP8: return "CHIP-8";
        case eCHIP10: return "CHIP-10";
        case eCHIP48: return "CHIP-48";
        case eSCHIP10: return "SUPER-CHIP 1.0";
        case eSCHIP11: return "SUPER-CHIP 1.1";
        case eXOCHIP: return "XO-CHIP";
        case eCHICUEYI: return "CHICUEYI";
        default: return "unknown";
    }
}

Chip8EmulatorOptions Chip8EmulatorOptions::optionsOfPreset(SupportedPreset preset)
{
    switch(preset) {
        case eCHIP10: return { .behaviorBase = preset, .startAddress = 0x200, .optJustShiftVx = false, .optDontResetVf = false, .optLoadStoreIncIByX = false, .optLoadStoreDontIncI = false, .optWrapSprites = false, .optInstantDxyn = false, .optJump0Bxnn = false, .optAllowHires = true, .optOnlyHires = true, .optAllowColors = false, .optHas16BitAddr = false, .optXOChipSound = false, .optChicueyiSound = false, .instructionsPerFrame = 9};
        case eCHIP48: return { .behaviorBase = preset, .startAddress = 0x200, .optJustShiftVx = true, .optDontResetVf = true, .optLoadStoreIncIByX = true, .optLoadStoreDontIncI = false, .optWrapSprites = false, .optInstantDxyn = true, .optJump0Bxnn = true, .optAllowHires = false, .optOnlyHires = false, .optAllowColors = false, .optHas16BitAddr = false, .optXOChipSound = false, .optChicueyiSound = false, .instructionsPerFrame = 15};
        case eSCHIP10: return { .behaviorBase = preset, .startAddress = 0x200, .optJustShiftVx = true, .optDontResetVf = true, .optLoadStoreIncIByX = true, .optLoadStoreDontIncI = false, .optWrapSprites = false, .optInstantDxyn = true, .optJump0Bxnn = true, .optAllowHires = true, .optOnlyHires = false, .optAllowColors = false, .optHas16BitAddr = false, .optXOChipSound = false, .optChicueyiSound = false, .instructionsPerFrame = 15};
        case eSCHIP11: return { .behaviorBase = preset, .startAddress = 0x200, .optJustShiftVx = true, .optDontResetVf = true, .optLoadStoreIncIByX = false, .optLoadStoreDontIncI = true, .optWrapSprites = false, .optInstantDxyn = true, .optJump0Bxnn = true, .optAllowHires = true, .optOnlyHires = false, .optAllowColors = false, .optHas16BitAddr = false, .optXOChipSound = false, .optChicueyiSound = false, .instructionsPerFrame = 30};
        case eXOCHIP: return { .behaviorBase = preset, .startAddress = 0x200, .optJustShiftVx = false, .optDontResetVf = true, .optLoadStoreIncIByX = false, .optLoadStoreDontIncI = false, .optWrapSprites = true, .optInstantDxyn = true, .optJump0Bxnn = false, .optAllowHires = true, .optOnlyHires = false, .optAllowColors = true, .optHas16BitAddr = true, .optXOChipSound = true, .optChicueyiSound = false, .instructionsPerFrame = 500};
        case eCHICUEYI: return { .behaviorBase = preset, .startAddress = 0x200, .optJustShiftVx = false, .optDontResetVf = true, .optLoadStoreIncIByX = false, .optLoadStoreDontIncI = false, .optWrapSprites = false, .optInstantDxyn = true, .optJump0Bxnn = false, .optAllowHires = true, .optOnlyHires = false, .optAllowColors = true, .optHas16BitAddr = true, .optXOChipSound = false, .optChicueyiSound = true, .instructionsPerFrame = 500};
        case eCHIP8:
        default: return { .behaviorBase = preset, .startAddress = 0x200, .optJustShiftVx = false, .optDontResetVf = false, .optLoadStoreIncIByX = false, .optLoadStoreDontIncI = false, .optWrapSprites = false, .optInstantDxyn = false, .optJump0Bxnn = false, .optAllowHires = false, .optOnlyHires = false, .optAllowColors = false, .optHas16BitAddr = false, .optXOChipSound = false, .optChicueyiSound = false, .instructionsPerFrame = 9};
    }
}

}
