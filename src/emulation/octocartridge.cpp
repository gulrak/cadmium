//
// Created by schuemann on 30.03.23.
//

#include "utility.hpp"
#include <emulation/chip8options.hpp>

#include "octocartridge.hpp"

extern "C" {
#define OCTO_PALETTE_SIZE     6
typedef struct {
    // core settings
    int tickrate;                  // {7,15,20,30,100,200,500,1000,10000,...}
    int max_rom;                   // {3232, 3583, 3584, 65024}
    int rotation;                  // {0, 90, 180, 270}
    int font;                      // OCTO_FONT_...
    int touch_mode;                // OCTO_TOUCH_...
    int colors[OCTO_PALETTE_SIZE]; // OCTO_COLOR_... (ARGB)

    // quirks flags
    char q_shift;
    char q_loadstore;
    char q_jump0;
    char q_logic;
    char q_clip;
    char q_vblank;
} octo_options;

void octo_default_options(octo_options*options){
    memset(options,0,sizeof(octo_options));
    options->tickrate=20;
    options->max_rom=3584;
    uint32_t dc[]={0xFF996600,0xFFFFCC00,0xFFFF6600,0xFF662200,0xFF000000,0xFFFFAA00};
    memcpy(options->colors,dc,sizeof(dc));
}

#include <c-octo/src/octo_cartridge.h>
}

namespace emu {

static Chip8EmulatorOptions optionsFromOctoOptions(const octo_options& octo)
{
    Chip8EmulatorOptions result;
    if(octo.max_rom > 3584 || !octo.q_clip) {
        result = Chip8EmulatorOptions::optionsOfPreset(Chip8EmulatorOptions::eXOCHIP);
    }
    else if(octo.q_vblank || !octo.q_loadstore || !octo.q_shift) {
        result = Chip8EmulatorOptions::optionsOfPreset(Chip8EmulatorOptions::eCHIP8);
    }
    else {
        result = Chip8EmulatorOptions::optionsOfPreset(Chip8EmulatorOptions::eSCHPC);
    }
    result.optJustShiftVx = octo.q_shift;
    result.optLoadStoreDontIncI = octo.q_loadstore;
    result.optLoadStoreIncIByX = false;
    result.optJump0Bxnn = octo.q_jump0;
    result.optDontResetVf = !octo.q_logic;
    result.optWrapSprites = !octo.q_clip;
    result.optInstantDxyn = !octo.q_vblank;
    result.instructionsPerFrame = octo.tickrate;
}

OctoCartridge emu::OctoCartridge::load(std::string filename)
{
    auto data = loadFile(filename);
    octo_options options;
    octo_default_options(&options);
    octo_str source;
    source.root = (char*)data.data();
    source.pos = source.size = data.size();
    octo_gif* g = octo_gif_decode(&source);
    octo_str json;
    octo_str_init(&json);
    int offset = 0, size = 0;
    for (int z = 0; z < 4; z++)
        size = (size << 8) | (0xFF & octo_cart_byte(g, &offset));
    for (int z = 0; z < size; z++)
        octo_str_append(&json, octo_cart_byte(g, &offset));
    octo_str_append(&json, '\0');
    char* program = octo_cart_parse_json(&json, &options);
    octo_str_destroy(&json);
    octo_gif_destroy(g);
    
    return {};
}

}