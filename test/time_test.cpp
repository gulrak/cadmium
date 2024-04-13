//---------------------------------------------------------------------------------------
// test/time_test.cpp
//---------------------------------------------------------------------------------------
//
// Copyright (c) 2023, Steffen Schümann <s.schuemann@pobox.com>
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

#include <doctest/doctest.h>

#include <emulation/time.hpp>
#include "chip8adapter.hpp"
#include "chip8testhelper.hpp"
#include <emulation/chip8options.hpp>
#include <emulation/chip8cores.hpp>
#include <emulation/chip8strict.hpp>
#include <emulation/chip8dream.hpp>
#include <emulation/chip8vip.hpp>
#include "../src/chip8emuhostex.hpp"

using namespace emu;

TEST_CASE("Time - construction")
{
    {
        Time t{};
        CHECK(!t.seconds());
        CHECK(!t.ticks());
        CHECK(t.isZero());
        CHECK(t == Time::zero);
    }

    {
        Time t{42,12};
        CHECK(t.seconds() == 42);
        CHECK(t.ticks() == 12);
    }

    {
        Time t{42, Time::ticksPerSecond};
        CHECK(t.seconds() == 43);
        CHECK(t.ticks() == 0);
    }

    {
        Time t{24.5};
        CHECK(t.seconds() == 24);
        CHECK(t.ticks() == Time::ticksPerSecond>>1);
    }

    {
        ClockedTime t{1000000};
        CHECK(!t.seconds());
        CHECK(!t.ticks());
    }

}

TEST_CASE("Time - difference")
{
    {
        ClockedTime a{500000}, b{500000};
        a.addCycles(42);
        CHECK(a.asClockTicks() == 42);
        CHECK(a.difference(b) == 42);
        CHECK(a.difference_us(b) == 84);
        CHECK(b.difference_us(a) == -84);
    }
}

TEST_CASE("Emulation timing")
{
    {
        auto opts = Chip8EmulatorOptions::optionsOfPreset(Chip8EmulatorOptions::eCHIP8);
        Chip8HeadlessTestHost host(opts);
        std::unique_ptr<IChip8Emulator> chip8 = std::make_unique<emu::Chip8StrictEmulator>(host, opts);
        chip8->reset();
        write(chip8, 0x200, {0x6000, 0x1200});
        chip8->executeFor(10000000); // execute 10s
        CHECK(chip8->frames() == 600);
        chip8->reset();
        write(chip8, 0x200, {0x6000, 0xA210, 0xD00F, 0x1204});
        chip8->executeFor(10000000); // execute 10s
        CHECK(chip8->frames() == 600);
        chip8->reset();
        write(chip8, 0x200, {0x6000, 0xA210, 0xD00F, 0x1204});
        int64_t exceed = 0;
        for(int i = 0; i < 600; ++i)
            exceed = chip8->executeFor(16667ll - exceed); // execute 16.667ms
        CHECK(chip8->frames() >= 600);
        CHECK(chip8->frames() < 602);
    }

    {
        auto opts = Chip8EmulatorOptions::optionsOfPreset(Chip8EmulatorOptions::eCHIP8);
        Chip8HeadlessTestHost host(opts);
        std::unique_ptr<IChip8Emulator> chip8fp = std::make_unique<emu::Chip8EmulatorFP>(host, opts);
        chip8fp->reset();
        write(chip8fp, 0x200, {0x6000, 0x1200});
        chip8fp->executeFor(10000000); // execute 10s
        CHECK(chip8fp->frames() == 600);
        chip8fp->reset();
        write(chip8fp, 0x200, {0x6000, 0xA210, 0xD00F, 0x1204});
        chip8fp->executeFor(10000000); // execute 10s
        CHECK(chip8fp->frames() == 600);
        chip8fp->reset();
        write(chip8fp, 0x200, {0x6000, 0xA210, 0xD00F, 0x1204});
        int64_t exceed = 0;
        for(int i = 0; i < 600; ++i)
            exceed = chip8fp->executeFor(16667 - exceed); // execute 16.667ms
        CHECK(chip8fp->frames() >= 600);
        CHECK(chip8fp->frames() < 602);
    }


    {
        auto opts = Chip8EmulatorOptions::optionsOfPreset(Chip8EmulatorOptions::eSCHIP11);
        Chip8HeadlessTestHost host(opts);
        std::unique_ptr<IChip8Emulator> chip8fp = std::make_unique<emu::Chip8EmulatorFP>(host, opts);
        chip8fp->reset();
        write(chip8fp, 0x200, {0x6000, 0x1200});
        chip8fp->executeFor(10000000); // execute 10s
        CHECK(chip8fp->frames() == 640);
        chip8fp->reset();
        write(chip8fp, 0x200, {0x6000, 0xA210, 0xD00F, 0x1204});
        chip8fp->executeFor(10000000); // execute 10s
        CHECK(chip8fp->frames() == 640);
        chip8fp->reset();
        write(chip8fp, 0x200, {0x6000, 0xA210, 0xD00F, 0x1204});
        int64_t exceed = 0;
        for(int i = 0; i < 600; ++i)
            exceed = chip8fp->executeFor(16667 - exceed); // execute 16.667ms
        CHECK(chip8fp->frames() == 640);
    }

    {
        auto opts = Chip8EmulatorOptions::optionsOfPreset(Chip8EmulatorOptions::eCHIP8VIP);
        Chip8HeadlessTestHost host(opts);
        std::unique_ptr<IChip8Emulator> chip8vip = std::make_unique<emu::Chip8VIP>(host, opts);
        chip8vip->reset();
        write(chip8vip, 0x200, {0x6000, 0x1200});
        auto initialFrames = chip8vip->frames();
        chip8vip->executeFor(10000000); // execute 10s
        CHECK(chip8vip->frames() - initialFrames == 600);
        chip8vip->reset();
        write(chip8vip, 0x200, {0x6000, 0xA210, 0xD008, 0x1204});
        chip8vip->executeFor(10000000); // execute 10s
        CHECK(chip8vip->frames() - initialFrames == 600);
        chip8vip->reset();
        write(chip8vip, 0x200, {0x6000, 0xA210, 0xD00F, 0x1204});
        int64_t exceed = 0;
        for(int i = 0; i < 600; ++i)
            exceed = chip8vip->executeFor(16667 - exceed); // execute 16.667ms
        CHECK(chip8vip->getTime().secondsRounded() == 10);
        CHECK(chip8vip->frames() >= 600);
        CHECK(chip8vip->frames() < 605);
    }

    {
        auto opts = Chip8EmulatorOptions::optionsOfPreset(Chip8EmulatorOptions::eCHIP8DREAM);
        Chip8HeadlessTestHost host(opts);
        std::unique_ptr<IChip8Emulator> dream6k8 = std::make_unique<emu::Chip8Dream>(host, opts);
        dream6k8->reset();
        write(dream6k8, 0x200, {0x6000, 0x1200});
        auto initialFrames = dream6k8->frames();
        dream6k8->executeFor(10000000); // execute 10s
        CHECK(dream6k8->frames() - initialFrames == 501); // The real frame rate is 1MHz / 19968 cycles per frame so the 501th frame has just begun and vsyncs are counted
        dream6k8->reset();
        write(dream6k8, 0x200, {0x6000, 0xA210, 0xD00F, 0x1204});
        dream6k8->executeFor(10000000); // execute 10s
        CHECK(dream6k8->frames() - initialFrames == 501); // The real frame rate is 1MHz / 19968 cycles per frame so the 501th frame has just begun and vsyncs are counted
        dream6k8->reset();
        write(dream6k8, 0x200, {0x6000, 0xA210, 0xD00F, 0x1204});
        int64_t exceed = 0;
        for(int i = 0; i < 600; ++i)
            exceed = dream6k8->executeFor(16667 - exceed); // execute 16.667ms
        CHECK(dream6k8->frames() >= 500);
        CHECK(dream6k8->frames() < 505);
    }
}
