#include <doctest/doctest.h>
#include <emulation/coreregistry.hpp>

#include <algorithm>
#include <set>
#include <string>
#include <vector>

namespace {

struct ExpectedProperty
{
    std::string propertyClass;
    std::string label;
    std::string id;
    std::string optionName;
};

const std::vector<ExpectedProperty> EXPECTED_PROPERTIES = {
    {"CHIP-8 GENERIC", "Behavior Base", "behaviorBase", "behavior-base"},
    {"CHIP-8 GENERIC", "Trace Log", "traceLog", "trace-log"},
    {"CHIP-8 GENERIC", "Trace File", "traceFile", "trace-file"},
    {"CHIP-8 GENERIC", "Instructions per frame", "instructionsPerFrame", "instructions-per-frame"},
    {"CHIP-8 GENERIC", "Frame rate", "frameRate", "frame-rate"},
    {"CHIP-8 GENERIC", "Memory", "memory", "memory"},
    {"CHIP-8 GENERIC", "Start Address", "startAddress", "start-address"},
    {"CHIP-8 GENERIC", "Clean RAM", "cleanRam", "clean-ram"},
    {"CHIP-8 GENERIC", "8xy6/8xyE just shift Vx", "justShiftVx", "just-shift-vx"},
    {"CHIP-8 GENERIC", "8xy1/8xy2/8xy3 don't reset VF", "dontResetVf", "dont-reset-vf"},
    {"CHIP-8 GENERIC", "Fx55/Fx65 increment I by x + 1", "loadStoreIncIByXPlus1", "load-store-inc-i-by-x-plus-1"},
    {"CHIP-8 GENERIC", "Fx55/Fx65 increment I by x", "loadStoreIncIByx", "load-store-inc-i-by-x"},
    {"CHIP-8 GENERIC", "Wrap sprite pixels", "wrapSprites", "wrap-sprites"},
    {"CHIP-8 GENERIC", "Dxyn doesn't wait for vsync", "instantDxyn", "instant-dxyn"},
    {"CHIP-8 GENERIC", "Extended CHIP-8 wait emulation", "extendedVblank", "extended-vblank"},
    {"CHIP-8 GENERIC", "Lores Dxy0 draws 8 pixel width", "loresDxy0Is8x16", "lores-dxy-0-is-8-x-16"},
    {"CHIP-8 GENERIC", "Lores Dxy0 draws 16 pixel width", "loresDxy0Is16x16", "lores-dxy-0-is-16-x-16"},
    {"CHIP-8 GENERIC", "Dxyn uses SCHIP1.1 collision", "schip11Collision", "schip-11-collision"},
    {"CHIP-8 GENERIC", "HP-48SX display emulation", "hp48Display", "hp-48-display"},
    {"CHIP-8 GENERIC", "HP SuperChip lores drawing", "schipLoresDrawing", "schip-lores-drawing"},
    {"CHIP-8 GENERIC", "HP SuperChip 1.0 'preview' Fx29", "schipPreviewFx29", "schip-preview-fx-29"},
    {"CHIP-8 GENERIC", "Half pixel scrolling", "halfPixelScroll", "half-pixel-scroll"},
    {"CHIP-8 GENERIC", "Mode change clear", "modeChangeClear", "mode-change-clear"},
    {"CHIP-8 GENERIC", "Bxnn/jump0 uses Vx", "jump0Bxnn", "jump-0-bxnn"},
    {"CHIP-8 GENERIC", "128x64 hires support", "allowHires", "allow-hires"},
    {"CHIP-8 GENERIC", "Only 128x64 mode", "onlyHires", "only-hires"},
    {"CHIP-8 GENERIC", "Multicolor support", "allowColors", "allow-colors"},
    {"CHIP-8 GENERIC", "Cyclic stack", "cyclicStack", "cyclic-stack"},
    {"CHIP-8 GENERIC", "Has 16 bit addresses", "has16bitAddr", "has-16-bit-addr"},
    {"CHIP-8 GENERIC", "XO-CHIP sound engine", "xoChipSound", "xo-chip-sound"},
    {"CHIP-8 GENERIC", "CubeChip contextual tone", "cubechipTone", "cubechip-tone"},
    {"CHIP-8 GENERIC", "PAL video format", "palVideo", "pal-video"},
    {"CHIP-8 GENERIC", "Screen rotation", "screenRotation", "screen-rotation"},
    {"CHIP-8 GENERIC", "Touch input mode", "touchMode", "touch-mode"},
    {"CHIP-8 GENERIC", "Font 5px", "font5px", "font-5-px"},
    {"CHIP-8 GENERIC", "Font 10px", "font10px", "font-10-px"},
    {"CHIP-8 STRICT", "Trace Log", "traceLog", "trace-log"},
    {"CHIP-8 STRICT", "Clock Rate", "clockRate", "clock-rate"},
    {"CHIP-8 STRICT", "Memory", "memory", "memory"},
    {"CHIP-8 STRICT", "Clean RAM", "cleanRam", "clean-ram"},
    {"RS-M8K1", "VIP Mode", "vipMode", "vip-mode"},
    {"RS-M8K1", "Trace Log", "traceLog", "trace-log"},
    {"COSMAC-VIP", "Trace Log", "traceLog", "trace-log"},
    {"COSMAC-VIP", "CPU", "cpu", "cpu"},
    {"COSMAC-VIP", "Clock Rate", "clockRate", "clock-rate"},
    {"COSMAC-VIP", "Memory", "memory", "memory"},
    {"COSMAC-VIP", "Clean RAM", "cleanRam", "clean-ram"},
    {"COSMAC-VIP", "Video", "video", "video"},
    {"COSMAC-VIP", "Trace Video", "traceVideo", "trace-video"},
    {"COSMAC-VIP", "Audio", "audio", "audio"},
    {"COSMAC-VIP", "Keyboard", "keyboard", "keyboard"},
    {"COSMAC-VIP", "ROM Name", "romName", "rom-name"},
    {"COSMAC-VIP", "Interpreter", "interpreter", "interpreter"},
    {"COSMAC-VIP", "Start Address", "startAddress", "start-address"},
    {"HP-48SX", "Interpreter", "interpreter", "interpreter"},
    {"HP-48SX", "ROM", "rom", "rom"},
    {"HP-48SX", "Fixed contrast", "fixedContrast", "fixed-contrast"},
    {"HP-48SX", "Contrast", "contrast", "contrast"},
    {"HP-48SX", "Interpreter Source", "interpreterSource", "interpreter-source"},
    {"HP-48SX", "Fetch Symbol", "fetchSymbol", "fetch-symbol"},
    {"HP-48SX", "Symbol Address Bias", "symbolAddressBias", "symbol-address-bias"},
    {"HP-48SX", "Timer Symbol", "timerSymbol", "timer-symbol"},
    {"HP-48SX", "Sound Symbol", "soundSymbol", "sound-symbol"},
    {"HP-48SX", "Stack Pointer Symbol", "stackPointerSymbol", "stack-pointer-symbol"},
    {"HP-48SX", "Stack Symbol", "stackSymbol", "stack-symbol"},
    {"HP-48SX", "Key Table Symbol", "keyTableSymbol", "key-table-symbol"},
    {"DREAM6800", "Trace Log", "traceLog", "trace-log"},
    {"DREAM6800", "CPU", "cpu", "cpu"},
    {"DREAM6800", "Clock Rate", "clockRate", "clock-rate"},
    {"DREAM6800", "Memory", "memory", "memory"},
    {"DREAM6800", "Clean RAM", "cleanRam", "clean-ram"},
    {"DREAM6800", "Video", "video", "video"},
    {"DREAM6800", "ROM Name", "romName", "rom-name"},
    {"DREAM6800", "Start Address", "startAddress", "start-address"},
    {"ETI660", "Trace Log", "traceLog", "trace-log"},
    {"ETI660", "CPU", "cpu", "cpu"},
    {"ETI660", "Clock Rate", "clockRate", "clock-rate"},
    {"ETI660", "Memory", "memory", "memory"},
    {"ETI660", "Clean RAM", "cleanRam", "clean-ram"},
    {"ETI660", "Video", "video", "video"},
    {"ETI660", "Audio", "audio", "audio"},
    {"ETI660", "Keyboard", "keyboard", "keyboard"},
    {"ETI660", "ROM Name", "romName", "rom-name"},
    {"ETI660", "Start Address", "startAddress", "start-address"},
};

} // namespace

TEST_CASE("Property ids and CLI options stay compatible")
{
    // Only the cores of this build are checked (e.g. HP-48SX is optional), a core without expectations fails.
    emu::CoreRegistry registry;
    for (const auto& [className, info] : registry) {
        auto proto = info->propertiesPrototype();
        std::vector<ExpectedProperty> actual;
        for (size_t i = 0; i < proto.numProperties(); ++i) {
            const auto& prop = proto[i];
            if (!prop.getId().empty())
                actual.push_back({std::string(className), prop.getLabel(), prop.getId(), prop.getOptionName()});
        }
        std::vector<ExpectedProperty> expected;
        std::copy_if(EXPECTED_PROPERTIES.begin(), EXPECTED_PROPERTIES.end(), std::back_inserter(expected), [&](const auto& e) { return e.propertyClass == className; });
        INFO(std::string(className));
        REQUIRE(actual.size() == expected.size());
        for (size_t i = 0; i < actual.size(); ++i) {
            INFO(expected[i].label);
            CHECK(actual[i].label == expected[i].label);
            CHECK(actual[i].id == expected[i].id);
            CHECK(actual[i].optionName == expected[i].optionName);
        }
    }
}

TEST_CASE("Property ids are unique and usable as JSON keys")
{
    emu::CoreRegistry registry;
    for (const auto& [className, info] : registry) {
        auto proto = info->propertiesPrototype();
        std::set<std::string> ids;
        for (size_t i = 0; i < proto.numProperties(); ++i) {
            const auto& id = proto[i].getId();
            if (id.empty())
                continue;  // separator
            const auto context = std::string(className) + ": " + id;
            INFO(context);
            CHECK(ids.insert(id).second);
            CHECK(std::all_of(id.begin(), id.end(), [](char c) { return std::isalnum(static_cast<unsigned char>(c)); }));
        }
    }
}
