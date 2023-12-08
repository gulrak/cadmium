//---------------------------------------------------------------------------------------
// src/librarian.cpp
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
#include <emulation/chip8cores.hpp>
#include <chiplet/chip8decompiler.hpp>
#include <chiplet/utility.hpp>
#include <librarian.hpp>
#include <chip8emuhostex.hpp>

#include <nlohmann/json.hpp>
#include <raylib.h>

#include <algorithm>
#include <chrono>

static std::unique_ptr<emu::IChip8Emulator> minion;

template<typename TP>
inline std::chrono::system_clock::time_point convertClock(TP tp)
{
    using namespace std::chrono;
    return time_point_cast<system_clock::duration>(tp - TP::clock::now() + system_clock::now());
}

struct KnownRomInfo {
    const char* sha1;
    emu::chip8::Variant variant;
    const char* name;
    const char* options;
    const char* url;
};

static KnownRomInfo g_knownRoms[] = {
    //{"18418563acd5c64ff410fdede56ffd80c139888a", {emu::Chip8EmulatorOptions::eCHIP8X}},
    //{"a5fe40576733f4324b85c5cff10d831f9d245449", {emu::Chip8EmulatorOptions::eCHIP8X}},
    //{"a9365fc13d22118cdedf3979e2199831b0b605a9", {emu::Chip8EmulatorOptions::eCHIP8X}},
    //{"f452b63f9ba21d8716b7630f4f327c2ebdff0755", {emu::Chip8EmulatorOptions::eCHIP8X}},
    //{"fede2cb2fa570b361c7915e24499a263f3c36a12", {emu::Chip8EmulatorOptions::eCHIP8X}},
    {"004fa49c91fbd387484bda62f843e8c5bd2c53d2", emu::chip8::Variant::CHIP_8, "Lainchain (Ashton Harding, 2018)", nullptr, nullptr},
    {"0068ff5421f5d62a1ae1c814c68716ddb65cec5b", emu::chip8::Variant::XO_CHIP, "Master B8 (Andrew James, 2021)", nullptr, nullptr},
    {"0085dd8fce4f7ac2e39ba73cf67cc043f9ba4812", emu::chip8::Variant::CHIP_8, "Stars (Sergey Naydenov, 2010)", R"({"optLoadStoreDontIncI": true})", nullptr},
    {"016345d75eef34448840845a9590d41e6bfdf46a", emu::chip8::Variant::CHIP_8, "Clock Program (Bill Fisher, 1981)", nullptr, nullptr},
    {"018442698067c95d67e27a94e6642c11f049f108", emu::chip8::Variant::CHIP_8, "1D Cellular Automata (SharpenedSpoon, 2014-10-26)", R"({"instructionsPerFrame": 1000, "advanced": {"palette": ["#274a17", "#00f80a"], "buzzColor": "#FFAA00", "quietColor": "#142a12"}})", nullptr},
    //{"018e6da9937173b1ac44d4261e848af485dcd305", {emu::Chip8EmulatorOptions::eXOCHIP}},
    {"018e6da9937173b1ac44d4261e848af485dcd305", emu::chip8::Variant::XO_CHIP, "Truck Simul8Or (Bjorn Kempen, 2015)", nullptr, nullptr},
    //{"01ffe488efbe14ca63de1c23053806533e329f3f", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"01ffe488efbe14ca63de1c23053806533e329f3f", emu::chip8::Variant::SCHIPC, "H (Paul Raines, 1995)", nullptr, nullptr},
    //{"0268a789a6c1e281b6fc472c41bbdd00a40e2850", {emu::Chip8EmulatorOptions::eXOCHIP}},
    {"0268a789a6c1e281b6fc472c41bbdd00a40e2850", emu::chip8::Variant::XO_CHIP, "Turtle (Ian J Sikes, 2016)", nullptr, nullptr},
    {"02972781f36cd9ccf36162789ec9687fa3f1a733", emu::chip8::Variant::CHIP_8, "Drop Your Program Here (An Phu Dupont, 2016)", nullptr, nullptr},
    //{"0317e94014ebc3a9a1a2a33c46bc766a9cf44cb0", {emu::Chip8EmulatorOptions::eXOCHIP}},
    {"0317e94014ebc3a9a1a2a33c46bc766a9cf44cb0", emu::chip8::Variant::XO_CHIP, "Chip8-Multiply (John Deeny, 2016)", nullptr, nullptr},
    {"032408f1f1d8e6058ecf0f23f421783c87701b39", emu::chip8::Variant::CHIP_8, "Trip-8 Demo (Revival Studios, Martijn Wenting, 2008)", nullptr, nullptr},
    //{"044021b046cf207c0b555ea884d61a726f7a3c22", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"044021b046cf207c0b555ea884d61a726f7a3c22", emu::chip8::Variant::SCHIPC, "Ded-Lok (parityb1t, 2015)", nullptr, nullptr},
    {"048659b97e0cf9506eba85ef7baaf21ada22c6f2", emu::chip8::Variant::CHIP_8, "Astro Dodge (Revival Studios)", nullptr, nullptr},
    {"04e18ff4ae42e3056c502e0c99d4740ecea65966", emu::chip8::Variant::XO_CHIP, "Nyan Cat 2 (Kouzerumatsu, 2022)", R"({"instructionsPerFrame": 10000})", nullptr},
    {"050f07a54371da79f924dd0227b89d07b4f2aed0", emu::chip8::Variant::CHIP_8, "Hidden (David Winter, 1996)", nullptr, nullptr},
    {"0572f188fc25ccda14b0c306c4156fe4b1d21ae1", emu::chip8::Variant::GENERIC_CHIP_8, "4-Flags (Timendus, 2023-04-12)", nullptr, "@GH/Timendus/chip8-test-suite/v4.0/bin/4-flags.ch8"},
    {"064492173cf4ccac3cce8fe307fc164b397013b9", emu::chip8::Variant::CHIP_8, "Division Test (Sergey Naydenov, 2010)", nullptr, nullptr},
    //{"0663449e1cc8d79ee38075fe86d6b9439a7e43d7", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"0663449e1cc8d79ee38075fe86d6b9439a7e43d7", emu::chip8::Variant::SCHIPC, "Super Sierp Chaos (Marco Varesio, 2015)", nullptr, nullptr},
    {"066e7a84efde433e4d937d8aa41518666955086c", emu::chip8::Variant::CHIP_8_COSMAC_VIP, "Astro Dodge Hires (Revival-Studios, 2008)", nullptr, nullptr},  // Reveal Studios HIRES-CHIP-8 programs (actually self patching "Two page display CHIP-8", VIPER vol. 1, issue 3)
    {"06a6692c92eb8077329b6d4e59d55479d60574a8", emu::chip8::Variant::SCHIPC, "Snake (TimoTriisa, 2014-10-11)", R"({"instructionsPerFrame": 15, "advanced": {"col1": "#84a174", "col0": "#30283e", "buzzColor": "#FFAA00", "quietColor": "#000000"}})", nullptr},
    {"082c71b67e36e033c2e615ad89ba4ed5d55a56d0", emu::chip8::Variant::CHIP_8, "Delay Timer Test (Matthew Mikolay, 2010)", nullptr, nullptr},
    //{"085394b959f03a0e525b404d8a68a36e56d6446a", {emu::Chip8EmulatorOptions::eXOCHIP}},
    {"085394b959f03a0e525b404d8a68a36e56d6446a", emu::chip8::Variant::XO_CHIP, "Static Organ (TomR, 2015)", nullptr, nullptr},
    //{"0893dd3b5fafa013f07acc9aa98876f84f328d54", {emu::Chip8EmulatorOptions::eXOCHIP}},
    {"0893dd3b5fafa013f07acc9aa98876f84f328d54", emu::chip8::Variant::XO_CHIP, "An Evening To Die For (John Earnest, 2019)", nullptr, nullptr},
    {"0920bfcaf974a10621af7ef0e48929c86dd0df2e", emu::chip8::Variant::CHIP_8, "Snake (Timo Triisa, 2014)", nullptr, nullptr},
    {"09ce01c54ddddda42ca5cd171f1ffcfd47355d12", emu::chip8::Variant::CHIP_8, "Wall (David Winter, 19xx)", nullptr, nullptr},
    {"09d8e40f143f808ff379f04a473f58cbba5f3838", emu::chip8::Variant::CHIP_8, "Ded-Lok (ParityB1t, 2016)", nullptr, nullptr},
    {"09f47bea104b86169b9aeb3bdee6e26315ed0a53", emu::chip8::Variant::CHIP_8, "Zero Demo (ZeroShadowZ, 2007)", nullptr, nullptr},
    //{"0b5522b1ce775879092be840b0e840cb1dea74fd", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"0b5522b1ce775879092be840b0e840cb1dea74fd", emu::chip8::Variant::SCHIPC, "Link Demo (John Earnest, 2014)", nullptr, nullptr},
    {"0cd895dc3d489d0e40656218900a04310e95f560", emu::chip8::Variant::CHIP_8, "Fuse (JohnEarnest, 2016-10-27)", R"({"instructionsPerFrame": 15, "optWrapSprites": true, "advanced": {"col1": "#306230", "col2": "#FF6600", "col3": "#662200", "col0": "#8BAC0F", "buzzColor": "#333333", "quietColor": "#000000", "screenRotation": 0}})", nullptr},
    {"0ce13060abe94e2b73404fc78186b786121ddeeb", emu::chip8::Variant::CHIP_8, "Dot-Dash (Tom Chen, 1978)", nullptr, nullptr},
    {"0d0cc129dad3c45ba672f85fec71a668232212cc", emu::chip8::Variant::CHIP_8, "Missile Command (David Winter, 19xx)", nullptr, nullptr},
    //{"0dc782f0607d34b8355c150e81bc280de7472d94", {emu::Chip8EmulatorOptions::eXOCHIP}},
    {"0dc782f0607d34b8355c150e81bc280de7472d94", emu::chip8::Variant::XO_CHIP, "Dig Site 8 (taqueso, 2018)", nullptr, nullptr},
    {"0df2789f661358d8f7370e6cf93490c5bcd44b01", emu::chip8::Variant::GENERIC_CHIP_8, "1-chip8-logo (Timendus, 2023-04-12)", nullptr,"@GH/Timendus/chip8-test-suite/v4.0/bin/1-chip8-logo.ch8"},
    {"0ebc4b92c6059d6193565644fb00108161d03d23", emu::chip8::Variant::CHIP_8, "Keypad Test (Hap, 2006)", R"({"optJustShiftVx": true})", nullptr},
    {"0f479a10fec51d159866e5760069cd18bdfd293f", emu::chip8::Variant::CHIP_8, "Bad Kaiju Ju (MattBooth, 2015-08-24)", R"({"instructionsPerFrame": 7, "advanced": {"col1": "#330033", "col0": "#AAAAFF"}})", nullptr},
    {"100dea0037219d82a090e35eb93526ba4413ffe4", emu::chip8::Variant::CHIP_8, "Shooter (Group 8 Team, 2019)", nullptr, nullptr},
    {"107366630b4e0449add7ab00f93cce65f38f9713", emu::chip8::Variant::CHIP_8, "Space Defense! (Jim South-2014)", nullptr, nullptr},
    {"10fe2d629a3cebdbfe23fb9310ca74a3574e5a67", emu::chip8::Variant::XO_CHIP, "Akahad_V1", nullptr, nullptr},                      // Akahad_v1.0.xo8
    {"11c68038d64a09be549a6c1e50724808914d8991", emu::chip8::Variant::CHIP_8, "Octojam 2 Title (JohnEarnest, 2015-09-21)", R"({"instructionsPerFrame": 7, "advanced": {"col1": "#FFAA00", "col2": "#FF6600", "col3": "#662200", "col0": "#AA4400", "buzzColor": "#FFAA00", "quietColor": "#000000"}})", nullptr},
    {"11d66c2ff456ca3aea5f384a5a11503a6c8f85ed", emu::chip8::Variant::CHIP_8, "Boot Super Chip8X (Ersanio, 2018)", nullptr, nullptr},
    //{"12572c9e957cace53076d1656ea1b12cd0f331af", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"12572c9e957cace53076d1656ea1b12cd0f331af", emu::chip8::Variant::SCHIPC, "Ultimate Tic-Tac-Toe (your name here, 2014-09-01)", R"({"instructionsPerFrame": 7, "advanced": {"col1": "#553300", "col0": "#FFFFFF", "buzzColor": "#FFAA00", "quietColor": "#FFFFFF"}})", nullptr},
    {"1261b79da4d25792c05eaed47a0285b48dd7b7f4", emu::chip8::Variant::CHIP_8, "Jackpot (Joyce Weisbecker, 1978)", nullptr, nullptr},
    {"1293db0ccccbe7dd3fc5a09a2abc5d7b175e18e0", emu::chip8::Variant::CHIP_8, "Puzzle", nullptr, nullptr},                  // Puzzle.ch8
    {"12d3bf6c28ebf07b49524f38d6436f814749d4c0", emu::chip8::Variant::SCHIP_1_1, nullptr, nullptr, nullptr},
    {"12e053d66be67836deff1c07af93fe1d33a8eec5", emu::chip8::Variant::XO_CHIP, "Jub8 Song 2 (your name here, 2016-08-31)", R"({"instructionsPerFrame": 1000, "advanced": {"col1": "#000000", "col2": "#FDFFD5", "col3": "#BA5A1A", "col0": "#353C41", "buzzColor": "#353C41", "quietColor": "#353C41", "screenRotation": 0}})", nullptr},
    {"12fccf60004f685c112fe3db3d3bcfba104cbcb1", emu::chip8::Variant::CHIP_8_COSMAC_VIP, "Video Display Drawing Game (Joseph Weisbecker)", nullptr, nullptr}, // CHIP-8 VIP (hybrid roms)
    {"1368d7eae124661aacaf3411819ca9c113c0c10c", emu::chip8::Variant::CHIP_8, "Down8 (tinaun, 2015-10-28)", R"({"instructionsPerFrame": 15, "advanced": {"col1": "#27130F", "col0": "#3DBDFA", "buzzColor": "#FF6600", "quietColor": "#000000"}})", nullptr},
    {"137cb8397456f53fcab216124458238bc18c0965", emu::chip8::Variant::CHIP_8, "Guess The Number (David Winter)", nullptr, nullptr},
    {"1539d55e2dda1dd2affa584d8e8e19a7d1f4a41e", emu::chip8::Variant::XO_CHIP, "Elm8Tal", nullptr, nullptr},                        // Elm8tal.xo8
    {"17238bcd1cb8e21142a1d7533f878c833ef19caa", emu::chip8::Variant::CHIP_8, "Cavern (Matthew Mikolay, 2014)", nullptr, nullptr},
    {"175bbb8b3b671c13ff6d9f5b80e31218956e7281", emu::chip8::Variant::SCHIPC, "Turm8 (Tobias V. Langhoff, 2020)", nullptr, nullptr},
    //{"17d775833f073be77f2834751523996e0a398edd", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"17d775833f073be77f2834751523996e0a398edd", emu::chip8::Variant::SCHIPC, "By The Moon (SystemLogoff, 2019)", nullptr, nullptr},
    {"1830eb401ba8789a477dfcf294873a5479ebcfe8", emu::chip8::Variant::CHIP_8, "Pong 2 (David Winter, 1997)", nullptr, nullptr},
    {"18aef6d2d3b560681038d0dda2273d780dc1daa5", emu::chip8::Variant::CHIP_8, "Octojam 6 Title (JohnEarnest, 2019-09-07)", R"({"instructionsPerFrame": 7, "optWrapSprites": true, "advanced": {"col1": "#330033", "col2": "#00FFFF", "col3": "#FFFFFF", "col0": "#AAAAFF", "buzzColor": "#990099", "quietColor": "#330033", "screenRotation": 0}})", nullptr},
    {"18b9d15f4c159e1f0ed58c2d8ec1d89325d3a3b6", emu::chip8::Variant::CHIP_8, "Tank Battle", nullptr, nullptr},                     // Tank.ch8
    {"18fdb6728fc74be08859b27195ec289ea9d132d4", emu::chip8::Variant::MEGA_CHIP, "Mega Test Demo (Ready4Next, 2014)", R"({"optWrapSprites": true})"},
    {"19279f8cfbb58a925a80b52e690ad71ee0907134", emu::chip8::Variant::XO_CHIP, "Truck Simul8Or (buffi, 2015-07-28)", R"({"instructionsPerFrame": 1000, "advanced": {"col1": "#FFCC00", "col2": "#FF6600", "col3": "#662200", "col0": "#996600", "buzzColor": "#FFAA00", "quietColor": "#000000"}})", nullptr},
    {"193915dcde1365ae054c4eaa21a35baa27cd3356", emu::chip8::Variant::CHIP_8, "Breakout (Carmelo Cortez0(1979)", nullptr, nullptr},
    {"19c64fc12bfdefb8c3c608a37b433ceff4286e52", emu::chip8::Variant::CHIP_8, "Gem Catcher (Dakota Hernandez, 2017)", nullptr, nullptr},
    {"1b6dcf8c02ea0b89a4f04ce28e7c39a5e7a513d6", emu::chip8::Variant::XO_CHIP, "Sk8 H8 1988 (Willfor, 2015-10-28)", R"({"instructionsPerFrame": 20, "advanced": {"col1": "#7B0201", "col2": "#F2FAF7", "col3": "#E7ED3E", "col0": "#D4D4D4", "buzzColor": "#990099", "quietColor": "#330033"}})", nullptr},
    {"1ba58656810b67fd131eb9af3e3987863bf26c90", emu::chip8::Variant::CHIP_8, "IBM Logo", nullptr, nullptr},                        // IBM Logo.ch8
    {"1bd92042717c3bc4f7f34cab34be2887145a6704", emu::chip8::Variant::CHIP_8, "Spooky Spot (Joseph Weisbecker)", nullptr, nullptr},
    {"1bdb4ddaa7049266fa3226851f28855a365cfd12", emu::chip8::Variant::CHIP_8, "Syzygy (Roy Trevino, 1990)", nullptr, nullptr},
    {"1e3be162480380b6276d0848e1c71576b4c041f2", emu::chip8::Variant::CHIP_8, "Ghost Escape (TomRintjema, 2016-10-29)", R"({"instructionsPerFrame": 7, "optWrapSprites": true, "advanced": {"col1": "#FF00FF", "col2": "#FF6600", "col3": "#662200", "col0": "#00FFFF", "buzzColor": "#990099", "quietColor": "#330033", "screenRotation": 0}})", nullptr},
    //{"1e981dac636d88d26a3fc056a53b28175f1d9b82", {emu::Chip8EmulatorOptions::eXOCHIP}},
    {"1e981dac636d88d26a3fc056a53b28175f1d9b82", emu::chip8::Variant::XO_CHIP, "Modem Dialing (sound)", nullptr, nullptr},
    {"1ebcb2ec0be2ec9fa209d5c73be19b2d408399bf", emu::chip8::Variant::CHIP_8_COSMAC_VIP, "Hires Particle Demo (zeroZshadow, 2008)", nullptr, nullptr}, // Reveal Studios HIRES-CHIP-8 programs (actually self patching "Two page display CHIP-8", VIPER vol. 1, issue 3)
    //{"1f386e1ae47957dec485d3e4034dff706d316d15", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"1f386e1ae47957dec485d3e4034dff706d316d15", emu::chip8::Variant::SCHIPC, "Traffic (Christian Kosman, 2018)", nullptr, nullptr},
    {"1fe935d48dabe516f5119cc4c12e2edab36de8ec", emu::chip8::Variant::CHIP_8_TPD, nullptr, nullptr, nullptr}, // CHIP-8 VIP TPD (two page display, 64x64)
    //{"1ff6e2a8920c5b48def34348df65226285f39ce9", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"1ff6e2a8920c5b48def34348df65226285f39ce9", emu::chip8::Variant::SCHIPC, "Octovore (Pangasaurus Rex, 2015)", nullptr, nullptr},
    {"200b313e4d4c1970641142cc7ff578d7956b93da", emu::chip8::Variant::CHIP_8_COSMAC_VIP, nullptr, nullptr, nullptr},  // Reveal Studios HIRES-CHIP-8 programs (actually self patching "Two page display CHIP-8", VIPER vol. 1, issue 3)
    {"2079134ecaaaa356724d1f856b2a00153b176cc7", emu::chip8::Variant::XO_CHIP, "Jeff Quest (Jason DuPertuis, 2020)", nullptr, nullptr},
    {"20c2b4baf40c2c30c7db91107d4b5af980626f1c", emu::chip8::Variant::CHIP_8, "Chip8Stein 3D (demo)", nullptr, nullptr},
    //{"2229606a59bbcdeb81408f75e8646ea05553a580", {emu::Chip8EmulatorOptions::eXOCHIP}},
    {"2229606a59bbcdeb81408f75e8646ea05553a580", emu::chip8::Variant::XO_CHIP, "Horde (Dupersaurus, 2017)", nullptr, nullptr},
    {"234d1688bf4d1b34786cb9171b5f0800b3889874", emu::chip8::Variant::CHIP_8, "Lights Out (Dion Williams, 2016)", nullptr, nullptr},
    {"237756a4014fb3aa82a29246a7cdd534f8dc2dbb", emu::chip8::Variant::CHIP_8, "Breakout (David Winter, 1997)", nullptr, nullptr},
    {"238585615069ec905aa56f0048880fc6eb456d4e", emu::chip8::Variant::CHIP_8, "Loose Cannon (demo)", nullptr, nullptr},
    {"238e6fb829b03522d60568cac3d8f00de4a53bcf", emu::chip8::Variant::CHIP_8, "Dogfight (Jef Winsor, 1980)", nullptr, nullptr},
    {"23a08dc955d6afe95b9a4880b0e75d7fbf0b4dac", emu::chip8::Variant::XO_CHIP, "Octopi (TomR, 2021)", nullptr, nullptr},
    //{"244c746b4f81c9c3df9cea69389387da67589bb8", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"244c746b4f81c9c3df9cea69389387da67589bb8", emu::chip8::Variant::SCHIPC, "C-Tetris (Klaus von Sengbusch, 1994)", nullptr, nullptr},
    {"24960090b2afc9de2a4cb3ee7daf6a21456bb49b", emu::chip8::Variant::CHIP_8, "Russian Roulette (Carmelo Cortez)", nullptr, nullptr},
    {"2498050e4f5645574daefaa8a679576374c55973", emu::chip8::Variant::CHIP_8, "Shooth3Rd Ii Plus (Beholder, 2018)", nullptr, nullptr},
    {"24ef21009527ee674de44ccb37e37081654883f9", emu::chip8::Variant::XO_CHIP, "Alien-Inv8Sion", nullptr, nullptr},                 // Alien-Inv8sion.xo8
    //{"24fd50a95b84e3a42e336a06567a9752f17b9979", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"24fd50a95b84e3a42e336a06567a9752f17b9979", emu::chip8::Variant::SCHIPC, "Matches (unknown author)", nullptr, nullptr},
    {"27868be46213718792ab3b8415855a1975366dbe", emu::chip8::Variant::CHIP_8, "Chip-8 Snake (steveRoll, 2020)", nullptr, nullptr},
    {"28ac3467fbb4544a3e3a1ec3cd27d9e819ac7323", emu::chip8::Variant::CHIP_8, "Horse World Online (TomRintjema, 2014-10-28)", R"({"instructionsPerFrame": 30, "advanced": {"col1": "#783809", "col0": "#a0cb6d", "buzzColor": "#FFAA00", "quietColor": "#000000"}})", nullptr},
    //{"28e8f7b405d48647eb090a550ec679327c57f2f5", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"28e8f7b405d48647eb090a550ec679327c57f2f5", emu::chip8::Variant::SCHIPC, "Octo Slam (Dupersaurus, 2017)", nullptr, nullptr},
    {"2925c79f35e4ced1923b5ef8ba3e795951e6de21", emu::chip8::Variant::CHIP_8, "Morse Code Demo (Matthew Mikolay, 2015)", nullptr, nullptr},
    {"29bc3a658b1607b6458571d5fe99f495306a6a4f", emu::chip8::Variant::CHIP_8, "Corners Game (Kyle Saburao, 2019)", nullptr, nullptr},
    //{"29f83328069205a1cdb7020846cca34d6988c83c", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"29f83328069205a1cdb7020846cca34d6988c83c", emu::chip8::Variant::SCHIPC, "Serpinski (Sergey Naydenov, 2010)", nullptr, nullptr},
    //{"2b48aa674707878bf6d22496a402985a9f7db9cb", {emu::Chip8EmulatorOptions::eXOCHIP}},
    {"2b48aa674707878bf6d22496a402985a9f7db9cb", emu::chip8::Variant::XO_CHIP, "Multiply Routine Demo By (John Deeny, 2015)", nullptr, nullptr},
    {"2b711cf58008f03168d0547063fe8e3c72f65ae3", emu::chip8::Variant::CHIP_8_COSMAC_VIP, "Blackjack (Andrew-Modla)", nullptr, nullptr}, // CHIP-8 VIP (hybrid roms)
    {"2c761f70a44e521ee848834cfdd2bd1646157d29", emu::chip8::Variant::CHIP_8, "Super Pong (offstatic, 2021-10-15)", R"({"instructionsPerFrame": 30, "optWrapSprites": true, "optInstantDxyn": true, "optDontResetVf": true, "advanced": {"col1": "#43523d", "col2": "#ff6600", "col3": "#662200", "col0": "#c7f0d8", "buzzColor": "#FFAA00", "quietColor": "#000000", "screenRotation": 0, "fontStyle": "octo"}})", nullptr},
    {"2cc98ab06cd250960118585971e842b56af3085e", emu::chip8::Variant::XO_CHIP, "Chicken Scratch (John Earnest, 2020)", nullptr, nullptr},
    //{"2cd26a9a84ed2be6aaa6916d49b2e5c503196400", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"2cd26a9a84ed2be6aaa6916d49b2e5c503196400", emu::chip8::Variant::SCHIPC, "Car Race (Klaus von Sengbusch, 1994)", nullptr, nullptr},
    {"2cda3b309234e693e5ab6179767a8f019dfd5c6e", emu::chip8::Variant::XO_CHIP, "Infini Br8Kr (HailTheFish, 2020)", nullptr, nullptr},
    {"2cdcb3c29a5f013a991db5909ca8e18e27b3c42b", emu::chip8::Variant::CHIP_8, "Glitch Ghost (JackieKircher, 2014-10-29)", R"({"instructionsPerFrame": 200, "advanced": {"col1": "#FFFFFF", "col0": "#555555", "buzzColor": "#FFFFFF", "quietColor": "#000000"}})", nullptr},
    {"2cf29f1128192d461988ba077550ca2c0d577e4d", emu::chip8::Variant::CHIP_8X, "CHIP-8x Test 2", nullptr, nullptr},
    {"2d10c07b532f4fa7c07a07324ba26ca39fe484fd", emu::chip8::Variant::CHIP_8, "Connect 4 (David Winter)", nullptr, nullptr},
    //{"2d415bf1f31777b22ad73208c4d1ad27d5d4f367", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"2d415bf1f31777b22ad73208c4d1ad27d5d4f367", emu::chip8::Variant::SCHIPC, "Superworm V4 (RB & Revival Studios, 2007)", nullptr, nullptr},
    {"2d6369d3e2ecfe3f180a5c4b8ad3513efacfd04f", emu::chip8::Variant::CHIP_8, "Worm (rstein, 2020)", nullptr, nullptr},
    {"2dabe15f846041b24faa21a6dc3632fdebe82b89", emu::chip8::Variant::CHIP_8, "Hello World (David Campion, 2019)", nullptr, nullptr},
    {"2dbb5b53121ec84cb2377fcb645e57cc8b5eaa09", emu::chip8::Variant::CHIP_8, "Sqrt Test Program (Sergey Naydenov, 2010)", nullptr, nullptr},
    {"2e0f2268c9a3be0fbb839f918336f161815bb80a", emu::chip8::Variant::CHIP_8, "Carbon8 (Chromatophore, 2018-11-01)", R"({"instructionsPerFrame": 15, "optWrapSprites": true, "advanced": {"col1": "#FFCC00", "col0": "#0000FF", "buzzColor": "#FFFFFF", "quietColor": "#000000", "screenRotation": 0}})", nullptr},
    {"2e2d9b370e08d6994fd2ded938a56b32b07ad768", emu::chip8::Variant::CHIP_8, "Mini Lights Out (Tobias V. Langhoff, 2019)", nullptr, nullptr},
    //{"2e87573b0fe9a49123bbb86d8384d00a302dc2e4", {emu::Chip8EmulatorOptions::eXOCHIP}},
    {"2e87573b0fe9a49123bbb86d8384d00a302dc2e4", emu::chip8::Variant::XO_CHIP, "You'Re Correct Horse ((sound)", nullptr, nullptr},
    {"2f34cace9cda8f04829b0cd0b39a3a1726fd4193", emu::chip8::Variant::CHIP_8, "Yas (Marco Varesio, 2015)", nullptr, nullptr},
    {"30c8ee7181173d7f213e8148cdc9a5157caec9f7", emu::chip8::Variant::CHIP_8, "Mueve (Diego Royo, 2017)", nullptr, nullptr},
    //{"310e523071c697503f0da385997f9c77f9ad0ea9", {emu::Chip8EmulatorOptions::eXOCHIP}},
    {"310e523071c697503f0da385997f9c77f9ad0ea9", emu::chip8::Variant::XO_CHIP, "Mario Demo (Dr.Stab, 2015)", nullptr, nullptr},
    {"318c6359405f8b1512b325c53eb119a6f9d57aef", emu::chip8::Variant::CHIP_8, "Spoong (SupSuper, 2018)", nullptr, nullptr},
    {"31bb555e6a1b06502425500a7fc61bc9d1a49164", emu::chip8::Variant::CHIP_8, "Lombat Lombat Asoy! (Razka173 Team, 2018)", nullptr, nullptr},
    {"31fc1c53cc610a9f4b9c5705c5a0f33fc028d123", emu::chip8::Variant::CHIP_8, "Br8Kout (SharpenedSpoon, 2014-09-01)", R"({"instructionsPerFrame": 7, "advanced": {"col1": "#ff84fe", "col0": "#ca2553", "buzzColor": "#FFAA00", "quietColor": "#f090e4"}})", nullptr},
    //{"31fe380556d65600ef293d99aabd3b6bb119aa01", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"31fe380556d65600ef293d99aabd3b6bb119aa01", emu::chip8::Variant::SCHIPC, "Field! (Al Roland, 199x)", nullptr, nullptr},
    //{"332e892ad054cf182e1ca4c465b603b8261ccec9", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"332e892ad054cf182e1ca4c465b603b8261ccec9", emu::chip8::Variant::SCHIPC, "Snow Daze (unknown author)", nullptr, nullptr},
    {"3368d56efeb584c509bafb548f1ee5e71ac1bc70", emu::chip8::Variant::CHIP_8, "Biorhythm (Jef Winsor)", nullptr, nullptr},
    //{"33abb5f1ba7db3166636911c6cfa81a5ce5b861c", {emu::Chip8EmulatorOptions::eXOCHIP}},
    {"33abb5f1ba7db3166636911c6cfa81a5ce5b861c", emu::chip8::Variant::XO_CHIP, "Octo Paint (your name here, 2016-06-12)", R"({"instructionsPerFrame": 30, "advanced": {"col1": "#FFCC00", "col2": "#FF6600", "col3": "#662200", "col0": "#996600", "buzzColor": "#FFAA00", "quietColor": "#000000"}})", nullptr},
    //{"33ec2f3081bed56438dc207477f06cd77f3f07d9", {emu::Chip8EmulatorOptions::eXOCHIP}},
    {"33ec2f3081bed56438dc207477f06cd77f3f07d9", emu::chip8::Variant::XO_CHIP, "Business Is Contagious (JohnEarnest, 2020-02-05)", R"({"instructionsPerFrame": 1000, "advanced": {"col1": "#43523d", "col2": "#43523d", "col3": "#43523d", "col0": "#c7f0d8", "buzzColor": "#43523d", "quietColor": "#43523d", "screenRotation": 0}})", nullptr},
    {"3450e0d92e0bbf8e9d3065fd088cd6dfa5f9441d", emu::chip8::Variant::CHIP_8, "Octo Bird (Cody Hoover, 2016)", nullptr, nullptr},
    {"346f2760ca55bb6d45b1f255fe4960a7d244191e", emu::chip8::Variant::CHIP_8_COSMAC_VIP, "Bingo (Andrew Modla)", nullptr, nullptr}, // CHIP-8 VIP (hybrid roms)
    {"35158696bd94ea22ef34e899fff1f15f7154d4fd", emu::chip8::Variant::CHIP_8, "Craps (Camerlo Cortez, 1978)", nullptr, nullptr},
    {"3643118e2e237deab98151b742f34caf9533dc05", emu::chip8::Variant::CHIP_8, "Mandelbrot Program (A-KouZ1, 2018)", nullptr, nullptr},
    {"39970ccfd3a3f00180d53464d4fd7862193eaf0f", emu::chip8::Variant::CHIP_8, "Octo: A Chip 8 Story (SystemLogoff, 2015-10-29)", R"({"instructionsPerFrame": 7, "advanced": {"col0": "#111111", "col1": "#EEEEEE", "buzzColor": "#FFFF00", "quietColor": "#222222"}})", nullptr},
    {"3a840c33442ad9e912df1fa2aa61833bf571af34", emu::chip8::Variant::CHIP_8, "Private Eye (TCNJ S.572.37)", nullptr, nullptr},
    {"3b2bf5dc7ffb5f3fbe168e802079f79730535ca8", emu::chip8::Variant::CHIP_8, "Figures", nullptr, nullptr},                 // Figures.ch8
    {"3b644b6d5a5591999094b22478a8efa3739da85d", emu::chip8::Variant::CHIP_8, "Grave Digger (TomR, 2017)", nullptr, nullptr},
    {"3be683d1ac0b27ae47a09984e420853fff0b7e0d", emu::chip8::Variant::CHIP_8, "Pet Dog (SystemLogoff, 2015-10-31)", R"({"instructionsPerFrame": 7, "advanced": {"col1": "#fafafa", "col2": "#FF6600", "col3": "#662200", "col0": "#0c0c0c", "buzzColor": "#fafafa", "quietColor": "#1a1a1a"}})", nullptr},
    //{"3c444e43e5f02dac4324b7b24cd38ef4938a4b56", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"3c444e43e5f02dac4324b7b24cd38ef4938a4b56", emu::chip8::Variant::SCHIPC, "Loopz (Andreas Daumann, 1993)", nullptr, nullptr},
    {"3cb8831051c0b6235b64f057a6a848a57d8900df", emu::chip8::Variant::CHIP_8, "Hello World (Joel Yliluoma, 2015)", nullptr, nullptr},
    {"3d1d029d6e31206d245c0ba881c0d1f003953bad", emu::chip8::Variant::CHIP_8, "Rocket (Joseph Weisbecker)", nullptr, nullptr},
    {"3dade9be601637ca2d96aeafaa086b93a0b83352", emu::chip8::Variant::XO_CHIP, "Frog", nullptr, nullptr},                   // Frog.xo8
    {"3ddf7b76b8f63d0089e00e3b518f78c213b74b1e", emu::chip8::Variant::CHIP_8, "8Ce Attourny - Disc 1 (SystemLogoff, 2016-10-30)", R"({"instructionsPerFrame": 7, "optWrapSprites": true, "advanced": {"col1": "#f1f1f1", "col2": "#FF6600", "col3": "#662200", "col0": "#0f0f0f", "buzzColor": "#aaff55", "quietColor": "#777777", "screenRotation": 0}})", nullptr},
    {"3ea97f251de6e72798234a2930205256a8f5d8cf", emu::chip8::Variant::CHIP_8, "Mysterious (Guillaume Desquesnes, 2019)", nullptr, nullptr},
    //{"3ee8a64a9af37a8d24aab9e73410b94cc0a4018f", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"3ee8a64a9af37a8d24aab9e73410b94cc0a4018f", emu::chip8::Variant::SCHIPC, "H8 (Mastigophoran, 2017)", nullptr, nullptr},
    {"3f9ef8dec999574a188ec3b9615cff9888283c85", emu::chip8::Variant::CHIP_8, "Tank! (Rectus, 2018-10-31)", R"({"instructionsPerFrame": 200})", nullptr},
    {"3fd62ae2bfe2572ceb194f1d3d1bd5a01695b86c", emu::chip8::Variant::XO_CHIP, "Jub8 Song 6 (your name here, 2016-08-31)", R"({"instructionsPerFrame": 1000, "advanced": {"col1": "#000000", "col2": "#FDFFD5", "col3": "#BA5A1A", "col0": "#353C41", "buzzColor": "#353C41", "quietColor": "#353C41", "screenRotation": 0}})", nullptr},
    {"400dbd1aa2b79b9b8546bc615bfb735c1bd1d268", emu::chip8::Variant::CHIP_8, "Cave Explorer (JohnEarnest, 2014-06-22)", R"({"instructionsPerFrame": 20, "advanced": {"col0": "#996600", "col1": "#FFCC00", "buzzColor": "#FFAA00", "quietColor": "#000000"}})", nullptr},
    {"402ea1ede1cc4ab1c074b89b2ed5e9845f056fc3", emu::chip8::Variant::GENERIC_CHIP_8, "5-Quirks (Timendus, 2023-11-13)", nullptr, "@GH/Timendus/chip8-test-suite/v4.1/bin/5-quirks.ch8"},
    {"4031dae5c7545a1adc160a661be36f19fc1d47b2", emu::chip8::Variant::CHIP_8, "Nim (Carmelo Cortez)", nullptr, nullptr},
    {"40329847cb898f9b34a6aea1095be0a1be0b4546", emu::chip8::Variant::CHIP_8, "Tic-Tac-Toe (David Winter)", nullptr, nullptr},
    {"40c33f5ae6f11def69a445220b3c96a6009f92ed", emu::chip8::Variant::CHIP_8, "Chesmac (Raimo Suonio, 1979)", nullptr, nullptr},
    //{"416763e940918ee7cfc5c277d7f2b66de71a46a1", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"416763e940918ee7cfc5c277d7f2b66de71a46a1", emu::chip8::Variant::SCHIPC, "Jump Heart (Buffis, 201x)", nullptr, nullptr},
    {"417ba551bc92751d3e0dc25e01e76373d0e504ea", emu::chip8::Variant::CHIP_8, "Chip-8 Dino (Anthony Pham, 2019)", nullptr, nullptr},
    {"419a0110d41332457c15ae09fff62cbd7ad197fc", emu::chip8::Variant::CHIP_8, "8Ce Attourny - Disc 3 (SystemLogoff, 2016-10-30)", R"({"instructionsPerFrame": 7, "optWrapSprites": true, "advanced": {"col1": "#f1f1f1", "col2": "#FF6600", "col3": "#662200", "col0": "#0f0f0f", "buzzColor": "#aaff55", "quietColor": "#777777", "screenRotation": 0}})", nullptr},
    {"41f2a4e7f372795e3d6ad657de8622c0169248db", emu::chip8::Variant::CHIP_8, "Shooth3Rd Ii (Beholder, 2018)", nullptr, nullptr},
    {"4200636c4d2a4495d10d6348049d21b887e8d1be", emu::chip8::Variant::CHIP_8, "Space Explorer (TCNJ S.572.2)", nullptr, nullptr},
    {"429d455a4bc53167942bf6fd934d72b0f648dce3", emu::chip8::Variant::CHIP_8, "Tic-Tac-Toe (David Winter)", nullptr, nullptr},
    {"4333eff4cbb49e57f8c0fb12f1e4cd0ac1dedd56", emu::chip8::Variant::CHIP_8, "Labview Splash Screen (Richard James Lewis, 2019)", nullptr, nullptr},
    {"4309cba3fb0b96761fcba01acaf233e0ca585b4d", emu::chip8::Variant::GENERIC_CHIP_8, "5-Quirks (Timendus, 2023-04-12)", nullptr, "@GH/Timendus/chip8-test-suite/v4.0/bin/5-quirks.ch8"},
    //{"440c5fbe9f5f840e76c308738fb0d37772d66674", {emu::Chip8EmulatorOptions::eXOCHIP}},
    {"440c5fbe9f5f840e76c308738fb0d37772d66674", emu::chip8::Variant::XO_CHIP, "Super Neatboy (JohnEarnest, 2020-08-01)", R"({"instructionsPerFrame": 1000, "optWrapSprites": false, "advanced": {"col1": "#E6E6FA", "col2": "#FF1493", "col3": "#FF1493", "col0": "#100010", "buzzColor": "#000000", "quietColor": "#000000", "screenRotation": 0, "fontStyle": "octo"}})", nullptr},
    {"442eb1cead3a3467d0e7fe9d557fb6eadc662094", emu::chip8::Variant::CHIP_8X, "CHIP-8x Test 1", nullptr, nullptr},
    {"443550abf646bc7f475ef0466f8e1232ec7474f3", emu::chip8::Variant::CHIP_8, "Shooting Stars (Philip Baltzer, 1978)", nullptr, nullptr},
    {"448f9d30d2157ab42679b809d4fb0b43d145f74f", emu::chip8::Variant::CHIP_8, "Sequence Shoot (Joyce Weisbecker)", nullptr, nullptr},
    {"453545dc5e6079e9d9be9d3775d2615c4b60724f", emu::chip8::Variant::SCHIP_1_1, nullptr, nullptr, nullptr},
    //{"4564a1bf149e5ab9777d33813a92cfd6ffc7a0bb", {emu::Chip8EmulatorOptions::eXOCHIP}},
    {"4564a1bf149e5ab9777d33813a92cfd6ffc7a0bb", emu::chip8::Variant::XO_CHIP, "Kesha Was Niiinja (Kesha, 2017-10-31)", R"({"instructionsPerFrame": 1000, "advanced": {"col1": "#b22d10", "col2": "#10b258", "col3": "#ffffff", "col0": "#283593", "buzzColor": "#182583", "quietColor": "#182583", "screenRotation": 0}})", nullptr},
    //{"45f7c33b284b0f3e1393f0dd97e4b3b9fd9c63c9", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"45f7c33b284b0f3e1393f0dd97e4b3b9fd9c63c9", emu::chip8::Variant::SCHIPC, "Horsey Jump (Team Horse, 2015)", nullptr, nullptr},
    {"4639f86beb0a203ae512b85d3b56d813b2dea7b4", emu::chip8::Variant::CHIP_8, "Rush Hour (Hap, 2006-12-17)", nullptr, nullptr},
    {"466ce147503c536b23a7548d6adf027c26d28df3", emu::chip8::Variant::XO_CHIP, "Rocket (Jason DuPertuis, 2020)", nullptr, nullptr},
    {"46b281516a3e9d1526bea224b79cc18ddd71833d", emu::chip8::Variant::XO_CHIP, "Computer Simulator", nullptr, nullptr},                     // Computer Simulator.xo8
    {"4701417c61d80d40fe6e3ae06d891cbe730c0dc7", emu::chip8::Variant::CHIP_8_COSMAC_VIP, "X-Ray (Rand-Miller, 1980, fixed)", nullptr, nullptr}, // CHIP-8 VIP (hybrid roms)
    //{"480b4dfa0918d034aea0bf8d8ef5b5a55e94b50b", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"480b4dfa0918d034aea0bf8d8ef5b5a55e94b50b", emu::chip8::Variant::SCHIPC, "Super Trip8 Demo (Revival Studios, 2008)", nullptr, nullptr},
    {"493c76b9d9252e1d349d408d341daff5267f35fa", emu::chip8::Variant::CHIP_8, "Octojam 3 Title (JohnEarnest, 2016-09-25)", R"({"instructionsPerFrame": 7, "optWrapSprites": true, "advanced": {"col1": "#FFAA00", "col2": "#FF6600", "col3": "#662200", "col0": "#AA4400", "buzzColor": "#FFAA00", "quietColor": "#000000", "screenRotation": 0}})", nullptr},
    {"49c7234a1733db355560a13c57b26f055533c233", emu::chip8::Variant::CHIP_8, "Fishie (Hap, 2005-07-10)", nullptr, nullptr},
    {"4a4123320d841ed04d8c1cd2ad6132a06b83dfa0", emu::chip8::Variant::CHIP_8, "Minimal Game (Revival Studios, 2007)", nullptr, nullptr},
    {"4a4c47e886d576c8e5172d797a276601084004bb", emu::chip8::Variant::CHIP_8, "Patterns (SystemLogoff, 2019)", nullptr, nullptr},
    {"4a68389601eafe3adf014576681eb30232acdac9", emu::chip8::Variant::CHIP_8, "Classic Snek (Andrew James, 2021)", nullptr, nullptr},
    {"4a89a66cfe78f788f45598d69f975d470229df0b", emu::chip8::Variant::CHIP_8_COSMAC_VIP, nullptr, nullptr, nullptr}, // Reveal Studios HIRES-CHIP-8 programs (actually self patching "Two page display CHIP-8", VIPER vol. 1, issue 3)
    {"4ac6414b1fd502074a6aab4de4b206a7273dcfb8", emu::chip8::Variant::CHIP_8, "Chip2048 (Lime, 2014)", nullptr, nullptr},
    {"4cb8bc4ddcfd23822c4a38990ac7e4225a323cec", emu::chip8::Variant::CHIP_8, "No_Rom_Selected (MissMuffin, 2019)", nullptr, nullptr},
    {"4cc4eff70802ac7a3b374a442411a13415f5e4d8", emu::chip8::Variant::XO_CHIP, "No Internet (pushfoo, 2020)", nullptr, nullptr},
    //{"4cce9f3a79c8d7ee33a9bfde7099568e0f3274cd", {emu::Chip8EmulatorOptions::eXOCHIP}},
    {"4cce9f3a79c8d7ee33a9bfde7099568e0f3274cd", emu::chip8::Variant::XO_CHIP, "Dump Trump (Micheal Wales, 2019)", nullptr, nullptr},
    {"4d223e2919e0f2463f2561d836015c6cd6c18eeb", emu::chip8::Variant::XO_CHIP, "Xotrackerdemov0", nullptr, nullptr},                        // XOTrackerDemov0.1.1.xo8
    {"4edb2848edbec6c79a2ae208490e12013e94ee98", emu::chip8::Variant::CHIP_8, "Flappy Pong (cnelmortimer, 2017)", nullptr, nullptr},
    {"507e7dc6783565071dfe4b72154af431d4466958", emu::chip8::Variant::CHIP_8, "Particle Demo (zeroZshadow, 2008)", nullptr, nullptr},
    //{"518c1d40f5d768ee49d2b7951d998588ef8238ba", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"518c1d40f5d768ee49d2b7951d998588ef8238ba", emu::chip8::Variant::XO_CHIP, "Wonky Pong (TomRintjema, 2018-11-01)", R"({"instructionsPerFrame": 100, "advanced": {"col0": "#FFFFFF", "col1": "#000000", "buzzColor": "#666666", "quietColor": "#000000"}})", nullptr},
    //{"51a31cc51414b4dd6c5c54081574f915e6f53744", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"51a31cc51414b4dd6c5c54081574f915e6f53744", emu::chip8::Variant::SCHIPC, "Seconds Counter (Michael Wales, 2014)", nullptr, nullptr},
    {"5303be6c79bff9426b2f4b1fa9af1f4a5bbcd525", emu::chip8::Variant::CHIP_8, "Computer (John Earnest, 2014)", nullptr, nullptr},
    //{"531c44e8204d8ab8c078bad36e34067baddfccdb", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"531c44e8204d8ab8c078bad36e34067baddfccdb", emu::chip8::Variant::SCHIPC, "Sw8 Copter (JohnEarnest, 2014-09-02)", R"({"instructionsPerFrame": 100, "advanced": {"col0": "#117799", "col1": "#FFFFFF", "buzzColor": "#1166DD", "quietColor": "#000000"}})", nullptr},
    {"5370ecf9ae444c71b63dab9b1f9968a4fe67c9dd", emu::chip8::Variant::CHIP_8, "Blinky (Hans Christian Egeberg)", nullptr, nullptr},
    {"54d892ef1ac3ac2d2ff2b58d8662d0374eb77cdd", emu::chip8::Variant::CHIP_8, "Team Chipotle Intro (Ethan Pini, 2019)", nullptr, nullptr},
    {"5551471e152afcbf61707393ce79cde360bbc23c", emu::chip8::Variant::CHIP_8, "Heart Monitor Demo (Matthew Mikolay, 2015)", nullptr, nullptr},
    {"55eab50c53a102bea5d2848d29d6546fb79ae0c0", emu::chip8::Variant::GENERIC_CHIP_8, "3-Corax+ (Timendus, 2023-11-13)", nullptr, "@GH/Timendus/chip8-test-suite/v4.1/bin/3-corax%2B.ch8"},
    {"5634ce5f7f08fee69eec2327529c646d5f596be0", emu::chip8::Variant::XO_CHIP, "Octoamp", nullptr, nullptr},                        // Octoamp.xo8
    {"565b40c19d653521a0257c28a92671d0e594f22a", emu::chip8::Variant::XO_CHIP, "Akir8 (TomR, 2020)", nullptr, nullptr},
    {"57b4b5fa3251dee9d6b588327cf5ff8d194d4a04", emu::chip8::Variant::XO_CHIP, "Safecracker", nullptr, nullptr},                    // Safecracker.xo8
    {"58b4865fec81427fd3c52bdc62b2230d412c12ea", emu::chip8::Variant::CHIP_8, "Monty Hall (blinky, 2016)", nullptr, nullptr},
    //{"58f7ce407aedf456dc8992342f4a6f9f0647383b", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"58f7ce407aedf456dc8992342f4a6f9f0647383b", emu::chip8::Variant::SCHIP_1_1, "Sens8Tion (Chromatophore, 2016-11-01)", R"({"instructionsPerFrame": 20, "optWrapSprites": true, "advanced": {"col1": "#1a3279", "col2": "#FF6600", "col3": "#662200", "col0": "#bad9b6", "buzzColor": "#fff6d6", "quietColor": "#000000", "screenRotation": 0}})", nullptr},
    {"599135c0a9bb33ce1bc5396fd30abef2df1cb2ed", emu::chip8::Variant::MEGA_CHIP, nullptr, nullptr, nullptr},
    {"59aca79b4b18e1bfbc71065bb34448fed5e1db1e", emu::chip8::Variant::CHIP_8, "Laser Defence (Kyle Saburao, 2019)", nullptr, nullptr},
    //{"59bdc7f990322d274d711b6b6982c7e8c9098e9e", {emu::Chip8EmulatorOptions::eXOCHIP}},
    {"59bdc7f990322d274d711b6b6982c7e8c9098e9e", emu::chip8::Variant::XO_CHIP, "Music Player (TomR, 2015)", nullptr, nullptr},
    {"5a183cc0530410c0887175ccaf6d5d4deb5d8fff", emu::chip8::Variant::CHIP_8, "Chip-Otto Logo 2 (Marco Varesio, 2015)", nullptr, nullptr},
    {"5a2c897da9cc78f6d75123818e04db4cd1044b63", emu::chip8::Variant::CHIP_8, "Psx Boot Sim (Shendo)", nullptr, nullptr},
    {"5a6366decb08df66da8bd685b497ccce0884c307", emu::chip8::Variant::CHIP_8, "Octojam 9 Title (JohnEarnest, 2022-10-01)", R"({"instructionsPerFrame": 15, "optWrapSprites": true, "optDontResetVf": true, "advanced": {"col1": "#FFCC00", "col2": "#FF6600", "col3": "#662200", "col0": "#996600", "buzzColor": "#FFAA00", "quietColor": "#000000", "screenRotation": 0, "fontStyle": "octo"}})", nullptr},
    //{"5abf3dcf4ce0e396a3a5bf977b1ea988535d35d5", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"5abf3dcf4ce0e396a3a5bf977b1ea988535d35d5", emu::chip8::Variant::SCHIPC, "00Schip8 Life Demo (Henry de Jongh, 2016)", nullptr, nullptr},
    {"5b29263763be401c31d805bc35a4cd211d552881", emu::chip8::Variant::CHIP_8, "Jumping X And O (Harry Kleinberg, 1977)", nullptr, nullptr},
    {"5b66aa248b3a0b6fffa6a72fdbee5e14e05d3f77", emu::chip8::Variant::MEGA_CHIP, nullptr, nullptr, nullptr},
    //{"5b733a60e7208f6aa0d15c99390ce4f670b2b886", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"5b733a60e7208f6aa0d15c99390ce4f670b2b886", emu::chip8::Variant::SCHIPC, "Blinky (Hans Christian Egeberg, 1991)", nullptr, nullptr},
    //{"5c0fff21df64f3fe8683a115353c293d435ca01a", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"5c0fff21df64f3fe8683a115353c293d435ca01a", emu::chip8::Variant::SCHIPC, "First Depth Search Of Maze (AKouZ1, 2017)", nullptr, nullptr},
    {"5c28a5f85289c9d859f95fd5eadbdcb1c30bb08b", emu::chip8::Variant::CHIP_8, "Space Invaders (David Winter, 1978)", R"({"optJustShiftVx": true})", nullptr},
    {"5c82520906073287a3ef781746c67207ca084d93", emu::chip8::Variant::CHIP_8, "Cave", nullptr, nullptr},                    // Cave.ch8
    //{"5d99d0c763cf528660a10a390abe89f2d12b024a", {emu::Chip8EmulatorOptions::eXOCHIP}},
    {"5d99d0c763cf528660a10a390abe89f2d12b024a", emu::chip8::Variant::XO_CHIP, "Jeff'S Quest (Dupersaurus, 2017)", nullptr, nullptr},
    {"5e70f91ca08e9b9e9de61670492e3db2d7f7d57a", emu::chip8::Variant::CHIP_8, "Rocket Launch (Jonas Lindstedt, 19xx)", R"({"optLoadStoreDontIncI": true})", nullptr},
    //{"5efc16ddebc1585b3c4d4cb27ce9fd76218c5d0a", {emu::Chip8EmulatorOptions::eXOCHIP}},
    {"5efc16ddebc1585b3c4d4cb27ce9fd76218c5d0a", emu::chip8::Variant::XO_CHIP, "Legboy'S Adventure 8 - Doki Doki Property Planning Panic (Faffochip, 2015)", nullptr, nullptr},
    {"5f518084744bf3cb8733f6e5454dfd1634320563", emu::chip8::Variant::CHIP_8, "Tetris (Fran Dachille, 1991)", nullptr, nullptr},
    {"607c4f7f4e4dce9f99d96b3182bfe7e88bb090ee", emu::chip8::Variant::CHIP_8, "Pong (1 player)", nullptr, nullptr},
    {"614a2b3d0bb5d62a16d963ac2d3a79eb3dd22742", emu::chip8::Variant::CHIP_8, "Coin Flipping (Carmelo Cortez, 1978)", nullptr, nullptr},
    //{"61931487c694c5bc6978ae22c0a36aca5a647e24", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"61931487c694c5bc6978ae22c0a36aca5a647e24", emu::chip8::Variant::SCHIPC, "Nokia 3310 Template (JohnEarnest, 2020-01-20)", R"({"instructionsPerFrame": 30, "optWrapSprites": true, "advanced": {"col1": "#c7f0d8", "col2": "#c7f0d8", "col3": "#c7f0d8", "col0": "#43523d", "buzzColor": "#43523d", "quietColor": "#43523d", "screenRotation": 0}})", nullptr},
    {"6194da2a89a3f431674d7323bf30f5ffe2f7190d", emu::chip8::Variant::CHIP_8, "Tetris (Verisimilitudes, 2020)", nullptr, nullptr},
    //{"627f01b20ce4d33f6df1aa88acb405a3a732bde0", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"627f01b20ce4d33f6df1aa88acb405a3a732bde0", emu::chip8::Variant::SCHIPC, "Dvn8 (SystemLogoff, 2017-10-30)", R"({"instructionsPerFrame": 20, "optWrapSprites": true, "advanced": {"col1": "#1f1f1f", "col2": "#FF6600", "col3": "#662200", "col0": "#f0f0f0", "buzzColor": "#aa2222", "quietColor": "#000000", "screenRotation": 0}})", nullptr},
    {"62e204572ac05be3748a746ac7831d6844f43003", emu::chip8::Variant::XO_CHIP, "3D Viper Maze (Timendus, 2020)", nullptr, nullptr},
    {"63458c204bd24234a33d263d965ea8d16dd5c9e8", emu::chip8::Variant::CHIP_8, "Tank-Viper (Demo)", nullptr, nullptr},
    //{"63e787fc3e78e5fb3a394cf1bc654ad9633d8907", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"63e787fc3e78e5fb3a394cf1bc654ad9633d8907", emu::chip8::Variant::SCHIPC, "Mondri8 (JohnEarnest, 2014-09-25)", R"({"instructionsPerFrame": 100, "advanced": {"col0": "#996600", "col1": "#FFCC00", "buzzColor": "#FFAA00", "quietColor": "#000000"}})", nullptr},
    {"64176ff030ebff27f483db5a16f38f2383d0026e", emu::chip8::Variant::SCHIP_1_1, nullptr, nullptr, nullptr},
    //{"642e6174ac7b2bccb7d0845eb5f18d2defbe98b4", {emu::Chip8EmulatorOptions::eXOCHIP}},
    {"642e6174ac7b2bccb7d0845eb5f18d2defbe98b4", emu::chip8::Variant::XO_CHIP, "Replicator (Björn Kempen, 2015)", nullptr, nullptr},
    {"64536d549c986e9edf25de9fa89db60d2ade85c0", emu::chip8::Variant::SCHIP_1_1, nullptr, nullptr, nullptr},
    {"64536d549c986e9edf25de9fa89db60d2ade85c0", emu::chip8::Variant::SCHIPC, "Sub-Terr8Nia (your name here, 2017-08-31)", R"({"instructionsPerFrame": 60, "optWrapSprites": true, "advanced": {"col1": "#ffffff", "col2": "#FF6600", "col3": "#662200", "col0": "#000000", "buzzColor": "#FFAA00", "quietColor": "#111111", "screenRotation": 270}})", nullptr},
    {"659cb966e976fcbcae76f6a8a07c65be4d18aae8", emu::chip8::Variant::CHIP_8, "Space Racer (WilliamDonnelly, 2017-10-30)", R"({"instructionsPerFrame": 20, "optWrapSprites": true, "advanced": {"col1": "#FCFCFC", "col2": "#FF6600", "col3": "#662200", "col0": "#111111", "buzzColor": "#FFAA00", "quietColor": "#000000", "screenRotation": 0}})", nullptr},
    {"65e3432c942df6ce18db2c2d01d3260e56dd1e53", emu::chip8::Variant::CHIP_8, "Lady Runner (noodulz, 2020)", nullptr, nullptr},
    //{"66068809c482a30aa4475dc554a18d8d727d3521", {emu::Chip8EmulatorOptions::eXOCHIP}},
    {"66068809c482a30aa4475dc554a18d8d727d3521", emu::chip8::Variant::XO_CHIP, "Scroll Pattern 1 (Björn Kempen, 2015)", nullptr, nullptr},
    {"669e32b6f42f52da658e428f501aabcdfa37fb2e", emu::chip8::Variant::CHIP_8, "Mastermind Fourrow (Robert Lindley, 1978)", nullptr, nullptr},
    //{"66c15e550c9cda39b50220c49d22578dabbfe319", {emu::Chip8EmulatorOptions::eXOCHIP}},
    {"66c15e550c9cda39b50220c49d22578dabbfe319", emu::chip8::Variant::XO_CHIP, "Patterns (Bjorn Kempen, 2015)", nullptr, nullptr},
    {"66d44799bc15637f742cf30d84007f412a9c9fb5", emu::chip8::Variant::XO_CHIP, "Chip-8 Calculator", nullptr, nullptr},                      // Chip-8 Calculator.xo8
    {"67384436edd903e4b0051be02c600730d649dd4b", emu::chip8::Variant::GENERIC_CHIP_8, "8-scrolling (Timendus, 2023-11-13)", nullptr,"@GH/Timendus/chip8-test-suite/v4.1/bin/8-scrolling.ch8"},
    {"67996195539c0ddcd98533a01dffeec6a53a6da1", emu::chip8::Variant::CHIP_8, "Timebomb", nullptr, nullptr},                        // Timebomb.ch8
    {"67ee534ad376d89f0d5d78a99c006a847e28c016", emu::chip8::Variant::CHIP_8, "Patrick'S Chip-8 Challenge (Tobias V. Langhoff, 2019)", nullptr, nullptr},
    {"67efbd5a84fe1337c3c9cb3040981ec4ce52577b", emu::chip8::Variant::SCHIPC, "Applejack (John Earnest, 2020)", nullptr, nullptr},
    {"680e265a128870091ed71410891b64d5ead303fb", emu::chip8::Variant::CHIP_8, "Jumping Sprite (lingib, 2020)", nullptr, nullptr},
    //{"681eaf2c6422cdd0e0ca0cf9f4c3a436b7b6f292", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"681eaf2c6422cdd0e0ca0cf9f4c3a436b7b6f292", emu::chip8::Variant::SCHIPC, "Binding Of Cosmac (buffi, 2016)", nullptr, nullptr},
    {"683cbb00d59e48fa34c3bbd2fd2b10775b22178a", emu::chip8::Variant::MEGA_CHIP, nullptr, nullptr, nullptr},
    {"6881684726d8bf97379d8eb988a9cdfb373c1698", emu::chip8::Variant::CHIP_8, "Snake (Tyson Decker, 2016)", nullptr, nullptr},
    {"68b6f9336c1bdc4dcaf7fca78c3a719894bdd376", emu::chip8::Variant::CHIP_8, "Super Block (Joshua Barretto, 2019)", nullptr, nullptr},
    {"693ba52f822c2e2713c5329ae77ff3271e5b954f", emu::chip8::Variant::CHIP_8, "Pixlar (Ethan Pini, 2019)", nullptr, nullptr},
    {"69956a514173f08926e7f4388c8c8fc6b5b465a1", emu::chip8::Variant::CHIP_8, "Ckosmic (Christian Kosman, 2018)", nullptr, nullptr},
    {"6a846ca9fed73a7ef0e6695a665d2f15dd6a8141", emu::chip8::Variant::CHIP_8, "Artifac (Ethan Pini, 2019)", nullptr, nullptr},
    //{"6b6502b03183e492f8170172308df9876c29d1d9", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"6b6502b03183e492f8170172308df9876c29d1d9", emu::chip8::Variant::SCHIPC, "Single Dragon (David Nurser, 199x)", nullptr, nullptr},
    {"6b9f23e6433b7d7ccfcb18015c5fc1348006d386", emu::chip8::Variant::CHIP_8, "Memory Shift (A-Kouz1, 2017)", nullptr, nullptr},
    {"6ba06eb27ad56e6f26b7d809e06394f719a89d01", emu::chip8::Variant::CHIP_8, "Minimal Nethack (John Earnest, 2015)", nullptr, nullptr},
    //{"6bb78d8a0aba93ea18eabdd0134cbdccd1dc2d16", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"6bb78d8a0aba93ea18eabdd0134cbdccd1dc2d16", emu::chip8::Variant::SCHIPC, "Climax Slideshow - Part 2 (Revival Studios, 2008)", nullptr, nullptr},
    //{"6d4514ae3a43c307763648b0bdd485fb77bcf20d", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"6d4514ae3a43c307763648b0bdd485fb77bcf20d", emu::chip8::Variant::SCHIPC, "Mines! - The Minehunter (David Winter, 199x)", nullptr, nullptr},
    //{"6d677bb44500a5ee4754b3a75516cfd9e73947fc", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"6d677bb44500a5ee4754b3a75516cfd9e73947fc", emu::chip8::Variant::SCHIPC, "Joust (Erin S. Catto, 199x)", nullptr, nullptr},
    {"6dc9b7bee24f9793b929d20a7757725f3183d12e", emu::chip8::Variant::SCHIPC, "3D Vip'R Maze (Tim Franssen, 2021)", nullptr, nullptr},
    {"6dde2db154ea508431d38b43a0f8b4a641e0439e", emu::chip8::Variant::CHIP_8, "Planet Of The Eights (Comrat-2016)", nullptr, nullptr},
    {"6df358d77961a0bf21e98876f9f616791cba31e3", emu::chip8::Variant::CHIP_8, "Soccer", nullptr, nullptr},                  // Soccer.ch8
    //{"6e556d92f30a75e7fa8016891438ae082ef33ad4", {emu::Chip8EmulatorOptions::eXOCHIP}},
    {"6e556d92f30a75e7fa8016891438ae082ef33ad4", emu::chip8::Variant::XO_CHIP, "Super Octo Track X-O (Blastron, 2015)", nullptr, nullptr},
    {"6e7cb52ec99e10f934b76eaf3fddeb8f2e2e14e1", emu::chip8::Variant::CHIP_8, "Tomb Ston Tipp (TomRintjema, 2018-10-12)", R"({"instructionsPerFrame": 7, "optWrapSprites": true, "advanced": {"col1": "#FFFFFF", "col2": "#FF6600", "col3": "#662200", "col0": "#000000", "buzzColor": "#FFAA00", "quietColor": "#000000", "screenRotation": 0}})", nullptr},
    {"6f6509f38220e057a7e32ebb22dd353c1078e3e7", emu::chip8::Variant::CHIP_8, "Blitz (David Winter)", nullptr, nullptr},
    //{"6f8e85158be98f30bf3cd5df60d7a7ad71c5f3e1", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"6f8e85158be98f30bf3cd5df60d7a7ad71c5f3e1", emu::chip8::Variant::SCHIPC, "Mr (Ryan Hitchman, 2014)", nullptr, nullptr},
    {"702066d7248dfa81d5535942e7c6ed3a32ebc84c", emu::chip8::Variant::SCHIP_1_1, nullptr, nullptr, nullptr},
    //{"709328365147967f434d1bf78430e9ec160cc24f", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"709328365147967f434d1bf78430e9ec160cc24f", emu::chip8::Variant::SCHIPC, "Worms Demo (unknown author)", nullptr, nullptr},
    {"70aa0e7f25f0f0fd6ec7c59e427bf1d03ee95617", emu::chip8::Variant::CHIP_8_COSMAC_VIP, nullptr, nullptr, nullptr}, // Reveal Studios HIRES-CHIP-8 programs (actually self patching "Two page display CHIP-8", VIPER vol. 1, issue 3)
    {"70ccd390c90f586bcd75bbbc1c89e53e67179ff1", emu::chip8::Variant::CHIP_8, "Flipflipboom (Ian J Sikes, 2016)", nullptr, nullptr},
    {"7143cb2e8c895eccbc1d768417c932bde8337b94", emu::chip8::Variant::XO_CHIP, "Skellespresso", nullptr, nullptr},                  // Skellespresso.xo8
    {"716fc9634c39f73afe795004589d353448a6c8e3", emu::chip8::Variant::CHIP_8, "Labyrinthine (TempVar Studios, 2020)", nullptr, nullptr},
    {"7171deb1dabdf37d7f87507a11d4c07d11690b97", emu::chip8::Variant::CHIP_8, "Walking Dog (John Earnest, 2015)", nullptr, nullptr},
    {"71d06da9e605804d2099b808c02548ab2b3511b2", emu::chip8::Variant::CHIP_8_COSMAC_VIP, nullptr, nullptr, nullptr}, // Reveal Studios HIRES-CHIP-8 programs (actually self patching "Two page display CHIP-8", VIPER vol. 1, issue 3)
    {"726cb39afa7e17725af7fab37d153277d86bff77", emu::chip8::Variant::CHIP_8, "Programmable Spacefighters (Jef Winsor)", nullptr, nullptr},
    {"72c2cbfea48000e25891dd4968ae9f1adef1e7e3", emu::chip8::Variant::CHIP_8, "Bmp Viewer (Hap, 2005)", R"({"optJustShiftVx": true})", nullptr},
    {"72e8f3a10a32bd7fb91322ecab87249f95e81e57", emu::chip8::Variant::CHIP_8, "Lunar Lander (Udo Pernisz, 1979)", nullptr, nullptr},
    {"72f071d5197497519d301ec32baef749f3191a4d", emu::chip8::Variant::CHIP_8, "Golf (buffi, 2019)", nullptr, nullptr},
    {"72fb3e0a4572bdb81f484df7948a8bc736fe78d0", emu::chip8::Variant::CHIP_8, "Landing", nullptr, nullptr},                 // Landing.ch8
    //{"7321e1bbe885a749b2ca875d1f49fb6c01f54f91", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"7321e1bbe885a749b2ca875d1f49fb6c01f54f91", emu::chip8::Variant::SCHIPC, "U-Boat (Michael Kemper, 1994)", nullptr, nullptr},
    //{"733d41d4c367214cd177071ee6a783a46cf14bf4", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"733d41d4c367214cd177071ee6a783a46cf14bf4", emu::chip8::Variant::SCHIPC, "Octoroads (abbeyj, 2017)", nullptr, nullptr},
    {"738c6a6ef6a285c3cdd708960413c0a3b1682c7e", emu::chip8::Variant::CHIP_8, "Videah Logo (Ruairidh Carmichael, 2015)", nullptr, nullptr},
    {"738f8dd1aa53e5043ee9b45521b0af118c3970ee", emu::chip8::Variant::XO_CHIP, "Wave Logo", nullptr, nullptr},                      // WAVE Logo.xo8
    {"74936ffb0db233c722f5a39932d75240b6437a72", emu::chip8::Variant::SCHIPC, "Octovore (JackieKircher, 2015-11-02)", R"({"instructionsPerFrame": 120, "advanced": {"col1": "#5B1B96", "col2": "#E03B22", "col3": "#68E022", "col0": "#8BDCE8", "buzzColor": "#68E022", "quietColor": "#000000"}})", nullptr},
    {"75fac059356e7f47c7ac27afb8523162a9ffa2b5", emu::chip8::Variant::CHIP_8, "Down8 (this is not a team, 2015)", nullptr, nullptr},
    {"7623fa0fa915979226566b24107360e7537735f4", emu::chip8::Variant::CHIP_8, "Slide (Joyce Weisbecker)", nullptr, nullptr},
    //{"76a770000b314659ac792e17724b783a464ab67e", {emu::Chip8EmulatorOptions::eXOCHIP}},
    {"76a770000b314659ac792e17724b783a464ab67e", emu::chip8::Variant::XO_CHIP, "Civiliz8N (tann, 2016-09-26)", R"({"instructionsPerFrame": 100, "advanced": {"col1": "#754D27", "col2": "#738C3A", "col3": "#E2CB8A", "col0": "#141421", "buzzColor": "#FFAA00", "quietColor": "#000000", "screenRotation": 0}})", nullptr},
    {"775e82a36c93f1b41b42eca94b55acbc4a48cebe", emu::chip8::Variant::CHIP_8, "Tapeworm (JDR, 1999)", nullptr, nullptr},
    //{"77d5d2d9c5fe19c72d6564b3601a8d17cfedcb41", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"77d5d2d9c5fe19c72d6564b3601a8d17cfedcb41", emu::chip8::Variant::SCHIPC, "10 Bytes Pattern (Björn Kempen, 2015)", nullptr, nullptr},
    {"77dc518e6779ccd862205cfeb0f3f7772caed60e", emu::chip8::Variant::XO_CHIP, "Expedition. (JohnEarnest, 2021-01-06)", R"({"instructionsPerFrame": 1000, "advanced": {"col1": "#43523d", "col2": "#43523d", "col3": "#43523d", "col0": "#c7f0d8", "buzzColor": "#43523d", "quietColor": "#43523d", "screenRotation": 0, "fontStyle": "octo"}})", nullptr},
    {"7851dd47c67217426f31b27778b19d39407a9bf2", emu::chip8::Variant::SCHIPC, "Bull8 H3Ll (Flamore, 2020)", nullptr, nullptr},
    {"788661c6a49c4e081492416bf2ce86342116bb1d", emu::chip8::Variant::CHIP_8, "Crack Me (Pawel Lukasik, 2017)", nullptr, nullptr},
    //{"7a4a89870f2ab23c28024dd1c3dd52cf1af1ad00", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"7a4a89870f2ab23c28024dd1c3dd52cf1af1ad00", emu::chip8::Variant::SCHIPC, "Octopeg (Chromatophore, 2015-10-29)", R"({"instructionsPerFrame": 200, "advanced": {"col1": "#acd5ff", "col2": "#FF6600", "col3": "#662200", "col0": "#113152", "buzzColor": "#264c74", "quietColor": "#000000"}})", nullptr},
    {"7b6f54169a9511c976fdaa85db3822d9c707077f", emu::chip8::Variant::GENERIC_CHIP_8, "6-keypad (Timendus, 2023-04-12)", nullptr,"@GH/Timendus/chip8-test-suite/v4.0/bin/6-keypad.ch8"},
    {"7c680cd427c2d0eecde208ebbce667707c0f13a2", emu::chip8::Variant::CHIP_8, "Simple Snek (John Earnest, 2021)", nullptr, nullptr},
    {"7cd0334fc30cbbb21d3c5a909fa2c69927ec4a6c", emu::chip8::Variant::CHIP_8, "Alternate (TCNJ S.572.3)", nullptr, nullptr},
    {"7d38669b1542d2352b900eed11b78dd1c8d144ec", emu::chip8::Variant::CHIP_8, "Breakfree (David Winter)", nullptr, nullptr},
    {"7d6cc6068ea324f81873353d8a28fe2aa2cf8862", emu::chip8::Variant::CHIP_8, "M'Lady (demo)", nullptr, nullptr},
    {"7d9abc18f187fafeb6f799f42882ff6007d340ac", emu::chip8::Variant::CHIP_8, "Sneak Surround (TomSwan, 2020)", nullptr, nullptr},
    {"7da3eba52a8d8025ddf14ee40d28f151585529a0", emu::chip8::Variant::CHIP_8, "Piper (Aeris, JordanMecom, LillianWang, 2017-01-23)", R"({"instructionsPerFrame": 200, "optWrapSprites": true, "advanced": {"col1": "#FFCC00", "col0": "#0000FF", "buzzColor": "#FFFFFF", "quietColor": "#000000", "screenRotation": 0}})", nullptr},
    {"7dbd54b5adc7e409b64a716ceafba864301128b8", emu::chip8::Variant::CHIP_8, "Enchantment Enhanced (Verisimilitudes, 2020)", nullptr, nullptr},
    {"7dc6605ed7b139330ee7e1dec33efba76486f4d7", emu::chip8::Variant::CHIP_8, "Rulerbrain (Group 8 Team, 2019)", nullptr, nullptr},
    {"7e46d8b67ccc71be45591089d2ec187f7ca8883e", emu::chip8::Variant::CHIP_8, "Vip Demo (unknown author)", nullptr, nullptr},
    {"7e53264cda0014e108182e449fdd3034b6bd53c3", emu::chip8::Variant::CHIP_8, "F8Z (demo)", nullptr, nullptr},
    {"7e8d5a79cabeb9a791524ea7126867a539d825ee", emu::chip8::Variant::XO_CHIP, "Jub8 Song 4 (your name here, 2016-08-31)", R"({"instructionsPerFrame": 1000, "advanced": {"col1": "#000000", "col2": "#FDFFD5", "col3": "#BA5A1A", "col0": "#353C41", "buzzColor": "#353C41", "quietColor": "#353C41", "screenRotation": 0}})", nullptr},
    {"7fb69647e6b10e2b12f9357d5c1c177349028236", emu::chip8::Variant::SCHIPC, "Dodge (JohnEarnest, 2021-10-01)", R"({"instructionsPerFrame": 1000, "optWrapSprites": true, "advanced": {"col1": "#FFCC00", "col2": "#FF6600", "col3": "#662200", "col0": "#996600", "buzzColor": "#FFAA00", "quietColor": "#000000", "screenRotation": 0, "fontStyle": "octo"}})", nullptr},
    //{"808aeb072604809e0ef13c245115a81f40422d1d", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"808aeb072604809e0ef13c245115a81f40422d1d", emu::chip8::Variant::SCHIPC, "Flutter By (Tom Rintjema, 2019)", nullptr, nullptr},
    {"80d8baefbc2c2c2eab78a7b09c621f7618357b84", emu::chip8::Variant::CHIP_8, "Minesweep8R (James Kohli aka Hottie Pippen, 2014)", nullptr, nullptr},
    {"80feda2028aa31788d3d1d9e062d77d2fd9308cc", emu::chip8::Variant::XO_CHIP, "Octoma (Cratmang, 2021-10-25)", R"({"instructionsPerFrame": 10000, "optLoadStoreDontIncI": true, "advanced": {"col1": "#FF00FF", "col2": "#00FFFF", "col3": "#FFFFFF", "col0": "#000000", "buzzColor": "#000000", "quietColor": "#000000", "screenRotation": 0, "fontStyle": "fish"}})", nullptr},
    {"80ffa819cfa42f2f5f9f836b67c666d01a915970", emu::chip8::Variant::CHIP_8, "Tower Of Hanoi (Joel Yliluoma, 2015)", nullptr, nullptr},
    {"8109e5f502a624ce6c96b8aa4b44b3f7dc0ef968", emu::chip8::Variant::CHIP_10, nullptr, nullptr, nullptr},
    {"8166328ddd1deb0df718323c0c63c76b267cec4a", emu::chip8::Variant::CHIP_8, "Arrows (Ashton Harding, 2018)", nullptr, nullptr},
    {"817c7c8429c82ecbdb3c57ab68c449c2e4c447b9", emu::chip8::Variant::HI_RES_CHIP_8X, "CHIP-8x Four Page Display", nullptr, nullptr},
    {"8198311054b6cd440dde42d6efed0eda1b1e461d", emu::chip8::Variant::XO_CHIP, "D8Gn (SystemLogoff, 2020)", nullptr, nullptr},
    {"821751787374cc362f4c58759961f0aa7a2fd410", emu::chip8::Variant::CHIP_8, "Flight Runner (TodPunk, 2014-11-01)", R"({"instructionsPerFrame": 15, "advanced": {"col1": "#FFCC00", "col0": "#0000FF", "buzzColor": "#FFFFFF", "quietColor": "#000000"}})", nullptr},
    {"8263bac7d98d94097171f0a5dc6f210f77543080", emu::chip8::Variant::CHIP_8, "Octo Rancher (SystemLogoff, 2018-10-30)", R"({"instructionsPerFrame": 7, "optWrapSprites": true, "advanced": {"col1": "#f1f1f1", "col2": "#FF6600", "col3": "#662200", "col0": "#0f0f0f", "buzzColor": "#aaff55", "quietColor": "#777777", "screenRotation": 0}})", nullptr},
    {"82fd0d202a068bedfb869fc303fdeae0c814024f", emu::chip8::Variant::CHIP_8, "8Ce Attourny - Disc 2 (SystemLogoff, 2016-10-30)", R"({"instructionsPerFrame": 7, "optWrapSprites": true, "advanced": {"col1": "#f1f1f1", "col2": "#FF6600", "col3": "#662200", "col0": "#0f0f0f", "buzzColor": "#aaff55", "quietColor": "#777777", "screenRotation": 0}})", nullptr},
    //{"83300ff710acdd8417376b88adf40f68171f7ec7", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"83300ff710acdd8417376b88adf40f68171f7ec7", emu::chip8::Variant::SCHIPC, "Chipcross (buffi, 2018-10-15)", R"({"instructionsPerFrame": 20, "optWrapSprites": true, "advanced": {"col1": "#FFCC00", "col2": "#FF6600", "col3": "#662200", "col0": "#996600", "buzzColor": "#FFAA00", "quietColor": "#000000", "screenRotation": 0}})", nullptr},
    {"835880c85c4c1c318b04dc940e89cb6e7466e652", emu::chip8::Variant::CHIP_8, "Greet (Boro Sitnikovski, 2014)", nullptr, nullptr},
    {"838706ee2d7001e6e909360f9a02f48d81d3a0d6", emu::chip8::Variant::XO_CHIP, "Octo Space Program (rozisdead, 2020)", nullptr, nullptr},
    {"83a2f9c8153be955c28e788bd803aa1d25131330", emu::chip8::Variant::CHIP_8, "Sum Fun (Joyce Weisbecker)", nullptr, nullptr},
    {"84d612c7eccf24835eb585711a49964572444737", emu::chip8::Variant::CHIP_8, "Horse World Online (TomR, 2014)", nullptr, nullptr},
    {"852f506c6a56bd9f59592c4a1cb5a0aaaf31381c", emu::chip8::Variant::XO_CHIP, "Cool 3D Spinning Octo", nullptr, nullptr},                  // Cool 3D Spinning Octo.xo8
    //{"858b55ce47e98a7b2238f8db33463f76fd15b18b", {emu::Chip8EmulatorOptions::eXOCHIP}},
    {"858b55ce47e98a7b2238f8db33463f76fd15b18b", emu::chip8::Variant::XO_CHIP, "Life Is Gr8 (Faffochip, 2015)", nullptr, nullptr},
    //{"8603e177fcbb04a5b1a685c216380bee6a05b0f2", {emu::Chip8EmulatorOptions::eXOCHIP}},
    {"8603e177fcbb04a5b1a685c216380bee6a05b0f2", emu::chip8::Variant::XO_CHIP, "Octo Roads (James Abbatiello, 2016)", nullptr, nullptr},
    {"8713062b1983c26090b742a9ffc30777c007ff93", emu::chip8::Variant::CHIP_8, "Drag Ram (ChaseParate, 2020)", nullptr, nullptr},
    {"89247fc70ab073b36cb1b6a6ea3770ac4a877b9b", emu::chip8::Variant::CHIP_8, "Hedgehog The Drug Dog (FunkyStu, 2016)", nullptr, nullptr},
    {"898ef1505c874065697ffc6cba688367e143d82e", emu::chip8::Variant::CHIP_8, "Mastermind (WilliamDonnelly, 2015-11-08)", R"({"instructionsPerFrame": 20, "advanced": {"col1": "#FCFCFC", "col2": "#FF6600", "col3": "#662200", "col0": "#005E20", "buzzColor": "#FFAA00", "quietColor": "#000000"}})", nullptr},
    {"89929eb46c0682caad909d30561a934f28941963", emu::chip8::Variant::CHIP_8, "Clostro (jibbl, 2020)", nullptr, nullptr},
    {"89aadf7c28bcd1c11e71ad9bd6eeaf0e7be474f3", emu::chip8::Variant::CHIP_8, "Submarine (Carmelo Cortez)", nullptr, nullptr},
    {"8af7d183230de959a53ec84418b9e2609838d3fb", emu::chip8::Variant::CHIP_8, "Only The Good Die Neil Young (cishetkayfaber, 2020)", nullptr, nullptr},
    //{"8b2fc2e08830b8a9e604d11c9b319e2cc0a581b3", {emu::Chip8EmulatorOptions::eXOCHIP}},
    {"8b2fc2e08830b8a9e604d11c9b319e2cc0a581b3", emu::chip8::Variant::XO_CHIP, "T8Nks (your name here, 2015-08-31)", R"({"instructionsPerFrame": 1000, "advanced": {"col1": "#554422", "col2": "#456543", "col3": "#EEEEFF", "col0": "#87CEEB", "buzzColor": "#FFFFFF", "quietColor": "#000000"}})", nullptr},
    {"8b70080adbac44513ec60005734a816372b845ec", emu::chip8::Variant::CHIP_8, "Maze (David Winter, 199x)", nullptr, nullptr},
    {"8c404dc15f854456cafe9b22fcdbaf16830ffde5", emu::chip8::Variant::CHIP_8, "Tick-Tack-Toe (Joseph Weisbecker, 1977)", nullptr, nullptr},
    {"8c7f101c61f82cacaacc45f8c11c1a00c8cc451e", emu::chip8::Variant::GENERIC_CHIP_8, "6-Keypad (Timendus, 2023-04-12)", nullptr, "@GH/Timendus/chip8-test-suite/v4.0/bin/6-keypad.ch8"},
    {"8cf29db367b7db4760dee8252dfc88066a45ce4a", emu::chip8::Variant::CHIP_8, "Jumpfill (Bj”rn Kempen, 2015)", nullptr, nullptr},
    {"8d56a781bf16acccb307177b80ff326f62aabbdc", emu::chip8::Variant::CHIP_8_COSMAC_VIP, nullptr, nullptr, nullptr}, // Reveal Studios HIRES-CHIP-8 programs (actually self patching "Two page display CHIP-8", VIPER vol. 1, issue 3)
    {"8e5f19d8ae9f3346779613359610967a5ed95fa8", emu::chip8::Variant::CHIP_8, "Deflection (John Fort)", nullptr, nullptr},
    {"8e96555ee62ed3c4dcd082fdef5d16450dcb99af", emu::chip8::Variant::GENERIC_CHIP_8, "1-chip8-logo (Timendus, 2023-11-13)", nullptr,"@GH/Timendus/chip8-test-suite/v4.1/bin/1-chip8-logo.ch8"},
    //{"8ebf74e790e58a8d5a7beff598bb32ed7eeeabf7", {emu::Chip8EmulatorOptions::eXOCHIP}},
    {"8ebf74e790e58a8d5a7beff598bb32ed7eeeabf7", emu::chip8::Variant::XO_CHIP, "Skyward (tann, JackieKircher, 2016-11-01)", R"({"instructionsPerFrame": 1000, "optLoadStoreDontIncI": true, "advanced": {"col1": "#4B636F", "col2": "#AF2D3D", "col3": "#AF2D3D", "col0": "#121212", "buzzColor": "#000000", "quietColor": "#000000", "screenRotation": 0}})", nullptr},
    //{"8fd0212f4b8b491e8eb260e995313bdb210b1d6b", {emu::Chip8EmulatorOptions::eXOCHIP}},
    {"8fd0212f4b8b491e8eb260e995313bdb210b1d6b", emu::chip8::Variant::XO_CHIP, "Tmnt-Nes (sound)", nullptr, nullptr},
    //{"91015f51f6ffd0043a2ae757dc11ec35216949ef", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"91442577a6bbf8c3267f2df95fdfc50baebe176d", emu::chip8::Variant::CHIP_8, "Brick (Brix hack, 1990)", nullptr, nullptr},
    //{"92a325c36ad2116a5256946b8bf711ed9befd319", {emu::Chip8EmulatorOptions::eXOCHIP}},
    {"92a325c36ad2116a5256946b8bf711ed9befd319", emu::chip8::Variant::XO_CHIP, "Kesha Was Biird (Kesha, 2016-10-31)", R"({"instructionsPerFrame": 1000, "advanced": {"col1": "#1a1c19", "col2": "#a0a29f", "col3": "#5d5b5d", "col0": "#fafdf9", "buzzColor": "#222211", "quietColor": "#212110", "screenRotation": 0}})", nullptr},
    {"93cc9ed25534f9b143206f846c2a9145df691d6c", emu::chip8::Variant::CHIP_8, "Flip-8 - Think-A-Dot Edition (Tobias V. Langhoff, 2020)", nullptr, nullptr},
    {"9441cd611eb019217621a11ebeba15b499bbd31e", emu::chip8::Variant::CHIP_8, "Tank Warfare (unknown author)", nullptr, nullptr},
    {"945fa6dd1ac72f1ede1cb829ef31b5328a32f67a", emu::chip8::Variant::CHIP_8, "Etch-A-Sketch (KrzysztofJeszke, 2020)", nullptr, nullptr},
    {"9468a94294997009a2c50c1a18376947d3d3d3bb", emu::chip8::Variant::CHIP_8, "Driving Simulator (Team 15 Chipotle, 2019)", nullptr, nullptr},
    {"949b661091efe706a32fb0d89991005783243bb9", emu::chip8::Variant::GENERIC_CHIP_8, "3-Corax+ (Timendus, 2023-04-12)", nullptr, "@GH/Timendus/chip8-test-suite/v4.0/bin/3-corax%2B.ch8"},
    {"9514e9e2ab7e1ddc91265823e7e895de6c8dd303", emu::chip8::Variant::SCHIP_1_1, nullptr, nullptr, nullptr},
    {"95384fbb895b6420da690bc06cb16739c9a5d800", emu::chip8::Variant::CHIP_8, "Maze276 (Firas Fakih, 2019)", nullptr, nullptr},
    {"9593099c1fe1be31cbaea526aa04fd492ff90382", emu::chip8::Variant::SCHIP_1_1, nullptr, nullptr, nullptr},
    {"959ed7d6b61e667bb59d1b497401258463f88454", emu::chip8::Variant::CHIP_8, "Octojam 8 Title (JohnEarnest, 2021-10-01)", R"({"instructionsPerFrame": 7, "optWrapSprites": true, "optInstantDxyn": true, "optDontResetVf": true, "advanced": {"col1": "#FFAA00", "col2": "#FF6600", "col3": "#662200", "col0": "#AA4400", "buzzColor": "#FFAA00", "quietColor": "#000000", "screenRotation": 0, "fontStyle": "fish"}})", nullptr},
    {"96bc31f23c7f917dab4a082d2c7fd7c69820e6a4", emu::chip8::Variant::SCHIP_1_1, nullptr, nullptr, nullptr},
    {"96c0ae3b45839a570d180760835ceab9ed503fd0", emu::chip8::Variant::CHIP_8, "Deflap (hitcherland, 2015)", nullptr, nullptr},
    //{"9797a7eaf1e80ec19c085c60bb37991420f54678", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"9797a7eaf1e80ec19c085c60bb37991420f54678", emu::chip8::Variant::SCHIPC, "Grad School Simulator 2014 (JohnEarnest, 2014-09-15)", R"({"instructionsPerFrame": 20, "advanced": {"col0": "#808080", "col1": "#000000", "buzzColor": "#FFAA00", "quietColor": "#000000"}})", nullptr},
    {"97a65f7c877f923a93e7b423ad39187e91e938fd", emu::chip8::Variant::CHIP_8, "Snafu (Shendo, 2010)", nullptr, nullptr},
    {"981e7029587172681765243b45445fcc382139b3", emu::chip8::Variant::SCHIPC, "Simple Dodge (John Earnest, 2021)", nullptr, nullptr},
    {"9909082230fd33218ac374acaeaaefbb786e3194", emu::chip8::Variant::GENERIC_CHIP_8, "6-keypad (Timendus, 2023-11-13)", nullptr,"@GH/Timendus/chip8-test-suite/v4.1/bin/6-keypad.ch8"},
    //{"99a97c772fc93d669b73016761ea6fee0210497e", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"99a97c772fc93d669b73016761ea6fee0210497e", emu::chip8::Variant::SCHIPC, "Minesweeper (AKouZ1, 2017)", nullptr, nullptr},
    {"9a09cdfa7b0820310043dd0f098384c1c9b325a3", emu::chip8::Variant::MEGA_CHIP, nullptr, nullptr, nullptr},
    {"9a9c341571ace516c9789b1eb92590833af13239", emu::chip8::Variant::CHIP_8, "Octojam 7 Title (JohnEarnest, 2020-08-07)", R"({"instructionsPerFrame": 30, "optWrapSprites": true, "optInstantDxyn": true, "optDontResetVf": true, "advanced": {"col1": "#ff8c1f", "col2": "#FF6600", "col3": "#662200", "col0": "#662200", "buzzColor": "#FFAA00", "quietColor": "#000000", "screenRotation": 0, "fontStyle": "octo"}})", nullptr},
    {"9b72b6656cb714cd64de00ac78dc7bf8374adec6", emu::chip8::Variant::CHIP_8, "Hidden (David Winter)", nullptr, nullptr},
    {"9b7faac49c44c1194a3283c2ef89eabfca76fe38", emu::chip8::Variant::SCHIP_1_1, nullptr, nullptr, nullptr},
    {"9be0cc119f8e3c18b7f0203c54b30c50b8f438a9", emu::chip8::Variant::CHIP_8, "Sierpchaos (Marco Varesio, 2015)", nullptr, nullptr},
    //{"9bf96e23963995c6d702ae21c9b8741cbb688f47", {emu::Chip8EmulatorOptions::eXOCHIP}},
    {"9bf96e23963995c6d702ae21c9b8741cbb688f47", emu::chip8::Variant::XO_CHIP, "Jub8 Song 3 (your name here, 2016-08-31)", R"({"instructionsPerFrame": 1000, "advanced": {"col1": "#000000", "col2": "#FDFFD5", "col3": "#BA5A1A", "col0": "#353C41", "buzzColor": "#353C41", "quietColor": "#353C41", "screenRotation": 0}})", nullptr},
    {"9bfae01da1a94f99aba692da1a7a2148eb8561b4", emu::chip8::Variant::CHIP_8_COSMAC_VIP, "Pinball (Andrew Modla)", nullptr, nullptr}, // CHIP-8 VIP (hybrid roms)
    {"9c05f5295282abfd89483790191ea59f9d031de5", emu::chip8::Variant::XO_CHIP, "Tapeworm", nullptr, nullptr},                       // Tapeworm.xo8
    {"9d834860f455aec7e95fb886984497e5be501610", emu::chip8::Variant::CHIP_8, "Slippery Slope (JohnEarnest, 2018-10-31)", R"({"instructionsPerFrame": 30, "optWrapSprites": true, "advanced": {"col1": "#000080", "col2": "#FF6600", "col3": "#662200", "col0": "#B0E0E6", "buzzColor": "#FFAA00", "quietColor": "#000000", "screenRotation": 0}})", nullptr},
    //{"9d9f88509b5033152b7b49d2c7ea3c3c5fce2bd6", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"9d9f88509b5033152b7b49d2c7ea3c3c5fce2bd6", emu::chip8::Variant::SCHIPC, "Climax Slideshow - Part 1 (Revival Studios, 2008)", nullptr, nullptr},
    {"9dc674f4a7c8662671e9337421acea49e0447090", emu::chip8::Variant::CHIP_8, "Octo Slam-Home Run Derby (Jason DuPertuis, 2017)", nullptr, nullptr},
    {"9ddbccdef6b5d4b9740103ce79d19607e0b785a1", emu::chip8::Variant::CHIP_8, "Display Numbers (Michael Wales, 2018)", nullptr, nullptr},
    {"9f55f7abc8f2bc4b59a01515f1d887a6568a8ab4", emu::chip8::Variant::CHIP_8, "Octojam 4 Title (JohnEarnest, 2017-09-23)", R"({"instructionsPerFrame": 30, "optWrapSprites": true, "advanced": {"col1": "#FFAA00", "col2": "#FF6600", "col3": "#662200", "col0": "#AA4400", "buzzColor": "#FFAA00", "quietColor": "#000000", "screenRotation": 0}})", nullptr},
    //{"9f7cf6fe0025878c26b317160c57edd06b3361ba", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"9f7cf6fe0025878c26b317160c57edd06b3361ba", emu::chip8::Variant::SCHIPC, "Super Square (tann, 2014-11-04)", R"({"instructionsPerFrame": 500, "optWrapSprites": true, "advanced": {"col1": "#FFFFFF", "col0": "#552200", "buzzColor": "#FFFFFF", "quietColor": "#000000", "screenRotation": 0}})", nullptr},
    {"9f9a4affbf7afd70bb594fb321e16579318c0164", emu::chip8::Variant::CHIP_8, "Spacejam! (WilliamDonnelly, 2015-10-30)", R"({"instructionsPerFrame": 100, "advanced": {"col1": "#fcfcfc", "col2": "#FF6600", "col3": "#662200", "col0": "#111111", "buzzColor": "#FFAA00", "quietColor": "#000000"}})", nullptr},
    {"9ffb063f600f670b682bf6a010292d5aa0a67efd", emu::chip8::Variant::CHIP_8, "Demo-Poo (Juraj Borza, 2020)", nullptr, nullptr},
    {"a0073e944d5ae9ca14324543fdf818907de80449", emu::chip8::Variant::CHIP_8, "Sirpinski (Sergey Naydenov, 2010)", nullptr, nullptr},
    //{"a05844df3305738e4030512f0063db2fe4f3bd11", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"a05844df3305738e4030512f0063db2fe4f3bd11", emu::chip8::Variant::SCHIPC, "Spacefight 2091 (Carsten Soerensen, 1992)", nullptr, nullptr},
    //{"a168709fcf09b28cd9b9519698d3d8a383944f43", {emu::Chip8EmulatorOptions::eXOCHIP}},
    {"a168709fcf09b28cd9b9519698d3d8a383944f43", emu::chip8::Variant::XO_CHIP, "Lan8Ton'S Ant (Faffochip, 2015)", nullptr, nullptr},
    {"a18f1e3897416180b32e47ddc82cba9aca2c8d52", emu::chip8::Variant::CHIP_8, "Paddles", nullptr, nullptr},                 // Paddles.ch8
    {"a1c1e0e7b01004be3ee77c69030e6b536cb316e6", emu::chip8::Variant::CHIP_8, "Superworm V4 (RB-Revival Studios, Martijn Wenting, 2007)", nullptr, nullptr},
    {"a1ec824285a593cd1ca84dc6c732c61b0fe96330", emu::chip8::Variant::SCHIP_1_1, nullptr, nullptr, nullptr},
    //{"a2788177b820a28cd27e6d2d180340cb7f4948fb", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"a2788177b820a28cd27e6d2d180340cb7f4948fb", emu::chip8::Variant::SCHIPC, "Loopz (hap, 2006)", nullptr, nullptr},
    {"a27dcf88a931f70c3ccf3c01a5410b263bac48bc", emu::chip8::Variant::CHIP_8, "Animal Race (Brian Astle)", nullptr, nullptr},
    {"a2807d2b9591a2cb061e3c3a64c2766b4bab4327", emu::chip8::Variant::CHIP_8, "Fractal Set (A-KouZ1, 2018)", nullptr, nullptr},
    {"a28c25586a38b0e6147092a2bc50899b463528df", emu::chip8::Variant::CHIP_8, "Elite International Golf", nullptr, nullptr},                        // Elite International Golf.ch8
    {"a3b80d4a9efa8e7700d348d3e3ddf81d3c7e92a9", emu::chip8::Variant::CHIP_8, "Blitz (David Winter)", nullptr, nullptr},
    {"a3f0eae99964b873eb1adbd3e8bcb90d15f762c3", emu::chip8::Variant::CHIP_8, "Mini Lights Out (tobiasvl, 2019-10-04)", R"({"instructionsPerFrame": 7})", nullptr},
    {"a4a9351775b2a64bbd14e3980968db19c254a988", emu::chip8::Variant::XO_CHIP, "Flutter By (TomRintjema, 2019-10-08)", R"({"instructionsPerFrame": 7, "advanced": {"col1": "#9BBC0F", "col2": "#8BAC0F", "col3": "#306230", "col0": "#0F380F", "buzzColor": "#333333", "quietColor": "#000000", "screenRotation": 0}})", nullptr},
    //{"a4c8e14b43dc75bc960a42a5300f64dc6e52cf32", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"a4c8e14b43dc75bc960a42a5300f64dc6e52cf32", emu::chip8::Variant::SCHIPC, "Stars (Sergey Naydenov, 2010)", nullptr, nullptr},
    {"a558e24022e30dd5206909eeca074949f3fb6f59", emu::chip8::Variant::SCHIP_1_1, nullptr, nullptr, nullptr},
    //{"a56c09537df0f32e2d49fb68cb2ba8216b38f632", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"a56c09537df0f32e2d49fb68cb2ba8216b38f632", emu::chip8::Variant::SCHIPC, "Ant - In Search Of Coke (Erin S. Catto, 199x)", nullptr, nullptr},
    {"a58ec7cc63707f9e7274026de27c15ec1d9945bd", emu::chip8::Variant::CHIP_8, "Squash (David Winter, 19xx)", nullptr, nullptr},
    {"a60611339661e3ab2d8af024ad1da5880a6f8665", emu::chip8::Variant::CHIP_8, "Pong (center-line, 19xx)", nullptr, nullptr},
    {"a6a6cb2351c20b8f904da07c0ce91bd8161e9317", emu::chip8::Variant::CHIP_8, "Tron", nullptr, nullptr},                    // Tron.ch8
    {"a6ad3ff3a6e969f87535e9733850630e9d221e51", emu::chip8::Variant::CHIP_8, "Smile (demo)", nullptr, nullptr},
    {"a6f237c853c19160ed7375a93c7f554e5a41aae3", emu::chip8::Variant::CHIP_8, "Enchantment Extra Enhanced (Verisimilitudes, 2020)", nullptr, nullptr},
    {"a6f3ac2d89cdc1d7b22013301863bad6a4fb7318", emu::chip8::Variant::CHIP_8, "Rps (SystemLogoff, 2015-10-25)", R"({"instructionsPerFrame": 7, "advanced": {"col0": "#AA9999", "col1": "#220000", "buzzColor": "#FFAA00", "quietColor": "#000000"}})", nullptr},
    {"a7aba6032d4a01336eb0cf4f43ce28709ac451e6", emu::chip8::Variant::CHIP_8, "Pancake Panic (Aaron Williams, 2018)", nullptr, nullptr},
    {"a804e02641ef61a720d4f0056eca4af0ea453fa3", emu::chip8::Variant::CHIP_8, "Drag Chip-8 Games Here Intro (Andreas Van Vooren, 2016)", nullptr, nullptr},
    {"a82ca5c53e1dcedfab4f65efef02229145771b7d", emu::chip8::Variant::CHIP_8, "Chip8 Picture", nullptr, nullptr},                   // Chip8 Picture.ch8
    {"a8d6e9b1976c99ddc0c4818828a6d3cb3ae6f348", emu::chip8::Variant::CHIP_8, "Wdl (JohnEarnest, 2022-10-10)", R"({"instructionsPerFrame": 15, "advanced": {"col1": "#808080", "col2": "#FF6600", "col3": "#662200", "col0": "#D3D3D3", "buzzColor": "#FF0000", "quietColor": "#000000", "screenRotation": 0, "fontStyle": "octo"}})", nullptr},
    {"a8ed3c25c00130838b3ee36cc82fbf32ce6cea83", emu::chip8::Variant::CHIP_8, "Hello World (Tim Franssen, 2020)", nullptr, nullptr},
    {"a902480e6e18c5287388b6797da36d640db9992b", emu::chip8::Variant::XO_CHIP, "Joust (Jason DuPertuis, 2017)", nullptr, nullptr},
    {"a98ed56f88f11156871d871d9200fc4bb45190a4", emu::chip8::Variant::XO_CHIP, "Super Octo Track Xo (TomRintjema, 2015-10-15)", R"({"instructionsPerFrame": 100, "optLoadStoreDontIncI": true, "advanced": {"col1": "#FF00FF", "col2": "#00FFFF", "col3": "#FFFFFF", "col0": "#000000", "buzzColor": "#990099", "quietColor": "#330033"}})", nullptr},
    //{"a9bf29597674c39b4e11d964b352b1e52c4ebb2f", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"a9bf29597674c39b4e11d964b352b1e52c4ebb2f", emu::chip8::Variant::SCHIPC, "Line Demo (unknown aauthor)", nullptr, nullptr},
    {"a9d3c975a5e733646a04f6e61deebcd0ad50f700", emu::chip8::Variant::CHIP_8, "Outlaw (JohnEarnest, 2014-07-17)", R"({"instructionsPerFrame": 15, "optWrapSprites": true, "advanced": {"col0": "#664400", "col1": "#AA4400", "buzzColor": "#FF7F50", "quietColor": "#000000"}})", nullptr},
    {"aa4f1a282bd64a2364102abf5737a4205365a2b4", emu::chip8::Variant::CHIP_8, "Space Flight", nullptr, nullptr},                    // Space Flight.ch8
    {"aae22735122c1e15df1dde4ef19e7b4968d88f6f", emu::chip8::Variant::XO_CHIP, "Nyan Cat 1 (Kouzerumatsu, 2022)", R"({"instructionsPerFrame": 100000})", nullptr},
    {"ab36ced6e34affacd57b2874ede3f95b669a424c", emu::chip8::Variant::XO_CHIP, "Jub8 Song 1 (your name here, 2016-08-31)", R"({"instructionsPerFrame": 1000, "advanced": {"col1": "#000000", "col2": "#FDFFD5", "col3": "#BA5A1A", "col0": "#353C41", "buzzColor": "#353C41", "quietColor": "#353C41", "screenRotation": 0}})", nullptr},
    {"ab5cbf267d74c168e174041b9594ae856cbd671d", emu::chip8::Variant::CHIP_8, "Chipwar (JohnEarnest, 2014-06-06)", R"({"instructionsPerFrame": 15, "advanced": {"col0": "#6699FF", "col1": "#000066", "buzzColor": "#FFAA00", "quietColor": "#000000"}})", nullptr},
    {"abfce04ddd0f72838dd887f3db3106066fd675b3", emu::chip8::Variant::SCHIP_1_1, "Squad (JohnEarnest, 2020-10-25)", R"({"instructionsPerFrame": 500, "advanced": {"col1": "#FFAF00", "col2": "#FD8100", "col3": "#FD8100", "col0": "#663300", "buzzColor": "#F9FFB3", "quietColor": "#000000", "screenRotation": 0, "fontStyle": "octo"}})", nullptr},
    {"ac30bca77d47b252c0764aff94fcac5202779f18", emu::chip8::Variant::SCHIPC_GCHIPC, "Spacefight 2091! fixed (Carsten Soerensen, 1992)", R"({"optJustShiftVx":true,"optDontResetVf":true,"optLoadStoreDontIncI":true,"optInstantDxyn":true,"optJump0Bxnn":true,"optLoresDxy0Is16x16":true,"optModeChangeClear":true,"optAllowHires":true,"instructionsPerFrame": 20})", nullptr},
    {"ac621d9fcada302ba6965768229ef130630bc525", emu::chip8::Variant::CHIP_8, "Astro Dodge (Revival Studios, 2008)", R"({"optLoadStoreDontIncI": true})", nullptr},
    {"ac7c8db7865beb22c9ec9001c9c0319e02f5d5c2", emu::chip8::Variant::CHIP_8, "Framed Mk1 (GV Samways, 1980)", nullptr, nullptr},
    {"acfd0d29a83882de19dc37a56ee6c7d63ac309c4", emu::chip8::Variant::CHIP_8, "Chip War (John Earnest, 2014)", nullptr, nullptr},
    {"ad612a1409c96cc24fc5fc1368fab71463480e9b", emu::chip8::Variant::CHIP_8, "Glitch Ghost (Jackie Kircher, 2014)", nullptr, nullptr},
    //{"adcfece2c527a68d8d74e6cfe7e84a8a04ad8182", {emu::Chip8EmulatorOptions::eXOCHIP}},
    {"adcfece2c527a68d8d74e6cfe7e84a8a04ad8182", emu::chip8::Variant::XO_CHIP, "Duel Of The F8S (Chromatophore, 2019)", nullptr, nullptr},
    {"ade839585ddeb0e3633177df03c1d91589e629eb", emu::chip8::Variant::CHIP_8, "Vers (JMN, 1991)", nullptr, nullptr},
    {"ae71a7b081a947f1760cdc147759803aea45e751", emu::chip8::Variant::CHIP_8, "Filter", nullptr, nullptr},                  // Filter.ch8
    {"af98ee11adae28a6153cae8e4c16afa00f861907", emu::chip8::Variant::CHIP_8_COSMAC_VIP, nullptr, nullptr, nullptr}, // Reveal Studios HIRES-CHIP-8 programs (actually self patching "Two page display CHIP-8", VIPER vol. 1, issue 3)
    {"afd9fee7565c54970b6bd7758aa8aa7843dd2e86", emu::chip8::Variant::XO_CHIP, "An Evening To Die For (JohnEarnest, 2019-10-22)", R"({"instructionsPerFrame": 500, "advanced": {"col1": "#000000", "col2": "#FF0000", "col3": "#FF0000", "col0": "#FFFFFF", "buzzColor": "#808080", "quietColor": "#000000", "screenRotation": 0}})", nullptr},
    {"b05dfd6bc0dca5106fb51ebc185406d633c96b44", emu::chip8::Variant::XO_CHIP, "Chip-8 Snake", nullptr, nullptr},                   // CHIP-8 Snake.xo8
    {"b0eec238f877ad6b17f2be33454353ab95584c79", emu::chip8::Variant::CHIP_8, "Flaps (Phillip Wagner, 2014)", nullptr, nullptr},
    {"b119651b5aa08557a85ca2ad5de3d1a86796b66b", emu::chip8::Variant::GENERIC_CHIP_8, "7-beep (Timendus, 2023-11-13)", nullptr,"@GH/Timendus/chip8-test-suite/v4.1/bin/7-beep.ch8"},
    {"b1917346eaae178c6f4e154e83cc89dc5b83c72f", emu::chip8::Variant::CHIP_8, "Floppy Bird (Micheal Wales, 2014)", nullptr, nullptr},
    {"b1bf08cccffc56320f3b98c96a7911a58c1475b0", emu::chip8::Variant::CHIP_8, "Fizzbuzz (Verisimilitudes, 2018)", nullptr, nullptr},
    {"b1ec426de267f4335a672243f7d93de5fd03b356", emu::chip8::Variant::XO_CHIP, "Invisibleman (MrEmerson, 2020)", nullptr, nullptr},
    {"b232ef880bd6060fb45fa6effed7edf0ae95670e", emu::chip8::Variant::CHIP_8, "Pong (Paul Vervalin, 1990)", nullptr, nullptr},
    {"b274ab30ed7678400dc2283431a45f7d98d9fced", emu::chip8::Variant::XO_CHIP, "Jub8 Song 5 (your name here, 2016-08-31)", R"({"instructionsPerFrame": 1000, "advanced": {"col1": "#000000", "col2": "#FDFFD5", "col3": "#BA5A1A", "col0": "#353C41", "buzzColor": "#353C41", "quietColor": "#353C41", "screenRotation": 0}})", nullptr},
    {"b277c053b5b4ff9e40cd52cd4125a35ec22ccd0a", emu::chip8::Variant::SCHIP_1_1, "Bulb (JohnEarnest, 2020-10-15)", R"({"instructionsPerFrame": 100, "advanced": {"col1": "#1E90FF", "col2": "#ABCC47", "col3": "#00131A", "col0": "#F9FFB3", "buzzColor": "#F9FFB3", "quietColor": "#000000", "screenRotation": 0, "fontStyle": "octo"}})", nullptr},
    {"b2abb5312f0ad28421c1190a65a73d98d4ebf401", emu::chip8::Variant::CHIP_8, "Pumpkin 'Dreess' Up (SystemLogoff, 2015-11-01)", R"({"instructionsPerFrame": 7, "advanced": {"col0": "#FFA500", "col1": "#111122", "buzzColor": "#FFFF00", "quietColor": "#222222"}})", nullptr},
    {"b2c55b6aba3e2910036d5b5bc3956cf7493e0221", emu::chip8::Variant::CHIP_8_COSMAC_VIP, nullptr, nullptr, nullptr}, // Reveal Studios HIRES-CHIP-8 programs (actually self patching "Two page display CHIP-8", VIPER vol. 1, issue 3)
    //{"b3dcfd85a76a678960359e1ce9f742a4f9c35ed8", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"b3dcfd85a76a678960359e1ce9f742a4f9c35ed8", emu::chip8::Variant::SCHIPC, "Knight (Simon Klit-Johnson, 2016)", nullptr, nullptr},
    {"b3fed4ed1eb0ed693c9731dbe53b29a76236c781", emu::chip8::Variant::CHIP_8, "Bowling (Gooitzen van der Wal)", nullptr, nullptr},
    {"b41cc0b5b2faabafd532d705b804abb3e8f97baf", emu::chip8::Variant::CHIP_8, "Deep8 (John Earnest, 2014)", nullptr, nullptr},
    {"b4be55185804a19d7d46c4b340531ecf1fc2abc5", emu::chip8::Variant::CHIP_8, "Chip-Chess (Thom Laurence, 2020)", nullptr, nullptr},
    {"b5b66c3b0b2a109bfb166fdc4d2d2a352c32da53", emu::chip8::Variant::CHIP_8, "Miniature Golf (R.G.Marchessault, 1980)", nullptr, nullptr},
    {"b5cc3bf3a5da556a33f2621be0f51c19e751292d", emu::chip8::Variant::CHIP_8, "Rocket-70 (Sly DC-2020)", nullptr, nullptr},
    {"b63bcd4e96a71717b84a6334cceffdf5f032e85e", emu::chip8::Variant::CHIP_8, "Kemono Friends Logo (Volgy, 2017)", nullptr, nullptr},
    {"b66f55f83eb264d2b73c0b4ac81ea5044bf73138", emu::chip8::Variant::CHIP_8, "Spock Paper Scissors (fontz, 2020-11-01)", R"({"instructionsPerFrame": 15, "optWrapSprites": true, "optInstantDxyn": true, "optDontResetVf": true, "advanced": {"col1": "#B4B4B4", "col2": "#FF6600", "col3": "#662200", "col0": "#323232", "buzzColor": "#FFAA00", "quietColor": "#000000", "screenRotation": 0, "fontStyle": "octo"}})", nullptr},
    //{"b693e60f161e69c98b0bb2bc1761cf434f8fbb0e", {emu::Chip8EmulatorOptions::eXOCHIP}},
    {"b693e60f161e69c98b0bb2bc1761cf434f8fbb0e", emu::chip8::Variant::XO_CHIP, "Into The Garlicscape (JohnEarnest, 2020-08-01)", R"({"instructionsPerFrame": 1000, "advanced": {"col1": "#E0FFFF", "col2": "#7FFFD4", "col3": "#7FFFD4", "col0": "#001000", "buzzColor": "#333333", "quietColor": "#000000", "screenRotation": 0, "fontStyle": "octo"}})", nullptr},
    //{"b76fbca2ec089c7e77f4a2f754db37854b99debc", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"b76fbca2ec089c7e77f4a2f754db37854b99debc", emu::chip8::Variant::SCHIPC, "Rockto (SupSuper, 2014-11-01)", R"({"instructionsPerFrame": 15, "advanced": {"col1": "#CCCCCC", "col0": "#333333", "buzzColor": "#FFFFFF", "quietColor": "#000000"}})", nullptr},
    {"b7cfb02cfb357ab5bf7fb3069f730fb4bf5df5f0", emu::chip8::Variant::CHIP_8, "Snoopy Cosmac Picture (works with FNC)", nullptr, nullptr},
    {"b8be39922f38d0160e257de75899119dc5137e6e", emu::chip8::Variant::CHIP_8, "Danm8Ku (buffis, 2015)", nullptr, nullptr},
    //{"b8be672909554abc17ed1ea0c694726f9a87b43d", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"b8be672909554abc17ed1ea0c694726f9a87b43d", emu::chip8::Variant::SCHIPC, "Hors (TomR, 2015)", nullptr, nullptr},
    {"b9272ae1acdaaa79ab649f6b48b72088ca2b1d74", emu::chip8::Variant::CHIP_8, "Maze (David Winter, 199x)", nullptr, nullptr},
    {"b92ffba5ccd708c0422d77b9af63ca4b2f67b443", emu::chip8::Variant::CHIP_8, "Brick Breaker (Kyle Saburao, 2019)", nullptr, nullptr},
    {"bb1e786cb921f51d0540cca9a216c0b72bceb443", emu::chip8::Variant::SCHIP_1_1, "Super Octogon (JohnEarnest, 2021-10-30)", R"({"instructionsPerFrame": 200, "advanced": {"col1": "#FF69B4", "col2": "#333333", "col3": "#FFFFFF", "col0": "#000000", "buzzColor": "#111111", "quietColor": "#000000", "screenRotation": 0, "fontStyle": "fish"}})", nullptr},
    {"bb5740042385cae10724b051208bb95e5341f56d", emu::chip8::Variant::CHIP_8, "Snek (JohnEarnest, 2021-10-01)", R"({"instructionsPerFrame": 1000, "optWrapSprites": true, "optInstantDxyn": true, "optDontResetVf": true, "advanced": {"col1": "#FFCC00", "col2": "#FF6600", "col3": "#662200", "col0": "#996600", "buzzColor": "#FFAA00", "quietColor": "#000000", "screenRotation": 0, "fontStyle": "octo"}})", nullptr},
    {"bc158d819890f16f105b8a316eeeefe4a0bad875", emu::chip8::Variant::CHIP_8, "X-Mirror", nullptr, nullptr},                        // X-Mirror.ch8
    //{"bc5faf54f04da3f4dbde50d3b31ccfc2bf8b9e06", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"bc5faf54f04da3f4dbde50d3b31ccfc2bf8b9e06", emu::chip8::Variant::SCHIPC, "Alien (Jonas Lindstedt, 199x)", nullptr, nullptr},
    {"bcb80940a8ed339a97917025d67d3217d8b89717", emu::chip8::Variant::CHIP_8, "Octo Lander (Private Butts, 2020)", nullptr, nullptr},
    {"bcbf36a68cf389e87dd54a9707cf35c4436dcb92", emu::chip8::Variant::SCHIP_1_1, "Applejak (JohnEarnest, 2020-10-07)", R"({"instructionsPerFrame": 100, "advanced": {"col1": "#B00000", "col2": "#FF6600", "col3": "#662200", "col0": "#001000", "buzzColor": "#FFAA00", "quietColor": "#000000", "screenRotation": 0}})", nullptr},
    {"bdb92475acfe11bc7814a2f5eade13fcd09b756a", emu::chip8::Variant::CHIP_8, "Ufo (Lutz V, 1992)", nullptr, nullptr},
    {"c05d1316bbb8acb1ba425c3ebdd0123632a73fd8", emu::chip8::Variant::CHIP_8, "Asphyxiation (Verisimilitudes, 2020)", nullptr, nullptr},
    {"c1b605040e29cce2a6fc52334fb09b0985340314", emu::chip8::Variant::SCHIP_1_1, nullptr, nullptr, nullptr},
    //{"c2a361700209116a300457eacbf33a8c40c01b83", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"c2a361700209116a300457eacbf33a8c40c01b83", emu::chip8::Variant::SCHIPC, "Super Astro Dodge (Revival Studios, 2008)", nullptr, nullptr},
    {"c314300d1630a479678167e4e786cce2c17831cd", emu::chip8::Variant::CHIP_8, "Starfield (Joel Yliluoma, 2015)", nullptr, nullptr},
    {"c32175db0c0508065709fc9cb42b233b24dad7fe", emu::chip8::Variant::CHIP_8, "2048 (Andrew James, 2021)", nullptr, nullptr},
    {"c33af07674dbbec5365bd91954c8bfed4a7467bd", emu::chip8::Variant::CHIP_8, "Ghost Escape! (TomR, 2016)", nullptr, nullptr},
    {"c5a2e40a381086e7d2064f9836c57224e27ec7ed", emu::chip8::Variant::CHIP_8_COSMAC_VIP, "Message Center  (Andrew-Modla)", nullptr, nullptr}, // CHIP-8 VIP (hybrid roms)
    {"c606d52970b86edcca4e87e9f6fae4b1ccbbbb0f", emu::chip8::Variant::XO_CHIP, "Chicken Scratch (JohnEarnest, 2020-10-28)", R"({"instructionsPerFrame": 500, "advanced": {"col1": "#7C4300", "col2": "#FD8100", "col3": "#FD8100", "col0": "#D5A08C", "buzzColor": "#3C2300", "quietColor": "#000000", "screenRotation": 0, "fontStyle": "octo"}})", nullptr},
    {"c617cd419bb3b51c2224b247782d73c46bc075c8", emu::chip8::Variant::CHIP_8, "Chip-Otto Logo 1 (Marco Varesio, 2015)", nullptr, nullptr},
    //{"c7c59b38129fdcec5bb0775a9a141b6ba936e706", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"c7c59b38129fdcec5bb0775a9a141b6ba936e706", emu::chip8::Variant::SCHIPC, "Sokoban (hap, 2006)", nullptr, nullptr},
    {"c8375d6a626ea21532cde178a7a0a22b7e511414", emu::chip8::Variant::SCHIP_1_1, nullptr, nullptr, nullptr},
    {"c8375d6a626ea21532cde178a7a0a22b7e511414", emu::chip8::Variant::SCHIPC, "Super Gem Catcher (Dakota Hernandez, 2018)", nullptr, nullptr},
    {"c8a3ccbdde2a289992077779cb02f1200cfed4bb", emu::chip8::Variant::CHIP_8, "Rule 30 (Verisimilitudes, 2029)", nullptr, nullptr},
    {"c8d2ebbc16551a4bee1f0e2b33f0510e4170afcf", emu::chip8::Variant::CHIP_8, "Connect 4 (David Winter)", nullptr, nullptr},
    //{"c9583967a7a2fd2b8b14fc4fe0568844b9b9408e", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"c9a13c00d8391f352488661fc3b15217f0e8d9fd", emu::chip8::Variant::CHIP_8, "Br8Kout (SharpenedSpoon, 2014)", nullptr, nullptr},
    {"c9eb637f750e7ca11e5ab1f30b7a69db475e5e23", emu::chip8::Variant::CHIP_8, "Warshaws Revenge (Ethan Pini, 2019)", nullptr, nullptr},
    {"cbbbc76a440b4020ecb9a6c95e95e636a8b23214", emu::chip8::Variant::CHIP_8, "Lainchain (Ashton Harding, 2018)", nullptr, nullptr},
    {"cc8db4b4ce858b0255ade64b1b8ea9d7c9d7d7fb", emu::chip8::Variant::SCHIP_1_1, nullptr, nullptr, nullptr},
    //{"ccec955da264cd92fdbb18c4971419497513ae42", {emu::Chip8EmulatorOptions::eXOCHIP}},
    {"ccec955da264cd92fdbb18c4971419497513ae42", emu::chip8::Variant::XO_CHIP, "Pizza Topping Panic! (Tom Rintjema, 2019)", nullptr, nullptr},
    {"cda3f758c2566c2067cfe6dd984747f6d24ca759", emu::chip8::Variant::CHIP_8, "Whack-A-Mole (unfinished game)", nullptr, nullptr},
    //{"ce15f2f4281b1069d33e00c801d5ed4390049a76", {emu::Chip8EmulatorOptions::eXOCHIP}},
    {"ce15f2f4281b1069d33e00c801d5ed4390049a76", emu::chip8::Variant::XO_CHIP, "Mabe Village From Link'S Awakening (sound)", nullptr, nullptr},
    {"ce33f148bfd5f1ca9edc68988b900a256905d057", emu::chip8::Variant::CHIP_8, "M8Ze", nullptr, nullptr},                    // M8ze.ch8
    {"ce7a5355d90c4aabe0d96c5add93f4efb21f099b", emu::chip8::Variant::CHIP_8, "Chipolarium", nullptr, nullptr},                     // Chipolarium.ch8
    //{"d03f27f85a1cf68465e0853cc0c4abee4a94a4e5", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"d03f27f85a1cf68465e0853cc0c4abee4a94a4e5", emu::chip8::Variant::SCHIPC, "Turnover '77 (your name here, 2014-08-31)", R"({"instructionsPerFrame": 200, "advanced": {"col1": "#FFe900", "col0": "#ED7f37", "buzzColor": "#FFAA00", "quietColor": "#000000"}})", nullptr},
    {"d11e76793c231cdce513c09f0511202ed076834d", emu::chip8::Variant::CHIP_8, "Space Racer (William Donnely, 2017)", nullptr, nullptr},
    {"d2b0a8cdab1d0bdb4186953abcd75c3a8d660033", emu::chip8::Variant::CHIP_8, "Enchantment (Verisimilitudes, 2020)", nullptr, nullptr},
    {"d2fa3927b31f81fc06cd9466123309c59264fa41", emu::chip8::Variant::CHIP_8, "Mastermind (William Donnelly, 2015)", nullptr, nullptr},
    {"d3554b9789728294d881823126ba6eb8103bd42c", emu::chip8::Variant::GENERIC_CHIP_8, "2-Ibm-Logo (Timendus, 2023-04-12)", nullptr, "@GH/Timendus/chip8-test-suite/v4.0/bin/2-ibm-logo.ch8"},
    //{"d40abc54374e4343639f993e897e00904ddf85d9", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"d40abc54374e4343639f993e897e00904ddf85d9", emu::chip8::Variant::CHIP_8, "Blinky (Hans Christian Egeberg, 1991)", R"({"optLoadStoreDontIncI": true, "optJustShiftVx": true})", nullptr},
    //{"d4339dac64038f30130af02fbe73b57cd7d481a1", {emu::Chip8EmulatorOptions::eXOCHIP}},
    {"d4339dac64038f30130af02fbe73b57cd7d481a1", emu::chip8::Variant::XO_CHIP, "Singing Voice (sound)", nullptr, nullptr},
    {"d52c2f85f56c963ff5e48a096afc61d3c8a71c11", emu::chip8::Variant::XO_CHIP, "Octo Party Mix! (Cratmang, 2020-10-29)", R"({"instructionsPerFrame": 1000, "advanced": {"col1": "#55ff55", "col2": "#ff7700", "col3": "#ffffff", "col0": "#000000", "buzzColor": "#000000", "quietColor": "#000000", "screenRotation": 0, "fontStyle": "octo"}})", nullptr},
    {"d54aaedefbf74f56b7446a5108885ddc33fb6fa1", emu::chip8::Variant::CHIP_8, "Death Star Vs Yoda (TodPunk, 2018)", nullptr, nullptr},
    {"d5ddd7d5071951c682cd4214474acbdd852234c4", emu::chip8::Variant::XO_CHIP, "Fest (Jacoboco, 2020)", nullptr, nullptr},
    {"d60314d126dd2aab429d2299dfc8740323adfae4", emu::chip8::Variant::SCHIP_1_1, nullptr, nullptr, nullptr},
    {"d666688a8fce468a7d88b536bc1ef5f35ba12031", emu::chip8::Variant::CHIP_8, "Wipe Off (Joseph Weisbecker, 19xx)", nullptr, nullptr},
    {"d68352857be1e44086aa0163eef8aad8251e0817", emu::chip8::Variant::SCHIP_1_1, nullptr, nullptr, nullptr},
    //{"d6cbd3af85b4c55b83c4e01f3a17c66fcebe9ccc", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"d6cbd3af85b4c55b83c4e01f3a17c66fcebe9ccc", emu::chip8::Variant::SCHIPC, "Single Dragon (David Nurser, 199x)", nullptr, nullptr},
    {"d6d8efef811350e7fba6197024c4973b360749b8", emu::chip8::Variant::CHIP_8, "Octojam 5 Title (JohnEarnest, 2018-09-24)", R"({"instructionsPerFrame": 7, "optWrapSprites": true, "advanced": {"col1": "#000080", "col2": "#FF6600", "col3": "#662200", "col0": "#FF69B4", "buzzColor": "#FFAA00", "quietColor": "#000000", "screenRotation": 0}})", nullptr},
    {"d73d48484a8fc60e8650f4228d6963a19a4de6c3", emu::chip8::Variant::CHIP_8, "Slippery Slope (John Earnest, 2018)", nullptr, nullptr},
    //{"d84494c47c5f63cf32fea555e8938c27941c2869", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"d84494c47c5f63cf32fea555e8938c27941c2869", emu::chip8::Variant::SCHIPC, "The Rude Street (SystemLogoff, 2019)", nullptr, nullptr},
    {"d867b0d0fe1e96ba60910c64d9362da5a986774e", emu::chip8::Variant::CHIP_8, "Area F (Nakatsugawa, 2015)", nullptr, nullptr},
    {"d92c71b955b7634370571bd707715cf8bb0e2fb4", emu::chip8::Variant::CHIP_8, "Chip8 Emulator Logo (Garstyciuks)", nullptr, nullptr},
    //{"d9389d564baced03192503a58ad930110bb0fe03", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"d9389d564baced03192503a58ad930110bb0fe03", emu::chip8::Variant::SCHIPC, "Hex Mixt (unknown author)", nullptr, nullptr},
    {"d979858bb9ffd07b48f52f92a8bcac0199f3623e", emu::chip8::Variant::CHIP_8, "Merlin (David Winter)", nullptr, nullptr},
    {"d97a7e1d952ed70d00715d92291ef08fc9a4c909", emu::chip8::Variant::CHIP_8, "Acey Deucy (Phil Baltzer)", nullptr, nullptr},
    {"da710f631f8e35534d0b9170bcf892a60f49c43d", emu::chip8::Variant::CHIP_8, "Vertical Brix (Paul Robson, 1996)", nullptr, nullptr},
    {"dbb52193db4063149c3d8768ab47dd740d90955c", emu::chip8::Variant::CHIP_8, "Hi-Lo (Jef Winsor, 1978)", nullptr, nullptr},
    //{"dbb5b085117d513f1ce403959d7136b767bb3dd3", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"dbb5b085117d513f1ce403959d7136b767bb3dd3", emu::chip8::Variant::SCHIPC, "Chipmark'77 (John Deeny, 2016)", nullptr, nullptr},
    {"dc5a12fa3ad88ea6c42dff1720be14f6772aef59", emu::chip8::Variant::CHIP_8, "Move Figure (John Earnest-20xx)", nullptr, nullptr},
    {"dcf6852e937aecedbe16bc93009624ef8590bce3", emu::chip8::Variant::CHIP_8, "Screenwipe (Andrew James, 2021)", nullptr, nullptr},
    //{"dd6ef80cadef1e7b42f71ad99573b1af2299e27d", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"dd6ef80cadef1e7b42f71ad99573b1af2299e27d", emu::chip8::Variant::SCHIPC, "Robot (unknown author)", nullptr, nullptr},
    {"de259351c65f790af035a4607a508c366bf4eaf2", emu::chip8::Variant::CHIP_8, "Nonogram (Verisimilitudes, 2021)", nullptr, nullptr},
    {"dea204fbfda4ed63fe2a2be255617bb9ee770a61", emu::chip8::Variant::CHIP_8, "3D Vip'R Maze (Tim Franssen, 2021)", nullptr, nullptr},
    {"debfea355f5737be394d3e62d4970e733ec3b6fc", emu::chip8::Variant::SCHIP_1_1, nullptr, nullptr, nullptr},
    {"df5ced9c20d00bf7be7d3361d76f27d0d577abfb", emu::chip8::Variant::SCHIPC, "Horsey Jump (LarissaR, 2015-10-30)", R"({"instructionsPerFrame": 20, "advanced": {"col1": "#C3C3C3", "col2": "#FF6600", "col3": "#662200", "col0": "#006C00", "buzzColor": "#FFAA00", "quietColor": "#000000"}})", nullptr},
    {"e0596d264ead3c71cf76b352f71959c82c748519", emu::chip8::Variant::GENERIC_CHIP_8, "4-Flags (Timendus, 2023-11-13)", nullptr, "@GH/Timendus/chip8-test-suite/v4.1/bin/4-flags.ch8"},
    //{"e14350d3b19443e5ad2848172bef9719a8680b01", {emu::Chip8EmulatorOptions::eXOCHIP}},
    {"e14350d3b19443e5ad2848172bef9719a8680b01", emu::chip8::Variant::XO_CHIP, "Red October V (Kesha, 2018-10-31)", R"({"instructionsPerFrame": 10000, "advanced": {"col1": "#000000", "col2": "#ffff00", "col3": "#808000", "col0": "#b22d10", "buzzColor": "#400000", "quietColor": "#400000", "screenRotation": 0}})", nullptr},
    {"e2005db6391f589534dd2d63a95b429338bd667c", emu::chip8::Variant::CHIP_8, "Rocket Launcher", nullptr, nullptr},                 // Rocket Launcher.ch8
    {"e234c3781f9b65d2c1940976d10bf06ce7742b8d", emu::chip8::Variant::SCHIP_1_1, nullptr, nullptr, nullptr},
    //{"e251b6132b15d411a9fe5d1e91a6579e3e057527", {emu::Chip8EmulatorOptions::eXOCHIP}},
    {"e251b6132b15d411a9fe5d1e91a6579e3e057527", emu::chip8::Variant::XO_CHIP, "I'Ll Be Back (sound)", nullptr, nullptr},
    //{"e2cf46c544bee2ef8a8b21dba1c583d5121b1b96", {emu::Chip8EmulatorOptions::eXOCHIP}},
    {"e2cf46c544bee2ef8a8b21dba1c583d5121b1b96", emu::chip8::Variant::XO_CHIP, "Octo Crawl (taqueso, 2016)", nullptr, nullptr},
    //{"e2d86d6c70877e99ed4253c9a83d4da42e5a14ee", {emu::Chip8EmulatorOptions::eXOCHIP}},
    {"e2d86d6c70877e99ed4253c9a83d4da42e5a14ee", emu::chip8::Variant::XO_CHIP, "Bustin (Tom Rintjema, 2019)", nullptr, nullptr},
    {"e46f333cd95785b554f8dda5690e165959dfb4be", emu::chip8::Variant::SCHIP_1_1, nullptr, nullptr, nullptr},
    //{"e4ef6fff9813c43bd7ad2ecaf02d1a3135d68418", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"e4ef6fff9813c43bd7ad2ecaf02d1a3135d68418", emu::chip8::Variant::SCHIPC, "Magic Square (David Winter, 199x)", nullptr, nullptr},
    {"e5564c1662d3f144507782784ae4e2f79eaf66d7", emu::chip8::Variant::SCHIPC, "Pich8-Logo (Philw07, 2020)", nullptr, nullptr},
    {"e55f36b9ecd6fbbeb626a78f222011bddd5e5197", emu::chip8::Variant::CHIP_8, "Love8 Intro (Athir Saleem, 2019)", nullptr, nullptr},
    {"e60257f0718aa6aab249667bd90af598d21b97bc", emu::chip8::Variant::CHIP_8, "Vip Demo - King Kong (unknown author)", nullptr, nullptr},
    {"e62d9d26d2d1b65e6aafb8331cae333fbadcaebf", emu::chip8::Variant::MEGA_CHIP, nullptr, nullptr, nullptr},
    {"e670ac22abbfe46a3bcf98e36ac5a34074c43693", emu::chip8::Variant::GENERIC_CHIP_8, "2-Ibm-Logo (Timendus, 2023-11-13)", nullptr, "@GH/Timendus/chip8-test-suite/v4.1/bin/2-ibm-logo.ch8"},
    //{"e6a027d00c524ab7ae00b720f64a06ad1137836c", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"e6a027d00c524ab7ae00b720f64a06ad1137836c", emu::chip8::Variant::CHIP_8, "Letter Scroll (Michael Wales, 2014)", nullptr, nullptr},
    //{"e6af47843f0ecc3302027a3756dd7b389a15e437", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"e6af47843f0ecc3302027a3756dd7b389a15e437", emu::chip8::Variant::SCHIPC, "Black Rainbow (JohnEarnest, 2016-11-01)", R"({"instructionsPerFrame": 20, "optWrapSprites": true, "advanced": {"col1": "#D3D3D3", "col2": "#FF6600", "col3": "#662200", "col0": "#808080", "buzzColor": "#FFFFFF", "quietColor": "#000000", "screenRotation": 0}})", nullptr},
    //{"e6d4a8598999b3d95047babf67b529d83eaa9554", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"e6d4a8598999b3d95047babf67b529d83eaa9554", emu::chip8::Variant::SCHIPC, "Car Race Demo (Erik Bryntse, 1991)", nullptr, nullptr},
    //{"e6d910b7c9f9680df462662ce16336ebcb0eab1e", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"e6d910b7c9f9680df462662ce16336ebcb0eab1e", emu::chip8::Variant::SCHIPC, "Super Maze (David Winter, 199x)", nullptr, nullptr},
    {"e74f20f234753e0cc2f58e29dc02d6128a6a3d97", emu::chip8::Variant::SCHIPC, "The Binding Of Cosmac (buffi, 2016-10-21)", R"({"instructionsPerFrame": 1000, "advanced": {"col1": "#FFCC00", "col2": "#FF6600", "col3": "#662200", "col0": "#996600", "buzzColor": "#FFAA00", "quietColor": "#000000", "screenRotation": 0}})", nullptr},
    {"e78144bb9bdf7b48b096e1cdd0f4db430bfd731e", emu::chip8::Variant::CHIP_8, "Shooth3Rd (Beholder, 2016)", nullptr, nullptr},
    {"e8477fad78863714c508c046d2419248c5f89690", emu::chip8::Variant::SCHIP_1_1, nullptr, nullptr, nullptr},
    {"e85ade7412e8affc4a8590fc0c928f1f00c5eb6b", emu::chip8::Variant::XO_CHIP, "Angle Of Death (Chromatophore, 2020)", nullptr, nullptr},
    {"e9488cc1878ba9c0042d576cc1b3679ea1632098", emu::chip8::Variant::SCHIP_1_1, nullptr, nullptr, nullptr},
    {"e99657c8a3bfbfb5a9cb70e7d330346802ce20ce", emu::chip8::Variant::CHIP_8, "Asphyxiation Advanced (Verisimilitudes, 2020)", nullptr, nullptr},
    {"e9ce37041ac752ef910bb5c47ee9a031403223be", emu::chip8::Variant::CHIP_8, "Animal Race (Brian_Astle, fixed)", nullptr, nullptr},
    {"ea4ec4c07c97e1ad77eb9bfe237d2a1578795fbf", emu::chip8::Variant::CHIP_8, "Seconds Counter (Michael Wales, 2014)", nullptr, nullptr},
    {"ea6fc1ff6e57800e2322641f6f02ebd462dda2b8", emu::chip8::Variant::CHIP_8, "2048 (Dr Gergo Erdi, 2014)", nullptr, nullptr},
    {"ea7c12f458932527802fdd4a18e4c6700dd91138", emu::chip8::Variant::CHIP_8, "Game 16 (TCNJ S.572.3)", nullptr, nullptr},
    {"ea9af3c09b0d9e265fcd92bcc5d51a2939fdf27a", emu::chip8::Variant::CHIP_8, "15 Puzzle (Roger Ivie, 19xx)", nullptr, nullptr},
    {"eb0076f3dd33b16fd040640b4b67bab19e491bef", emu::chip8::Variant::CHIP_8, "Fl8Ppy Mouse (buffis, 2016)", nullptr, nullptr},
    //{"eb1a09cc11c73938f39ce8d52c8e06576dec3a32", {emu::Chip8EmulatorOptions::eXOCHIP}},
    {"eb1a09cc11c73938f39ce8d52c8e06576dec3a32", emu::chip8::Variant::XO_CHIP, "Octofonts (John Deeny, 2016)", nullptr, nullptr},
    {"eb412becb086d3cbccce4e3e370b9149b969cff9", emu::chip8::Variant::CHIP_8, "Flashing 1 Demo (Yahia Farghaly, 2019)", nullptr, nullptr},
    {"eb548f0a0ceca4da0475112ab14e223a63350c89", emu::chip8::Variant::CHIP_8, "C-Zero (Ethan Pini, 2019)", nullptr, nullptr},
    {"eb72a25bd58e122e65a540807e7a1816abaa4f41", emu::chip8::Variant::CHIP_8, "Framed Mk2 (GV Samways, 1980)", nullptr, nullptr},
    {"eba3b6ac5539452d1dd0c7c045d69e6096c457dd", emu::chip8::Variant::SCHIP_1_1, nullptr, nullptr, nullptr},
    //{"ebada8eb97ce40a91554386696f7daa33023cc8c", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"ebada8eb97ce40a91554386696f7daa33023cc8c", emu::chip8::Variant::SCHIPC, "Chip-84 Scratch (Christian Kosman, 2018)", nullptr, nullptr},
    {"ec00e355117ed6385b51c0819f85954c3b765ed0", emu::chip8::Variant::CHIP_8, "Ball Breaker (Verisimilitudes, 2020)", nullptr, nullptr},
    {"ec1824ccdcc6c3b4390004946e551d4aa2058820", emu::chip8::Variant::CHIP_8, "Sierpchaos (Marco Varesio, 2015)", nullptr, nullptr},
    {"ecfe1354f04a8bc60adb84637c97c7d8b6809097", emu::chip8::Variant::XO_CHIP, "Sand", nullptr, nullptr},                   // Sand.xo8
    {"ed829190e37815771e7a8c675ba0074996a2ddb0", emu::chip8::Variant::CHIP_8, "Space Intercept (Joseph Weisbecker, 1978)", nullptr, nullptr},
    {"ed96881bf0d1e97157b04d8a4632f911067fe9e6", emu::chip8::Variant::CHIP_8, "Loose Cannon (fix)", nullptr, nullptr},
    {"ed9a9510aba2227ca9bb2d521adcaa903f433450", emu::chip8::Variant::CHIP_8, "Locked In A Room With A Ghost (TomR, 2015)", nullptr, nullptr},
    {"ee7fb407da5f17ea7be9d16ce8a7ff38028ca924", emu::chip8::Variant::CHIP_8, "8Min (TomR, 2020)", nullptr, nullptr},
    {"ef0e9afdf11fb8f71b807b93ff1c25ec5f13adf9", emu::chip8::Variant::CHIP_8X_TPD, "CHIP-8x Two Page Display Test", nullptr, nullptr},
    {"ef54d110d2ac9d4a172f523b011b05fe4caece5b", emu::chip8::Variant::CHIP_8, "Snoopy Picture (Marco Varesio, 2015)", nullptr, nullptr},
    {"efa6bc8f1f35baaa16700d68a83dc4919797e2fe", emu::chip8::Variant::CHIP_8, "Life (GV Samways)", nullptr, nullptr},
    {"f0b6e192b9589cc9ee9bc89bacdab00be6ac360d", emu::chip8::Variant::CHIP_8, "Advanced Warfare (Ethan Pini, 2019)", nullptr, nullptr},
    //{"f11793f86baae9f5f0c77e5d7aa216c2180c3d07", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"f11793f86baae9f5f0c77e5d7aa216c2180c3d07", emu::chip8::Variant::SCHIPC, "Super Particle Demo (zeroZshadow, 2008)", nullptr, nullptr},
    //{"f12038dcd28ca71661162bfb6fc92a8826f7d6b9", {emu::Chip8EmulatorOptions::eXOCHIP}},
    {"f12038dcd28ca71661162bfb6fc92a8826f7d6b9", emu::chip8::Variant::XO_CHIP, "Sk8 H8 1988 (Team Out of Left Field, 2015)", nullptr, nullptr},
    {"f13766c14aeb02ad8d4d103cb5eadd282d20cddc", emu::chip8::Variant::CHIP_8, "Brix (Andreas Gustafsson, 1990)", nullptr, nullptr},
    {"f199e23cbe29bb36f43373818b10bf72b35e9d05", emu::chip8::Variant::CHIP_8, "Bongocat (Andrew James, 2021)", nullptr, nullptr},
    {"f1e036fb93b482b1ddfcb2bc1a4de43c8cf51def", emu::chip8::Variant::CHIP_8, "Random Number Test (Matthew Mikolay, 2010)", nullptr, nullptr},
    {"f23ee6f22c3ada8c638096ec1209a65dd036cc52", emu::chip8::Variant::CHIP_8, "Black Lives Matter Demo (Ben Smith, 2020)", nullptr, nullptr},
    {"f26993a4afd5cda2fea19935773fd3db54866623", emu::chip8::Variant::CHIP_8, "Octojam 1 Title (JohnEarnest, 2014-09-29)", R"({"instructionsPerFrame": 7, "advanced": {"col1": "#FFAA00", "col0": "#AA4400", "buzzColor": "#FFAA00", "quietColor": "#000000"}})", nullptr},
    {"f274bf62145ba9f7740aab9d83e5b15db8047a1d", emu::chip8::Variant::CHIP_8, "Thom8S Was Alone (jusion, 2014)", nullptr, nullptr},
    {"f27d2375671fba01f87045f1f1fb67bcc3b284ee", emu::chip8::Variant::CHIP_8, "Magic Sprite (Alex Osipchuck, 2017)", nullptr, nullptr},
    {"f2e9c480af31a4039af02dd7a2b8d5d1f859704d", emu::chip8::Variant::CHIP_8, "Zeropong (zeroZshadow, 2007)", nullptr, nullptr},
    //{"f31a8912ffb8a2920eb7ad5d645aa65a413b6ae9", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"f31a8912ffb8a2920eb7ad5d645aa65a413b6ae9", emu::chip8::Variant::SCHIPC, "Laser (unknown author)", nullptr, nullptr},
    {"f3f2d91e4cb8886b5af2f556827574bf1fd8967c", emu::chip8::Variant::SCHIP_1_1, nullptr, nullptr, nullptr},
    {"f4392681b1fa38d7ad0a7d7a59cecf247ac1457a", emu::chip8::Variant::CHIP_8, "Chipquarium (mattmik, 2016-10-31)", R"({"instructionsPerFrame": 15, "advanced": {"col1": "#FFFFFF", "col2": "#FF6600", "col3": "#662200", "col0": "#0072ff", "buzzColor": "#FFAA00", "quietColor": "#0000FF", "screenRotation": 0}})", nullptr},
    //{"f4e50d6e209324906b7899ed785a0d849a397abc", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"f4e50d6e209324906b7899ed785a0d849a397abc", emu::chip8::Variant::SCHIPC, "Alien Hunter (Hans, 2015)", nullptr, nullptr},
    //{"f505bdc0b1f2da3cc4a69e3baaba8c3bf5303692", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"f505bdc0b1f2da3cc4a69e3baaba8c3bf5303692", emu::chip8::Variant::SCHIPC, "Super Etch-A-Sketch (KrzysztofJeszke, 2020)", nullptr, nullptr},
    //{"f55ab7c3776fd9a94ffac82f0feb965e93c057f1", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"f55ab7c3776fd9a94ffac82f0feb965e93c057f1", emu::chip8::Variant::SCHIPC, "Chip-84 3D Title (Christian Kosman, 2018)", nullptr, nullptr},
    //{"f56134c8196fdff347264a985add4d2648bac76a", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"f56134c8196fdff347264a985add4d2648bac76a", emu::chip8::Variant::SCHIPC, "Codegrid (Xikka, 2015)", nullptr, nullptr},
    //{"f5c666c33ed66a9662cef78b1ef62f80a33b0358", {emu::Chip8EmulatorOptions::eXOCHIP}},
    {"f5c666c33ed66a9662cef78b1ef62f80a33b0358", emu::chip8::Variant::XO_CHIP, "Ghostbusters! (sound)", nullptr, nullptr},
    {"f60bdb428e747b0a379063d7cc96d099ab2db18d", emu::chip8::Variant::CHIP_8, "Falling Stars (A-KouZ1, 2016)", nullptr, nullptr},
    //{"f64e87b8a4161806b4dad9bfc317d4341b410beb", {emu::Chip8EmulatorOptions::eXOCHIP}},
    {"f64e87b8a4161806b4dad9bfc317d4341b410beb", emu::chip8::Variant::XO_CHIP, "Heh, I Chipped Your Mom'S Eight Last Night (sound)", nullptr, nullptr},
    {"f6ee978a1dfded9262f08dc95bfb3071c5767e78", emu::chip8::Variant::CHIP_8, "Prads Demo (Pradipna Nepal, 2010)", nullptr, nullptr},
    {"f7510be8f3299f8e350626ef5cb88041a5c95f3c", emu::chip8::Variant::CHIP_8, "Falling (Verisimilitudes, 2019)", nullptr, nullptr},
    {"f7a3e2e3272b03631efa561976f981bac351a603", emu::chip8::Variant::SCHIP_1_1, nullptr, nullptr, nullptr},
    //{"f8008875a4b35dc7188eeca2a05535116371eaf0", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"f8008875a4b35dc7188eeca2a05535116371eaf0", emu::chip8::Variant::SCHIPC, "Superworm V3 (RB, 1992)", nullptr, nullptr},
    {"f84ad99d0095ef1281b55c779783b99cb53d2ade", emu::chip8::Variant::CHIP_8, "Bullet Patterns (buffi, 2015)", nullptr, nullptr},
    {"f9d0bdf4a80d5570a9af9fd13769e528dff411df", emu::chip8::Variant::XO_CHIP, "Ordinary Idle Garden (Cratmang, 2021-02-05)", R"({"instructionsPerFrame": 200, "advanced": {"col1": "#43523d", "col2": "#43523d", "col3": "#43523d", "col0": "#c7f0d8", "buzzColor": "#43523d", "quietColor": "#43523d", "screenRotation": 0, "fontStyle": "fish"}})", nullptr},
    {"fa1b7ad92e0dd498a1c0b1d9bfc7296f3b96fca8", emu::chip8::Variant::CHIP_8, "The Maze (Ian Schert, 2020)", nullptr, nullptr},
    {"fa7c04f68d78e0faf6d136a3babe3943fc2e02f1", emu::chip8::Variant::CHIP_8, "Most Dangerous Game (Peter Maruhnic)", nullptr, nullptr},
    {"fb48e162c7f2e8853909acc5534b55fb55030f9f", emu::chip8::Variant::CHIP_8, "Masquer8 (Chromatophore, 2015-10-30)", R"({"instructionsPerFrame": 15, "advanced": {"col1": "#FF6666", "col2": "#FF6600", "col3": "#662200", "col0": "#1a3674", "buzzColor": "#3b6c83", "quietColor": "#000000"}})", nullptr},
    //{"fb6a79a1f42cd4539c3da2783d4f7f035d9b3a2c", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"fb6a79a1f42cd4539c3da2783d4f7f035d9b3a2c", emu::chip8::Variant::CHIP_8, "Rule 30 Improved (Verisimilitudes, 2019)", nullptr, nullptr},
    //{"fb8d0807a00353ae8071238a2eb7f1e555afe525", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"fb8d0807a00353ae8071238a2eb7f1e555afe525", emu::chip8::Variant::CHIP_8, "Labview Splash Screen (Richard James Lewis, 2019)", nullptr, nullptr},
    //{"fbc7711ad068015b957e91d8714636b2ac90d9cb", {emu::Chip8EmulatorOptions::eXOCHIP}},
    {"fbc7711ad068015b957e91d8714636b2ac90d9cb", emu::chip8::Variant::XO_CHIP, "Kesha Was Bird (Kesha, 2016-01-21)", R"({"instructionsPerFrame": 1000, "advanced": {"col1": "#B8CD9E", "col2": "#ED656B", "col3": "#55989E", "col0": "#59755E", "buzzColor": "#000", "quietColor": "#000000"}})", nullptr},
    {"fc724ae0125f5f1ac94a79fe3afc6318b1f57556", emu::chip8::Variant::CHIP_8, "Kaleidoscope (Joseph Weisbecker, 1978)", nullptr, nullptr},
    {"fca71182a8838b686573e69b22aff945d79fe1d0", emu::chip8::Variant::CHIP_8, "Airplane", nullptr, nullptr},                        // Airplane.ch8
    //{"fcaa793332a83c93f4ed79f5ffbc8403c8b8aea0", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"fcaa793332a83c93f4ed79f5ffbc8403c8b8aea0", emu::chip8::Variant::SCHIPC, "Eaty The Alien (JohnEarnest, 2015-10-31)", R"({"instructionsPerFrame": 200, "advanced": {"col1": "#443300", "col2": "#FF6600", "col3": "#662200", "col0": "#aa7700", "buzzColor": "#FFAA00", "quietColor": "#000000"}})", nullptr},
    {"fcecf90496dadd214486a7a769e3a07f2b8f4eab", emu::chip8::Variant::SCHIPC, "Knight (simonpacis, 2016-10-07)", R"({"instructionsPerFrame": 200, "optWrapSprites": true, "advanced": {"col1": "#FFCC00", "col2": "#FF6600", "col3": "#662200", "col0": "#996600", "buzzColor": "#FFAA00", "quietColor": "#000000", "screenRotation": 0}})", nullptr},
    {"fdb2da9e06a07bb11dee8a7dc1a9589759a9c57d", emu::chip8::Variant::XO_CHIP, "Skipper (Jason DuPertuis, 2020)", nullptr, nullptr},
    {"fe25659856e1921ea629d3f8fce977c0cae57ff3", emu::chip8::Variant::CHIP_8, "Snake (Henry Wang, 2019)", nullptr, nullptr},
    {"feaa2b999737630a6402e990df4d0558f79ba43e", emu::chip8::Variant::CHIP_8, "Addition Problems (Paul C. Moews, 1979)", nullptr, nullptr},
    {"fed518f92023db76cd9fb4616c44c7be1cede2d2", emu::chip8::Variant::CHIP_8, "2048Game (A-Kouz1, 2017)", nullptr, nullptr},
    //{"ff5276bfd203634ef3034475ff7bc8bd9033a03d", {emu::Chip8EmulatorOptions::eSCHIP11}},
    {"ff5276bfd203634ef3034475ff7bc8bd9033a03d", emu::chip8::Variant::SCHIPC, "Bounce (Les Harris, 20xx)", nullptr, nullptr},
    {"ff639eceaf221ae66151a03779b41fae7118d2d8", emu::chip8::Variant::CHIP_8, "Reversi (Philip Baltzer)", nullptr, nullptr},
    {"ff6b8ac59bf281cd4b5ab6e161600b00f85a0265", emu::chip8::Variant::CHIP_8, "Danm8Ku (buffi, 2015-10-31)", R"({"instructionsPerFrame": 1000, "advanced": {"col1": "#00FF00", "col2": "#FF0000", "col3": "#FFFF00", "col0": "#000000", "buzzColor": "#999900", "quietColor": "#333300"}})", nullptr},
};

static constexpr int g_knownRomNum = sizeof(g_knownRoms) / sizeof(g_knownRoms[0]);

std::string Librarian::Info::minimumOpcodeProfile() const
{
    auto mask = static_cast<uint64_t>(possibleVariants);
    if(mask) {
        auto cv = static_cast<emu::Chip8Variant>(mask & -mask);
        return emu::Chip8Decompiler::chipVariantName(cv).second;
    }
    return "unknown";
}

emu::Chip8EmulatorOptions::SupportedPreset Librarian::Info::minimumOpcodePreset() const
{
    auto mask = static_cast<uint64_t>(possibleVariants);
    if(mask) {
        auto cv = (mask & -mask);
        int cnt = 0;
        while((cv & 1) == 0) {
            cv >>= 1;
            cnt++;
        }
        return emu::Chip8EmulatorOptions::presetForVariant(static_cast<emu::chip8::Variant>(cv + 1));
    }
    return emu::Chip8EmulatorOptions::eCHIP8;
}

Librarian::Librarian(const CadmiumConfiguration& cfg)
: _cfg(cfg)
{
    static bool once = false;
    if(!once) {
        once = true;
        TraceLog(LOG_INFO, "Internal database contains `%d` different program checksums.", g_knownRomNum);
    }
}

std::string Librarian::fullPath(std::string file) const
{
    return (fs::path(_currentPath) / file).string();
}

bool Librarian::fetchDir(std::string directory)
{
    std::error_code ec;
    _currentPath = fs::canonical(directory, ec).string();
    _directoryEntries.clear();
    _activeEntry = -1;
    _analyzing = true;
    try {
        _directoryEntries.push_back({"..", Info::eDIRECTORY, emu::Chip8EmulatorOptions::eCHIP8, 0, {}});
        for(auto& de : fs::directory_iterator(directory)) {
            if(de.is_directory()) {
                _directoryEntries.push_back({de.path().filename().string(), Info::eDIRECTORY, emu::Chip8EmulatorOptions::eCHIP8, 0, convertClock(de.last_write_time())});
            }
            else if(de.is_regular_file()) {
                auto ext = de.path().extension();
                auto type = Info::eUNKNOWN_FILE;
                auto variant = emu::Chip8EmulatorOptions::eCHIP8;
                if(ext == ".8o") type = Info::eOCTO_SOURCE;
                else if(ext == ".ch8")
                    type = Info::eROM_FILE;
                else if(ext == ".ch10")
                    type = Info::eROM_FILE, variant = emu::Chip8EmulatorOptions::eCHIP10;
                else if(ext == ".hc8")
                    type = Info::eROM_FILE, variant = emu::Chip8EmulatorOptions::eCHIP8VIP;
                else if(ext == ".c8h")
                    type = Info::eROM_FILE, variant = emu::Chip8EmulatorOptions::eCHIP8VIP_TPD;
                else if(ext == ".c8x")
                    type = Info::eROM_FILE, variant = emu::Chip8EmulatorOptions::eCHIP8XVIP;
                else if(ext == ".sc8")
                    type = Info::eROM_FILE, variant = emu::Chip8EmulatorOptions::eSCHIP11;
                else if(ext == ".mc8")
                    type = Info::eROM_FILE, variant = emu::Chip8EmulatorOptions::eMEGACHIP;
                else if(ext == ".xo8")
                    type = Info::eROM_FILE, variant = emu::Chip8EmulatorOptions::eXOCHIP;
                else if(ext == ".c8b")
                    type = Info::eROM_FILE;
                _directoryEntries.push_back({de.path().filename().string(), type, variant, (size_t)de.file_size(), convertClock(de.last_write_time())});
            }
        }
        std::sort(_directoryEntries.begin(), _directoryEntries.end(), [](const Info& a, const Info& b){
           if(a.type == Info::eDIRECTORY && b.type != Info::eDIRECTORY) {
               return true;
           }
           else if(a.type != Info::eDIRECTORY && b.type == Info::eDIRECTORY) {
               return false;
           }
           return a.filePath < b.filePath;
        });
    }
    catch(fs::filesystem_error& fe)
    {
        return false;
    }
    return true;
}

bool Librarian::intoDir(std::string subDirectory)
{
    return fetchDir((fs::path(_currentPath) / subDirectory).string());
}

bool Librarian::parentDir()
{
    return fetchDir(fs::path(_currentPath).parent_path().string());
}

bool Librarian::update(const emu::Chip8EmulatorOptions& options)
{
    bool foundOne = false;
    if(_analyzing) {
        for (auto& entry : _directoryEntries) {
            if (!entry.analyzed) {
                foundOne = true;
                if(entry.type == Info::eROM_FILE && entry.fileSize < 1024 * 1024 * 8) {
                    if (entry.variant == emu::Chip8EmulatorOptions::eCHIP8) {
                        auto file = loadFile((fs::path(_currentPath) / entry.filePath).string());
                        entry.isKnown = isKnownFile(file.data(), file.size());
                        entry.sha1sum = calculateSha1Hex(file.data(), file.size());
                        if(entry.isKnown) {
                            entry.variant = getPresetForFile(entry.sha1sum);
                        }
                        else {
                            emu::Chip8Decompiler dec;
                            uint16_t startAddress = endsWith(entry.filePath, ".c8x") ? 0x300 : 0x200;
                            dec.decompile(entry.filePath, file.data(), startAddress, file.size(), startAddress, nullptr, true, true);
                            entry.possibleVariants = dec.possibleVariants();
                            if ((uint64_t)dec.possibleVariants()) {
                                if (dec.supportsVariant(options.presetAsVariant()))
                                    entry.variant = options.behaviorBase;
                                else if (dec.supportsVariant(emu::Chip8Variant::XO_CHIP))
                                    entry.variant = emu::Chip8EmulatorOptions::eXOCHIP;
                                else if (dec.supportsVariant(emu::Chip8Variant::MEGA_CHIP))
                                    entry.variant = emu::Chip8EmulatorOptions::eMEGACHIP;
                                else if (dec.supportsVariant(emu::Chip8Variant::SCHIP_1_1))
                                    entry.variant = emu::Chip8EmulatorOptions::eSCHIP11;
                                else if (dec.supportsVariant(emu::Chip8Variant::SCHIP_1_0))
                                    entry.variant = emu::Chip8EmulatorOptions::eSCHIP10;
                                else if (dec.supportsVariant(emu::Chip8Variant::CHIP_48))
                                    entry.variant = emu::Chip8EmulatorOptions::eCHIP48;
                                else if (dec.supportsVariant(emu::Chip8Variant::CHIP_10))
                                    entry.variant = emu::Chip8EmulatorOptions::eSCHIP10;
                                else
                                    entry.variant = emu::Chip8EmulatorOptions::eCHIP8;
                            }
                            else {
                                entry.type = Info::eUNKNOWN_FILE;
                            }
                            TraceLog(LOG_DEBUG, "analyzed `%s`: %s", entry.filePath.c_str(), emu::Chip8EmulatorOptions::nameOfPreset(entry.variant).c_str());
                        }
                    }
                    else {
                        auto file = loadFile((fs::path(_currentPath) / entry.filePath).string());
                        entry.isKnown = isKnownFile(file.data(), file.size());
                        entry.sha1sum = calculateSha1Hex(file.data(), file.size());
                        entry.variant = getPresetForFile(entry.sha1sum);
                    }
                }
                entry.analyzed = true;
            }
        }
        if (!foundOne)
            _analyzing = false;
    }
    return foundOne;
}

const KnownRomInfo* findRom(const std::string& sha1)
{
    for(int i = 0; i < g_knownRomNum; ++i) {
        if(sha1 == g_knownRoms[i].sha1) {
            return &g_knownRoms[i];
        }
    }
    return nullptr;
}

bool Librarian::isKnownFile(const uint8_t* data, size_t size) const
{
    auto sha1sum = calculateSha1Hex(data, size);
    return _cfg.romConfigs.count(sha1sum) || findRom(sha1sum) != nullptr;
}

emu::Chip8EmulatorOptions::SupportedPreset Librarian::getPresetForFile(std::string sha1sum) const
{
    auto cfgIter = _cfg.romConfigs.find(sha1sum);
    if(cfgIter != _cfg.romConfigs.end())
        return cfgIter->second.behaviorBase;
    const auto* romInfo = findRom(sha1sum);
    return romInfo ? emu::Chip8EmulatorOptions::presetForVariant(romInfo->variant) : emu::Chip8EmulatorOptions::eCHIP8;
}

emu::Chip8EmulatorOptions::SupportedPreset Librarian::getPresetForFile(const uint8_t* data, size_t size) const
{
    auto sha1sum = calculateSha1Hex(data, size);
    return getPresetForFile(sha1sum);
}

emu::Chip8EmulatorOptions::SupportedPreset Librarian::getEstimatedPresetForFile(emu::Chip8EmulatorOptions::SupportedPreset currentPreset, const uint8_t* data, size_t size) const
{
    emu::Chip8Decompiler dec;
    uint16_t startAddress = 0x200;  // TODO: endsWith(entry.filePath, ".c8x") ? 0x300 : 0x200;
    dec.decompile("", data, startAddress, size, startAddress, nullptr, true, true);
    auto possibleVariants = dec.possibleVariants();
    if ((uint64_t)dec.possibleVariants()) {
        if (dec.supportsVariant(emu::Chip8EmulatorOptions::variantForPreset(currentPreset))) {
            return currentPreset;
        }
        else if (dec.supportsVariant(emu::Chip8Variant::XO_CHIP))
            return emu::Chip8EmulatorOptions::eXOCHIP;
        else if (dec.supportsVariant(emu::Chip8Variant::MEGA_CHIP))
            return emu::Chip8EmulatorOptions::eMEGACHIP;
        else if (dec.supportsVariant(emu::Chip8Variant::SCHIP_1_1))
            return emu::Chip8EmulatorOptions::eSCHIP11;
        else if (dec.supportsVariant(emu::Chip8Variant::SCHIP_1_0))
            return emu::Chip8EmulatorOptions::eSCHIP10;
        else if (dec.supportsVariant(emu::Chip8Variant::CHIP_48))
            return emu::Chip8EmulatorOptions::eCHIP48;
        else if (dec.supportsVariant(emu::Chip8Variant::CHIP_10))
            return emu::Chip8EmulatorOptions::eSCHIP10;
    }
    return emu::Chip8EmulatorOptions::eCHIP8;
}

emu::Chip8EmulatorOptions Librarian::getOptionsForFile(const uint8_t* data, size_t size) const
{
    auto sha1sum = calculateSha1Hex(data, size);
    auto cfgIter = _cfg.romConfigs.find(sha1sum);
    if(cfgIter != _cfg.romConfigs.end()) {
        return cfgIter->second;
    }
    const auto* romInfo = findRom(sha1sum);
    if (romInfo) {
        auto preset = emu::Chip8EmulatorOptions::presetForVariant(romInfo->variant);
        auto options = emu::Chip8EmulatorOptions::optionsOfPreset(preset);
        if(romInfo->options) {
            emu::from_json(nlohmann::json::parse(std::string(romInfo->options)), options);
            options.behaviorBase = preset;
        }
        return options;
    }
    return emu::Chip8EmulatorOptions::optionsOfPreset(emu::Chip8EmulatorOptions::eCHIP8);
}

Librarian::Screenshot Librarian::genScreenshot(const Info& info, const std::array<uint32_t, 256> palette) const
{
    using namespace std::literals::chrono_literals;
    if(info.analyzed && (info.type == Info::eROM_FILE || info.type == Info::eOCTO_SOURCE) ) {
        emu::Chip8HeadlessHostEx host;
        host.updateEmulatorOptions({});
        if(host.loadRom((fs::path(_currentPath) / info.filePath).string().c_str(), true)) {
            auto& chipEmu = host.chipEmu();
            auto options = host.options();
            auto ticks = 5000;
            auto startChip8 = std::chrono::steady_clock::now();
            auto colors = palette;
            if(options.hasColors()) {
                options.updateColors(colors);
            }
            int64_t lastCycles = -1;
            int64_t cycles = 0;
            int tickCount = 0;
            for (tickCount = 0; tickCount < ticks /* && (cycles == chipEmu.getCycles()) != lastCycles */ && std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startChip8) < 100ms; ++tickCount) {
                chipEmu.tick(options.instructionsPerFrame);
                lastCycles = cycles;
            }
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startChip8).count();
            TraceLog(LOG_WARNING, "executed %d cycles and %d frames in %dms for screenshot", (int)chipEmu.getCycles(), tickCount, (int)duration);
            if (chipEmu.getScreen()) {
                Screenshot s;
                auto screen = *chipEmu.getScreen();
                screen.setPalette(colors);
                if (chipEmu.isDoublePixel()) {
                    s.width = chipEmu.getCurrentScreenWidth() / 2;
                    s.height = chipEmu.getCurrentScreenHeight() / 2;
                    s.pixel.resize(s.width * s.height);
                    for (int y = 0; y < chipEmu.getCurrentScreenHeight() / 2; ++y) {
                        for (int x = 0; x < chipEmu.getCurrentScreenWidth() / 2; ++x) {
                            s.pixel[y * s.width + x] = screen.getPixel(x*2, y*2);
                        }
                    }
                }
                else {
                    s.width = chipEmu.getCurrentScreenWidth();
                    s.height = chipEmu.getCurrentScreenHeight();
                    s.pixel.resize(s.width * s.height);
                    for (int y = 0; y < chipEmu.getCurrentScreenHeight(); ++y) {
                        for (int x = 0; x < chipEmu.getCurrentScreenWidth(); ++x) {
                            s.pixel[y * s.width + x] = screen.getPixel(x, y);
                        }
                    }
                }
                return s;
            }
            else if (chipEmu.getScreenRGBA()) {
                Screenshot s;
                auto screen = *chipEmu.getScreenRGBA();
                s.width = chipEmu.getCurrentScreenWidth();
                s.height = chipEmu.getCurrentScreenHeight();
                s.pixel.resize(s.width * s.height);
                for (int y = 0; y < chipEmu.getCurrentScreenHeight(); ++y) {
                    for (int x = 0; x < chipEmu.getCurrentScreenWidth(); ++x) {
                        s.pixel[y * s.width + x] = screen.getPixel(x, y);
                    }
                }
                return s;
            }
        }
    }
    return Librarian::Screenshot();
}

bool Librarian::isPrefixedTPDRom(const uint8_t* data, size_t size)
{
    static const uint8_t magic[] = {0x12, 0x60, 0x01, 0x7a, 0x42, 0x70, 0x22, 0x78};
    return size > 0x60 && std::memcmp(magic, data, 8) == 0;
}

bool Librarian::isPrefixedRSTDPRom(const uint8_t* data, size_t size)
{
    static const uint8_t magic[] = {0x9c, 0x7c, 0x00, 0xbc, 0xfb, 0x10, 0x30, 0xfc};
    return size > 0xC0 && isPrefixedTPDRom(data, size) && std::memcmp(magic, data + 0x50, 8) == 0;
}
