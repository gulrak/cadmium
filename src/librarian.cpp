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
#include <emulation/chip8decompiler.hpp>
#include <emulation/utility.hpp>
#include <librarian.hpp>
#include <filesystem.hpp>

#include <raylib.h>

#include <algorithm>

static std::unique_ptr<emu::IChip8Emulator> minion;

template<typename TP>
inline std::chrono::system_clock::time_point convertClock(TP tp)
{
    using namespace std::chrono;
    return time_point_cast<system_clock::duration>(tp - TP::clock::now() + system_clock::now());
}

static std::map<std::string, emu::Chip8EmulatorOptions::SupportedPreset> g_knownRoms = {
    // CHIP-8
    {"eb412becb086d3cbccce4e3e370b9149b969cff9", emu::Chip8EmulatorOptions::eCHIP8},

    // CHIP-8 VIP (hybrid roms)
    {"12fccf60004f685c112fe3db3d3bcfba104cbcb1", emu::Chip8EmulatorOptions::eCHIP8VIP},
    {"2b711cf58008f03168d0547063fe8e3c72f65ae3", emu::Chip8EmulatorOptions::eCHIP8VIP},
    {"346f2760ca55bb6d45b1f255fe4960a7d244191e", emu::Chip8EmulatorOptions::eCHIP8VIP},
    {"4701417c61d80d40fe6e3ae06d891cbe730c0dc7", emu::Chip8EmulatorOptions::eCHIP8VIP},
    {"9bfae01da1a94f99aba692da1a7a2148eb8561b4", emu::Chip8EmulatorOptions::eCHIP8VIP},
    {"c5a2e40a381086e7d2064f9836c57224e27ec7ed", emu::Chip8EmulatorOptions::eCHIP8VIP},

    // HIRES-CHIP-8 (not supported yet)
    //{"066e7a84efde433e4d937d8aa41518666955086c", emu::Chip8EmulatorOptions::eHIRESCHIP8},
    //{"1ebcb2ec0be2ec9fa209d5c73be19b2d408399bf", emu::Chip8EmulatorOptions::eHIRESCHIP8},
    //{"200b313e4d4c1970641142cc7ff578d7956b93da", emu::Chip8EmulatorOptions::eHIRESCHIP8},
    //{"4a89a66cfe78f788f45598d69f975d470229df0b", emu::Chip8EmulatorOptions::eHIRESCHIP8}, - Might be Hybrid Hires Chip-8
    //{"70aa0e7f25f0f0fd6ec7c59e427bf1d03ee95617", emu::Chip8EmulatorOptions::eHIRESCHIP8},
    //{"71d06da9e605804d2099b808c02548ab2b3511b2", emu::Chip8EmulatorOptions::eHIRESCHIP8},
    //{"8d56a781bf16acccb307177b80ff326f62aabbdc", emu::Chip8EmulatorOptions::eHIRESCHIP8},
    //{"af98ee11adae28a6153cae8e4c16afa00f861907", emu::Chip8EmulatorOptions::eHIRESCHIP8},
    //{"b2c55b6aba3e2910036d5b5bc3956cf7493e0221", emu::Chip8EmulatorOptions::eHIRESCHIP8},

    // CHIP-8X (not supported yet)
    //{"18418563acd5c64ff410fdede56ffd80c139888a", emu::Chip8EmulatorOptions::eCHIP8X},
    //{"a5fe40576733f4324b85c5cff10d831f9d245449", emu::Chip8EmulatorOptions::eCHIP8X},
    //{"a9365fc13d22118cdedf3979e2199831b0b605a9", emu::Chip8EmulatorOptions::eCHIP8X},
    //{"f452b63f9ba21d8716b7630f4f327c2ebdff0755", emu::Chip8EmulatorOptions::eCHIP8X},
    //{"fede2cb2fa570b361c7915e24499a263f3c36a12", emu::Chip8EmulatorOptions::eCHIP8X},

    // CHIP-10
    {"8109e5f502a624ce6c96b8aa4b44b3f7dc0ef968", emu::Chip8EmulatorOptions::eCHIP10},

    // SUPER-CHIP 1.1
    {"01ffe488efbe14ca63de1c23053806533e329f3f", emu::Chip8EmulatorOptions::eSCHIP11},
    {"044021b046cf207c0b555ea884d61a726f7a3c22", emu::Chip8EmulatorOptions::eSCHIP11},
    {"0663449e1cc8d79ee38075fe86d6b9439a7e43d7", emu::Chip8EmulatorOptions::eSCHIP11},
    {"12572c9e957cace53076d1656ea1b12cd0f331af", emu::Chip8EmulatorOptions::eSCHIP11},
    {"12d3bf6c28ebf07b49524f38d6436f814749d4c0", emu::Chip8EmulatorOptions::eSCHIP11},
    {"17d775833f073be77f2834751523996e0a398edd", emu::Chip8EmulatorOptions::eSCHIP11},
    {"0b5522b1ce775879092be840b0e840cb1dea74fd", emu::Chip8EmulatorOptions::eSCHIP11},
    {"1f386e1ae47957dec485d3e4034dff706d316d15", emu::Chip8EmulatorOptions::eSCHIP11},
    {"1ff6e2a8920c5b48def34348df65226285f39ce9", emu::Chip8EmulatorOptions::eSCHIP11},
    {"244c746b4f81c9c3df9cea69389387da67589bb8", emu::Chip8EmulatorOptions::eSCHIP11},
    {"24fd50a95b84e3a42e336a06567a9752f17b9979", emu::Chip8EmulatorOptions::eSCHIP11},
    {"28e8f7b405d48647eb090a550ec679327c57f2f5", emu::Chip8EmulatorOptions::eSCHIP11},
    {"29f83328069205a1cdb7020846cca34d6988c83c", emu::Chip8EmulatorOptions::eSCHIP11},
    {"2cd26a9a84ed2be6aaa6916d49b2e5c503196400", emu::Chip8EmulatorOptions::eSCHIP11},
    {"2d415bf1f31777b22ad73208c4d1ad27d5d4f367", emu::Chip8EmulatorOptions::eSCHIP11},
    {"31fe380556d65600ef293d99aabd3b6bb119aa01", emu::Chip8EmulatorOptions::eSCHIP11},
    {"332e892ad054cf182e1ca4c465b603b8261ccec9", emu::Chip8EmulatorOptions::eSCHIP11},
    {"3c444e43e5f02dac4324b7b24cd38ef4938a4b56", emu::Chip8EmulatorOptions::eSCHIP11},
    {"3ee8a64a9af37a8d24aab9e73410b94cc0a4018f", emu::Chip8EmulatorOptions::eSCHIP11},
    {"416763e940918ee7cfc5c277d7f2b66de71a46a1", emu::Chip8EmulatorOptions::eSCHIP11},
    {"453545dc5e6079e9d9be9d3775d2615c4b60724f", emu::Chip8EmulatorOptions::eSCHIP11},
    {"45f7c33b284b0f3e1393f0dd97e4b3b9fd9c63c9", emu::Chip8EmulatorOptions::eSCHIP11},
    {"480b4dfa0918d034aea0bf8d8ef5b5a55e94b50b", emu::Chip8EmulatorOptions::eSCHIP11},
    {"518c1d40f5d768ee49d2b7951d998588ef8238ba", emu::Chip8EmulatorOptions::eSCHIP11},
    {"51a31cc51414b4dd6c5c54081574f915e6f53744", emu::Chip8EmulatorOptions::eSCHIP11},
    {"531c44e8204d8ab8c078bad36e34067baddfccdb", emu::Chip8EmulatorOptions::eSCHIP11},
    {"58f7ce407aedf456dc8992342f4a6f9f0647383b", emu::Chip8EmulatorOptions::eSCHIP11},
    {"5abf3dcf4ce0e396a3a5bf977b1ea988535d35d5", emu::Chip8EmulatorOptions::eSCHIP11},
    {"5b733a60e7208f6aa0d15c99390ce4f670b2b886", emu::Chip8EmulatorOptions::eSCHIP11},
    {"5c0fff21df64f3fe8683a115353c293d435ca01a", emu::Chip8EmulatorOptions::eSCHIP11},
    {"61931487c694c5bc6978ae22c0a36aca5a647e24", emu::Chip8EmulatorOptions::eSCHIP11},
    {"627f01b20ce4d33f6df1aa88acb405a3a732bde0", emu::Chip8EmulatorOptions::eSCHIP11},
    {"63e787fc3e78e5fb3a394cf1bc654ad9633d8907", emu::Chip8EmulatorOptions::eSCHIP11},
    {"64176ff030ebff27f483db5a16f38f2383d0026e", emu::Chip8EmulatorOptions::eSCHIP11},
    {"64536d549c986e9edf25de9fa89db60d2ade85c0", emu::Chip8EmulatorOptions::eSCHIP11},
    {"681eaf2c6422cdd0e0ca0cf9f4c3a436b7b6f292", emu::Chip8EmulatorOptions::eSCHIP11},
    {"6b6502b03183e492f8170172308df9876c29d1d9", emu::Chip8EmulatorOptions::eSCHIP11},
    {"6bb78d8a0aba93ea18eabdd0134cbdccd1dc2d16", emu::Chip8EmulatorOptions::eSCHIP11},
    {"6d4514ae3a43c307763648b0bdd485fb77bcf20d", emu::Chip8EmulatorOptions::eSCHIP11},
    {"6d677bb44500a5ee4754b3a75516cfd9e73947fc", emu::Chip8EmulatorOptions::eSCHIP11},
    {"6f8e85158be98f30bf3cd5df60d7a7ad71c5f3e1", emu::Chip8EmulatorOptions::eSCHIP11},
    {"702066d7248dfa81d5535942e7c6ed3a32ebc84c", emu::Chip8EmulatorOptions::eSCHIP11},
    {"709328365147967f434d1bf78430e9ec160cc24f", emu::Chip8EmulatorOptions::eSCHIP11},
    {"7321e1bbe885a749b2ca875d1f49fb6c01f54f91", emu::Chip8EmulatorOptions::eSCHIP11},
    {"733d41d4c367214cd177071ee6a783a46cf14bf4", emu::Chip8EmulatorOptions::eSCHIP11},
    {"77d5d2d9c5fe19c72d6564b3601a8d17cfedcb41", emu::Chip8EmulatorOptions::eSCHIP11},
    {"7a4a89870f2ab23c28024dd1c3dd52cf1af1ad00", emu::Chip8EmulatorOptions::eSCHIP11},
    {"808aeb072604809e0ef13c245115a81f40422d1d", emu::Chip8EmulatorOptions::eSCHIP11},
    {"83300ff710acdd8417376b88adf40f68171f7ec7", emu::Chip8EmulatorOptions::eSCHIP11},
    {"91015f51f6ffd0043a2ae757dc11ec35216949ef", emu::Chip8EmulatorOptions::eSCHIP11},
    {"9514e9e2ab7e1ddc91265823e7e895de6c8dd303", emu::Chip8EmulatorOptions::eSCHIP11},
    {"9593099c1fe1be31cbaea526aa04fd492ff90382", emu::Chip8EmulatorOptions::eSCHIP11},
    {"96bc31f23c7f917dab4a082d2c7fd7c69820e6a4", emu::Chip8EmulatorOptions::eSCHIP11},
    {"9797a7eaf1e80ec19c085c60bb37991420f54678", emu::Chip8EmulatorOptions::eSCHIP11},
    {"99a97c772fc93d669b73016761ea6fee0210497e", emu::Chip8EmulatorOptions::eSCHIP11},
    {"9b7faac49c44c1194a3283c2ef89eabfca76fe38", emu::Chip8EmulatorOptions::eSCHIP11},
    {"9d9f88509b5033152b7b49d2c7ea3c3c5fce2bd6", emu::Chip8EmulatorOptions::eSCHIP11},
    {"9f7cf6fe0025878c26b317160c57edd06b3361ba", emu::Chip8EmulatorOptions::eSCHIP11},
    {"a05844df3305738e4030512f0063db2fe4f3bd11", emu::Chip8EmulatorOptions::eSCHIP11},
    {"a1ec824285a593cd1ca84dc6c732c61b0fe96330", emu::Chip8EmulatorOptions::eSCHIP11},
    {"a2788177b820a28cd27e6d2d180340cb7f4948fb", emu::Chip8EmulatorOptions::eSCHIP11},
    {"a4c8e14b43dc75bc960a42a5300f64dc6e52cf32", emu::Chip8EmulatorOptions::eSCHIP11},
    {"a558e24022e30dd5206909eeca074949f3fb6f59", emu::Chip8EmulatorOptions::eSCHIP11},
    {"a56c09537df0f32e2d49fb68cb2ba8216b38f632", emu::Chip8EmulatorOptions::eSCHIP11},
    {"a9bf29597674c39b4e11d964b352b1e52c4ebb2f", emu::Chip8EmulatorOptions::eSCHIP11},
    {"b3dcfd85a76a678960359e1ce9f742a4f9c35ed8", emu::Chip8EmulatorOptions::eSCHIP11},
    {"b76fbca2ec089c7e77f4a2f754db37854b99debc", emu::Chip8EmulatorOptions::eSCHIP11},
    {"b8be672909554abc17ed1ea0c694726f9a87b43d", emu::Chip8EmulatorOptions::eSCHIP11},
    {"bc5faf54f04da3f4dbde50d3b31ccfc2bf8b9e06", emu::Chip8EmulatorOptions::eSCHIP11},
    {"c1b605040e29cce2a6fc52334fb09b0985340314", emu::Chip8EmulatorOptions::eSCHIP11},
    {"c2a361700209116a300457eacbf33a8c40c01b83", emu::Chip8EmulatorOptions::eSCHIP11},
    {"c7c59b38129fdcec5bb0775a9a141b6ba936e706", emu::Chip8EmulatorOptions::eSCHIP11},
    {"c8375d6a626ea21532cde178a7a0a22b7e511414", emu::Chip8EmulatorOptions::eSCHIP11},
    {"c9583967a7a2fd2b8b14fc4fe0568844b9b9408e", emu::Chip8EmulatorOptions::eSCHIP11},
    {"cc8db4b4ce858b0255ade64b1b8ea9d7c9d7d7fb", emu::Chip8EmulatorOptions::eSCHIP11},
    {"d03f27f85a1cf68465e0853cc0c4abee4a94a4e5", emu::Chip8EmulatorOptions::eSCHIP11},
    {"d40abc54374e4343639f993e897e00904ddf85d9", emu::Chip8EmulatorOptions::eSCHIP11},
    {"d60314d126dd2aab429d2299dfc8740323adfae4", emu::Chip8EmulatorOptions::eSCHIP11},
    {"d68352857be1e44086aa0163eef8aad8251e0817", emu::Chip8EmulatorOptions::eSCHIP11},
    {"d6cbd3af85b4c55b83c4e01f3a17c66fcebe9ccc", emu::Chip8EmulatorOptions::eSCHIP11},
    {"d84494c47c5f63cf32fea555e8938c27941c2869", emu::Chip8EmulatorOptions::eSCHIP11},
    {"d9389d564baced03192503a58ad930110bb0fe03", emu::Chip8EmulatorOptions::eSCHIP11},
    {"dbb5b085117d513f1ce403959d7136b767bb3dd3", emu::Chip8EmulatorOptions::eSCHIP11},
    {"dd6ef80cadef1e7b42f71ad99573b1af2299e27d", emu::Chip8EmulatorOptions::eSCHIP11},
    {"debfea355f5737be394d3e62d4970e733ec3b6fc", emu::Chip8EmulatorOptions::eSCHIP11},
    {"e234c3781f9b65d2c1940976d10bf06ce7742b8d", emu::Chip8EmulatorOptions::eSCHIP11},
    {"e46f333cd95785b554f8dda5690e165959dfb4be", emu::Chip8EmulatorOptions::eSCHIP11},
    {"e4ef6fff9813c43bd7ad2ecaf02d1a3135d68418", emu::Chip8EmulatorOptions::eSCHIP11},
    {"e6a027d00c524ab7ae00b720f64a06ad1137836c", emu::Chip8EmulatorOptions::eSCHIP11},
    {"e6af47843f0ecc3302027a3756dd7b389a15e437", emu::Chip8EmulatorOptions::eSCHIP11},
    {"e6d4a8598999b3d95047babf67b529d83eaa9554", emu::Chip8EmulatorOptions::eSCHIP11},
    {"e6d910b7c9f9680df462662ce16336ebcb0eab1e", emu::Chip8EmulatorOptions::eSCHIP11},
    {"e8477fad78863714c508c046d2419248c5f89690", emu::Chip8EmulatorOptions::eSCHIP11},
    {"e9488cc1878ba9c0042d576cc1b3679ea1632098", emu::Chip8EmulatorOptions::eSCHIP11},
    {"eba3b6ac5539452d1dd0c7c045d69e6096c457dd", emu::Chip8EmulatorOptions::eSCHIP11},
    {"ebada8eb97ce40a91554386696f7daa33023cc8c", emu::Chip8EmulatorOptions::eSCHIP11},
    {"f11793f86baae9f5f0c77e5d7aa216c2180c3d07", emu::Chip8EmulatorOptions::eSCHIP11},
    {"f31a8912ffb8a2920eb7ad5d645aa65a413b6ae9", emu::Chip8EmulatorOptions::eSCHIP11},
    {"f3f2d91e4cb8886b5af2f556827574bf1fd8967c", emu::Chip8EmulatorOptions::eSCHIP11},
    {"f4e50d6e209324906b7899ed785a0d849a397abc", emu::Chip8EmulatorOptions::eSCHIP11},
    {"f505bdc0b1f2da3cc4a69e3baaba8c3bf5303692", emu::Chip8EmulatorOptions::eSCHIP11},
    {"f55ab7c3776fd9a94ffac82f0feb965e93c057f1", emu::Chip8EmulatorOptions::eSCHIP11},
    {"f56134c8196fdff347264a985add4d2648bac76a", emu::Chip8EmulatorOptions::eSCHIP11},
    {"f7a3e2e3272b03631efa561976f981bac351a603", emu::Chip8EmulatorOptions::eSCHIP11},
    {"f8008875a4b35dc7188eeca2a05535116371eaf0", emu::Chip8EmulatorOptions::eSCHIP11},
    {"fb6a79a1f42cd4539c3da2783d4f7f035d9b3a2c", emu::Chip8EmulatorOptions::eSCHIP11},
    {"fb8d0807a00353ae8071238a2eb7f1e555afe525", emu::Chip8EmulatorOptions::eSCHIP11},
    {"fcaa793332a83c93f4ed79f5ffbc8403c8b8aea0", emu::Chip8EmulatorOptions::eSCHIP11},
    {"ff5276bfd203634ef3034475ff7bc8bd9033a03d", emu::Chip8EmulatorOptions::eSCHIP11},

    // MEGACHIP
    {"599135c0a9bb33ce1bc5396fd30abef2df1cb2ed", emu::Chip8EmulatorOptions::eMEGACHIP},
    {"5b66aa248b3a0b6fffa6a72fdbee5e14e05d3f77", emu::Chip8EmulatorOptions::eMEGACHIP},
    {"683cbb00d59e48fa34c3bbd2fd2b10775b22178a", emu::Chip8EmulatorOptions::eMEGACHIP},
    {"9a09cdfa7b0820310043dd0f098384c1c9b325a3", emu::Chip8EmulatorOptions::eMEGACHIP},
    {"e62d9d26d2d1b65e6aafb8331cae333fbadcaebf", emu::Chip8EmulatorOptions::eMEGACHIP},

    // XO-CHIP
    {"018e6da9937173b1ac44d4261e848af485dcd305", emu::Chip8EmulatorOptions::eXOCHIP},
    {"0268a789a6c1e281b6fc472c41bbdd00a40e2850", emu::Chip8EmulatorOptions::eXOCHIP},
    {"0317e94014ebc3a9a1a2a33c46bc766a9cf44cb0", emu::Chip8EmulatorOptions::eXOCHIP},
    {"085394b959f03a0e525b404d8a68a36e56d6446a", emu::Chip8EmulatorOptions::eXOCHIP},
    {"0893dd3b5fafa013f07acc9aa98876f84f328d54", emu::Chip8EmulatorOptions::eXOCHIP},
    {"0dc782f0607d34b8355c150e81bc280de7472d94", emu::Chip8EmulatorOptions::eXOCHIP},
    {"1e981dac636d88d26a3fc056a53b28175f1d9b82", emu::Chip8EmulatorOptions::eXOCHIP},
    {"2229606a59bbcdeb81408f75e8646ea05553a580", emu::Chip8EmulatorOptions::eXOCHIP},
    {"2b48aa674707878bf6d22496a402985a9f7db9cb", emu::Chip8EmulatorOptions::eXOCHIP},
    {"2e87573b0fe9a49123bbb86d8384d00a302dc2e4", emu::Chip8EmulatorOptions::eXOCHIP},
    {"310e523071c697503f0da385997f9c77f9ad0ea9", emu::Chip8EmulatorOptions::eXOCHIP},
    {"33abb5f1ba7db3166636911c6cfa81a5ce5b861c", emu::Chip8EmulatorOptions::eXOCHIP},
    {"33ec2f3081bed56438dc207477f06cd77f3f07d9", emu::Chip8EmulatorOptions::eXOCHIP},
    {"440c5fbe9f5f840e76c308738fb0d37772d66674", emu::Chip8EmulatorOptions::eXOCHIP},
    {"4564a1bf149e5ab9777d33813a92cfd6ffc7a0bb", emu::Chip8EmulatorOptions::eXOCHIP},
    {"4cce9f3a79c8d7ee33a9bfde7099568e0f3274cd", emu::Chip8EmulatorOptions::eXOCHIP},
    {"59bdc7f990322d274d711b6b6982c7e8c9098e9e", emu::Chip8EmulatorOptions::eXOCHIP},
    {"5d99d0c763cf528660a10a390abe89f2d12b024a", emu::Chip8EmulatorOptions::eXOCHIP},
    {"5efc16ddebc1585b3c4d4cb27ce9fd76218c5d0a", emu::Chip8EmulatorOptions::eXOCHIP},
    {"642e6174ac7b2bccb7d0845eb5f18d2defbe98b4", emu::Chip8EmulatorOptions::eXOCHIP},
    {"66068809c482a30aa4475dc554a18d8d727d3521", emu::Chip8EmulatorOptions::eXOCHIP},
    {"66c15e550c9cda39b50220c49d22578dabbfe319", emu::Chip8EmulatorOptions::eXOCHIP},
    {"6e556d92f30a75e7fa8016891438ae082ef33ad4", emu::Chip8EmulatorOptions::eXOCHIP},
    {"76a770000b314659ac792e17724b783a464ab67e", emu::Chip8EmulatorOptions::eXOCHIP},
    {"858b55ce47e98a7b2238f8db33463f76fd15b18b", emu::Chip8EmulatorOptions::eXOCHIP},
    {"8603e177fcbb04a5b1a685c216380bee6a05b0f2", emu::Chip8EmulatorOptions::eXOCHIP},
    {"8b2fc2e08830b8a9e604d11c9b319e2cc0a581b3", emu::Chip8EmulatorOptions::eXOCHIP},
    {"8ebf74e790e58a8d5a7beff598bb32ed7eeeabf7", emu::Chip8EmulatorOptions::eXOCHIP},
    {"8fd0212f4b8b491e8eb260e995313bdb210b1d6b", emu::Chip8EmulatorOptions::eXOCHIP},
    {"92a325c36ad2116a5256946b8bf711ed9befd319", emu::Chip8EmulatorOptions::eXOCHIP},
    {"9bf96e23963995c6d702ae21c9b8741cbb688f47", emu::Chip8EmulatorOptions::eXOCHIP},
    {"a168709fcf09b28cd9b9519698d3d8a383944f43", emu::Chip8EmulatorOptions::eXOCHIP},
    {"adcfece2c527a68d8d74e6cfe7e84a8a04ad8182", emu::Chip8EmulatorOptions::eXOCHIP},
    {"b693e60f161e69c98b0bb2bc1761cf434f8fbb0e", emu::Chip8EmulatorOptions::eXOCHIP},
    {"ccec955da264cd92fdbb18c4971419497513ae42", emu::Chip8EmulatorOptions::eXOCHIP},
    {"ce15f2f4281b1069d33e00c801d5ed4390049a76", emu::Chip8EmulatorOptions::eXOCHIP},
    {"d4339dac64038f30130af02fbe73b57cd7d481a1", emu::Chip8EmulatorOptions::eXOCHIP},
    {"e14350d3b19443e5ad2848172bef9719a8680b01", emu::Chip8EmulatorOptions::eXOCHIP},
    {"e251b6132b15d411a9fe5d1e91a6579e3e057527", emu::Chip8EmulatorOptions::eXOCHIP},
    {"e2cf46c544bee2ef8a8b21dba1c583d5121b1b96", emu::Chip8EmulatorOptions::eXOCHIP},
    {"e2d86d6c70877e99ed4253c9a83d4da42e5a14ee", emu::Chip8EmulatorOptions::eXOCHIP},
    {"eb1a09cc11c73938f39ce8d52c8e06576dec3a32", emu::Chip8EmulatorOptions::eXOCHIP},
    {"f12038dcd28ca71661162bfb6fc92a8826f7d6b9", emu::Chip8EmulatorOptions::eXOCHIP},
    {"f5c666c33ed66a9662cef78b1ef62f80a33b0358", emu::Chip8EmulatorOptions::eXOCHIP},
    {"f64e87b8a4161806b4dad9bfc317d4341b410beb", emu::Chip8EmulatorOptions::eXOCHIP},
    {"fbc7711ad068015b957e91d8714636b2ac90d9cb", emu::Chip8EmulatorOptions::eXOCHIP},
};

std::string Librarian::Info::minimumOpcodeProfile() const
{
    auto mask = static_cast<uint64_t>(possibleVariants);
    if(mask) {
        auto cv = static_cast<emu::Chip8Variant>(mask & -mask);
        return emu::Chip8Decompiler::chipVariantName(cv).first;
    }
    return "unknown";
}

Librarian::Librarian()
{
    fetchDir(".");
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
                if(entry.type == Info::eROM_FILE) {
                    if (entry.variant == emu::Chip8EmulatorOptions::eCHIP8) {
                        auto file = loadFile((fs::path(_currentPath) / entry.filePath).string());
                        emu::Chip8Decompiler dec;
                        uint16_t startAddress = endsWith(entry.filePath, ".c8x") ? 0x300 : 0x200;
                        dec.decompile(entry.filePath, file.data(), startAddress, file.size(), startAddress, nullptr, true, true);
                        entry.possibleVariants = dec.possibleVariants;
                        if ((uint64_t)dec.possibleVariants) {
                            if(dec.supportsVariant(options.presetAsVariant()))
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
                entry.analyzed = true;
            }
        }
        if (!foundOne)
            _analyzing = false;
    }
    return foundOne;
}

emu::Chip8EmulatorOptions::SupportedPreset Librarian::getPresetForFile(std::string sha1sum)
{
    auto iter = g_knownRoms.find(sha1sum);
    return iter != g_knownRoms.end() ? iter->second : emu::Chip8EmulatorOptions::eCHIP8;
}

emu::Chip8EmulatorOptions::SupportedPreset Librarian::getPresetForFile(const uint8_t* data, size_t size)
{
    auto sha1sum = calculateSha1Hex(data, size);
    return getPresetForFile(sha1sum);
}
