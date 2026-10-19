#include <emulation/chip8rsm8k1.hpp>
#include <emulation/coreregistry.hpp>
#include <rsm8k1/Chip8Core.hpp>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <stdexcept>

namespace {
void check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
struct Host final : emu::EmulatorHost {
    std::array<bool,16> keys{};
    int presentations = 0, preClears = 0;
    emu::Chip8RSM8k1* core = nullptr;
    uint32_t presentedPixel = 0;
    bool isHeadless() const override { return true; }
    int getKeyPressed() override { return 0; }
    bool isKeyDown(uint8_t key) override { return keys.at(key); }
    const std::array<bool,16>& getKeyStates() const override { return keys; }
    void updateScreen() override { ++presentations; if(core && core->getScreenRGBA()) presentedPixel = core->getScreenRGBA()->getPixel(0,0); }
    void vblank() override {}
    void updatePalette(const std::array<uint8_t,16>&) override {}
    void updatePalette(const std::vector<uint32_t>&, size_t) override {}
    void preClear() override { ++preClears; }
};
struct ReferenceHost final : rsm8k1::Platform {
    uint32_t now = 0;
    uint32_t keys = 0;
    int presentations = 0;
    int starts = 0, stops = 0;
    bool releaseOnKeycode = false;
    std::span<const uint8_t> sample{};
    uint32_t millis() override { return now; }
    rsm8k1::HostInput pollInput() override { rsm8k1::HostInput input; input.chipKeys = keys; return input; }
    uint32_t pollKeycode() override { if(releaseOnKeycode) { keys = 0; return 0; } return keys; }
    void present() override { ++presentations; }
    void playMegaChipSound(uint16_t, uint32_t, std::span<const uint8_t> data) override { ++starts; sample = data; }
    void stopMegaChipSound() override { ++stops; }
    void debugLog(const char*, ...) override {}
};
struct Pair {
    Host host;
    ReferenceHost referenceHost;
    emu::Properties props = emu::Chip8RSM8k1Options{}.asProperties();
    std::unique_ptr<emu::Chip8RSM8k1> core = std::make_unique<emu::Chip8RSM8k1>(host, props);
    std::unique_ptr<rsm8k1::Chip8Core> reference = std::make_unique<rsm8k1::Chip8Core>(referenceHost);
    Pair() { host.core = core.get(); }
    void reset() { core->reset(); reference->chip8_resetcpu(); core->setExecMode(emu::GenericCpu::eRUNNING); }
    void compare(bool fullMemory = true) {
        const auto& a = core->nativeState(); const auto& b = reference->state();
        check(!core->inErrorState(), core->errorMessage().c_str());
        check(core->getPC() == b.programCounter + 0x200, "Guest PC mismatch");
        check(core->getI() == (b.charset ? b.iregister : b.iregister + 0x200), "Guest I mismatch");
        check(core->getSP() == b.currentStack / 2, "Guest SP mismatch");
        check(a.vreg == b.vreg && a.stack == b.stack, "Registers/stack mismatch");
        check(a.delayTimer == b.delayTimer && a.soundTimer == b.soundTimer && a.chipTime == b.chipTime, "Timer mismatch");
        check(a.currentTick == b.currentTick && a.ticksPerFrame == b.ticksPerFrame, "Refresh budget mismatch");
        check(a.screen == b.screen && a.megaChipBuffer == b.megaChipBuffer && a.megaChipPalette == b.megaChipPalette, "Screen/palette mismatch");
        check(a.charset == b.charset && a.extendedMode == b.extendedMode && a.megaChipEnabled == b.megaChipEnabled &&
              a.randomCount == b.randomCount && a.exit == b.exit, "Mode/random/exit mismatch");
        check(a.megaChipSpriteWidth == b.megaChipSpriteWidth && a.megaChipSpriteHeight == b.megaChipSpriteHeight &&
              a.megaChipAlpha == b.megaChipAlpha && a.megaChipBlendMode == b.megaChipBlendMode &&
              a.megaChipCollisionColor == b.megaChipCollisionColor, "MegaChip control mismatch");
        if(fullMemory) check(std::equal(a.memory.begin(), a.memory.end(), b.memory.begin()), "RAM mismatch");
    }
    void opcode(uint16_t opcode, bool fullMemory = false) {
        core->setRegister(19, 0x200); reference->state().programCounter = 0;
        core->memory()[0x200] = reference->state().memory[0] = opcode >> 8;
        core->memory()[0x201] = reference->state().memory[1] = opcode;
        core->setExecMode(emu::GenericCpu::eRUNNING);
        core->executeInstruction(); reference->chip8_runcpu(1); compare(fullMemory);
    }
    void load(std::span<const uint8_t> bytes) { check(core->loadData(bytes,{}), "Cadmium load failed"); reference->chip8_loadrom(bytes); }
};
void arithmetic(Pair& p) {
    constexpr uint8_t values[] = {0,1,2,3,0x7f,0x80,0xfe,0xff};
    for(int mode : {0,1}) {
        p.props["vipMode"].setBool(mode); p.core->updateProperties(p.props,p.props["vipMode"]);
        p.reference->state().emulationMode = mode;
        for(int x : {0,1,15}) for(int y : {0,1,15}) for(auto a : values) for(auto b : values) for(int op : {0,1,2,3,4,5,6,7,14}) {
            for(int n = 0; n < 16; ++n) { p.core->setRegister(n,0xa5); p.reference->state().vreg[n] = 0xa5; }
            p.core->setRegister(x,a); p.core->setRegister(y,b); p.reference->state().vreg[x]=a; p.reference->state().vreg[y]=b;
            p.opcode(0x8000 | x << 8 | y << 4 | op);
        }
    }
    p.props["vipMode"].setBool(false); p.core->updateProperties(p.props,p.props["vipMode"]);
    p.reference->state().emulationMode=0;
}
void quirks(Pair& p) {
    p.reset();
    p.opcode(0x6011); p.opcode(0xb234);
    check(p.core->getPC() == 0x445, "Bnnn bug was corrected accidentally");
    p.opcode(0x2208); check(p.core->stackElement(0) == 0x202, "Cadmium stack address translation"); p.opcode(0x00ee);
    p.opcode(0xa230); p.opcode(0xf033,true); p.opcode(0xff75,true); p.opcode(0xff85,true);
    p.opcode(0x512f); p.opcode(0x912f); p.opcode(0xa050); p.opcode(0xf01e); p.opcode(0xd015);
    for(int n=0;n<260;++n) p.opcode(0xc0ff);
    p.opcode(0x00ff); p.opcode(0xd010); p.opcode(0x00fb); p.opcode(0x00fc); p.opcode(0x00c3);
    p.opcode(0x00fe); p.opcode(0x0011); p.opcode(0x0302); p.opcode(0x0401); p.opcode(0x0500); p.opcode(0x080e); p.opcode(0x0902);
    const uint8_t colors[] = {0xff,0x12,0x34,0x56,0xff,0x78,0x9a,0xbc};
    std::copy(std::begin(colors),std::end(colors),p.core->memory()+0x230);
    std::copy(std::begin(colors),std::end(colors),p.reference->state().memory.begin()+0x30);
    p.opcode(0xa230); p.opcode(0x0202);
    p.core->memory()[0x240] = p.reference->state().memory[0x40] = 1;
    p.core->memory()[0x241] = p.reference->state().memory[0x41] = 0;
    p.opcode(0x6000); p.opcode(0x6100); p.opcode(0xa240); p.opcode(0xd010); p.opcode(0x00e0);
    check(p.core->getScreenRGBA()->getPixel(0,0) == be32(0x123456ff), "Presented MegaChip frame was lost when work buffer cleared");
    check(p.host.preClears==1, "MegaChip clear must call Cadmium preClear");
    check(p.core->getWorkRGBA()->getPixel(0,0) == be32(255), "MegaChip work buffer was not cleared");
    const uint8_t audio[] = {0x56,0x22,0,0,2,0xee,0x10,0xf0};
    std::copy(std::begin(audio),std::end(audio),p.core->memory()+0x230);
    std::copy(std::begin(audio),std::end(audio),p.reference->state().memory.begin()+0x30);
    p.opcode(0xa230); p.opcode(0x060f);
    int16_t samples[6]; p.core->renderAudio(samples,6,44100);
    check(samples[0]==-28672 && samples[1]==-28672 && samples[2]==28672 && samples[3]==28672 && samples[4]==-28672, "PCM quantized loop mismatch");
    p.opcode(0x0700); p.core->renderAudio(samples,6,44100); check(samples[0]==0, "PCM stop failed");
    p.opcode(0x0010); p.opcode(0x0000); p.opcode(0x00fd);
}
void timersAndFrames(Pair& p) {
    p.reset(); p.opcode(0x6009); p.opcode(0xf015); p.opcode(0xf018);
    for(uint32_t now : {20U,21U,42U,1000U}) {
        p.core->advanceTime(now-p.referenceHost.now); p.referenceHost.now=now; p.opcode(0x6101);
    }
    p.reset(); const uint8_t loop[]={0x12,0x00}; p.load(loop);
    for(int n=0;n<10;++n) {
        p.core->executeFrame(); p.referenceHost.now+=20; p.reference->runRefresh(1); p.compare();
    }
    check(p.core->cycles()==250, "Double-counted ticks must produce 25 instructions/refresh");
}
void keyWait(Pair& p) {
    p.reset(); p.host.keys[5]=true; p.referenceHost.keys=1<<5; p.referenceHost.releaseOnKeycode=true;
    p.core->memory()[0x200]=0xf1; p.core->memory()[0x201]=0x0a;
    p.reference->state().memory[0]=0xf1; p.reference->state().memory[1]=0x0a;
    p.core->executeInstruction();
    check(p.core->cpuState()==emu::GenericCpu::eWAIT && p.core->getPC()==0x200, "Fx0A must yield without retiring");
    p.host.keys[5]=false; p.core->executeInstruction(); p.reference->chip8_runcpu(1); p.compare();
    check(p.core->getV(1)==5, "Fx0A result mismatch");
    p.referenceHost.releaseOnKeycode=false;
}
void faults(Pair& p) {
    for(const uint16_t op : {0x00ee,0x1100,0xe09e,0xf055}) {
        p.reset(); p.core->setRegister(0,16); p.core->setRegister(16,0x100);
        p.core->memory()[0x200]=op>>8; p.core->memory()[0x201]=op;
        p.core->executeInstruction();
        if(op==0x1100) p.core->executeInstruction();
        check(p.core->inErrorState() && p.core->execMode()==emu::GenericCpu::ePAUSED, "Unsafe access must halt with eERROR");
    }
    p.reset(); p.core->setRegister(20,16); p.core->memory()[0x200]=0x22; p.core->memory()[0x201]=0;
    p.core->executeInstruction(); check(p.core->inErrorState(),"Stack overflow not trapped");
    p.reset(); const uint8_t byte[]={1}; check(!p.core->loadData(byte,0x100),"Low RAM load accepted");
}
void demo(Pair& p, const char* path) {
    std::ifstream input(path,std::ios::binary); check(bool(input),"Missing demo");
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input)),{});
    p.reset(); p.load(bytes);
    for(int n=0;n<30;++n) { p.core->executeFrame(); p.referenceHost.now+=20; p.reference->runRefresh(1); p.compare(); }
}
}
int main(int argc,char** argv) {
    try {
        Pair pair;
        arithmetic(pair); quirks(pair); timersAndFrames(pair); keyWait(pair); faults(pair);
        for(int n=1;n<argc;++n) demo(pair,argv[n]);
        std::printf("RS-M8K1: 10,368 arithmetic comparisons, opcode/graphics/audio/timing/input/bounds checks, %d demo ROMs (%d frames) passed.\n", argc - 1, (argc - 1) * 30);
    } catch(const std::exception& e) { std::fprintf(stderr,"%s\n",e.what()); return 1; }
}
