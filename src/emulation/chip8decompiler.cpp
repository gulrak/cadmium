//
// Created by Steffen Schümann on 29.07.22.
//

#include "chip8decompiler.hpp"

namespace emu {

std::pair<std::string,std::string> Chip8Decompiler::chipVariantName(Chip8Variant cv)
{
    switch(cv)
    {
        case C8V::CHIP_8: return {"chip-8", "CHIP-8"};
        case C8V::CHIP_8_1_2: return {"chio-8.5", "CHIP-8 1/2"};
        case C8V::CHIP_8_I: return {"chip-8i", "CHIP-8I"};
        case C8V::CHIP_8_II: return {"chip-8ii", "CHIP-8 II aka. Keyboard Kontrol"};
        case C8V::CHIP_8_III: return {"chip-8iii", "CHIP-8III"};
        case C8V::CHIP_8_TPD: return {"chip-8-tpd", "Two-page display for CHIP-8"};
        case C8V::CHIP_8C: return {"chip-8c", "CHIP-8C"};
        case C8V::CHIP_10: return {"chip-10", "CHIP-10"};
        case C8V::CHIP_8_SRV: return {"chip-8-srv", "CHIP-8 modification for saving and restoring variables"};
        case C8V::CHIP_8_SRV_I: return {"chip-8-srv-i", "Improved CHIP-8 modification for saving and restoring variables"};
        case C8V::CHIP_8_RB: return {"chip-8-rb", "CHIP-8 modification with relative branching"};
        case C8V::CHIP_8_ARB: return {"chip-8-arb", "Another CHIP-8 modification with relative branching"};
        case C8V::CHIP_8_FSD: return {"chip-8-fsb", "CHIP-8 modification with fast, single-dot DXYN"};
        case C8V::CHIP_8_IOPD: return {"chip-8-iopd", "CHIP-8 with I/O port driver routine"};
        case C8V::CHIP_8_8BMD: return {"chip-8-8bmd", "CHIP-8 8-bit multiply and divide"};
        case C8V::HI_RES_CHIP_8: return {"hires-chip-8","HI-RES CHIP-8 (four-page display)"};
        case C8V::HI_RES_CHIP_8_IO: return {"hires-chip-8-io", "HI-RES CHIP-8 with I/O"};
        case C8V::HI_RES_CHIP_8_PS: return {"hires-chip-8-ps", "HI-RES CHIP-8 with page switching"};
        case C8V::CHIP_8E: return {"chip-8e", "CHIP-8E"};
        case C8V::CHIP_8_IBNNN: return {"chip-8-ibnnn", "CHIP-8 with improved BNNN"};
        case C8V::CHIP_8_SCROLL: return {"chip-8-scroll", "CHIP-8 scrolling routine"};
        case C8V::CHIP_8X: return {"chip-8x", "CHIP-8X"};
        case C8V::CHIP_8X_TPD: return {"chip-8x-tdp", "Two-page display for CHIP-8X"};
        case C8V::HI_RES_CHIP_8X: return {"hires-chip-8x", "Hi-res CHIP-8X"};
        case C8V::CHIP_8Y: return {"chip-8y", "CHIP-8Y"};
        case C8V::CHIP_8_CtS: return {"chip-8-cts", "CHIP-8 “Copy to Screen"};
        case C8V::CHIP_BETA: return {"chip-beta", "CHIP-BETA"};
        case C8V::CHIP_8M: return {"chip-8m", "CHIP-8M"};
        case C8V::MULTIPLE_NIM: return {"multi-nim", "Multiple Nim interpreter"};
        case C8V::DOUBLE_ARRAY_MOD: return {"double-array-mod", "Double Array Modification"};
        case C8V::CHIP_8_D6800: return {"chip-8-d6800", "CHIP-8 for DREAM 6800 (CHIPOS)"};
        case C8V::CHIP_8_D6800_LOP: return {"chip-8-d6800-lop", "CHIP-8 with logical operators for DREAM 6800 (CHIPOSLO)"};
        case C8V::CHIP_8_D6800_JOY: return {"chip-8-d6800-joy", "CHIP-8 for DREAM 6800 with joystick"};
        case C8V::CHIPOS_2K_D6800: return {"chipos-2k-d6800", "2K CHIPOS for DREAM 6800"};
        case C8V::CHIP_8_ETI660: return {"chip-8-eti660", "CHIP-8 for ETI-660"};
        case C8V::CHIP_8_ETI660_COL: return {"chip-8-eti660-col", "CHIP-8 with color support for ETI-660"};
        case C8V::CHIP_8_ETI660_HR: return {"chip-8-eti660-hr", "CHIP-8 for ETI-660 with high resolution"};
        case C8V::CHIP_8_COSMAC_ELF: return {"chip-8-cosmac-elf", "CHIP-8 for COSMAC ELF"};
        case C8V::CHIP_8_ACE_VDU: return {"chip-8-ace-vdu", "CHIP-VDU / CHIP-8 for the ACE VDU"};
        case C8V::CHIP_8_AE: return {"chip-8-ae", "CHIP-8 AE (ACE Extended)"};
        case C8V::CHIP_8_DC_V2: return {"chip-8-dc-v2", "Dreamcards Extended CHIP-8 V2.0"};
        case C8V::CHIP_8_AMIGA: return {"chip-8-amiga", "Amiga CHIP-8 interpreter"};
        case C8V::CHIP_48: return {"chip-48", "CHIP-48"};
        case C8V::SCHIP_1_0: return {"schip-1.0", "SUPER-CHIP 1.0"};
        case C8V::SCHIP_1_1: return {"schip-1.1", "SUPER-CHIP 1.1"};
        case C8V::GCHIP: return {"gchip", "GCHIP"};
        case C8V::SCHIPC_GCHIPC: return {"schpc-gchpc", "SCHIP Compatibility (SCHPC) and GCHIP Compatibility (GCHPC)"};
        case C8V::VIP2K_CHIP_8: return {"vip2k-chip-8", "VIP2K CHIP-8"};
        case C8V::SCHIP_1_1_SCRUP: return {"schip-1.1-scrup", "SUPER-CHIP with scroll up"};
        case C8V::CHIP8RUN: return {"chip8run", "chip8run"};
        case C8V::MEGA_CHIP: return {"megachip", "Mega-Chip"};
        case C8V::XO_CHIP: return {"xo-chip", "XO-CHIP"};
        case C8V::OCTO: return {"octo", "Octo"};
        case C8V::CHIP_8_CL_COL: return {"chip-8-cl-col", "CHIP-8 Classic / Color"};
        default: return {"", ""};
    }
}

}