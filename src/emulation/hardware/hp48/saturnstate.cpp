//---------------------------------------------------------------------------------------
// src/emulation/hardware/hp48/saturnstate.cpp
//---------------------------------------------------------------------------------------
//
// Copyright (c) 2026, Steffen Schümann <s.schuemann@pobox.com>
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

#include "saturnstate.hpp"
#include <nlohmann/json.hpp>
#include <ostream>
#include <random>
#include <fmt/core.h>

namespace hp48 {

void to_json(nlohmann::json& j, const SaturnState& s) {
    nlohmann::ordered_json oj;
    to_json(oj, s);
    j = oj;
}

void to_json(nlohmann::ordered_json& j, const SaturnState& s) {
    j = nlohmann::ordered_json{
        {"a", s.a}, {"b", s.b}, {"c", s.c}, {"d", s.d},
        {"r0", s.r0}, {"r1", s.r1}, {"r2", s.r2}, {"r3", s.r3}, {"r4", s.r4},
        {"d0", s.d0}, {"d1", s.d1},
        {"pc", s.pc},
        {"p", s.p},
        {"carry", s.carry},
        {"hexMode", s.hexMode},
        {"pstat", s.pstat},
        {"xm", (uint8_t)s.xm}, {"sb", (uint8_t)s.sb}, {"sr", (uint8_t)s.sr}, {"mp", (uint8_t)s.mp},
        {"rstk", s.rstk},
        {"crc", s.crc}
    };
}

void from_json(const nlohmann::json& j, SaturnState& s) {
    j.at("a").get_to(s.a);
    j.at("b").get_to(s.b);
    j.at("c").get_to(s.c);
    j.at("d").get_to(s.d);
    j.at("r0").get_to(s.r0);
    j.at("r1").get_to(s.r1);
    j.at("r2").get_to(s.r2);
    j.at("r3").get_to(s.r3);
    j.at("r4").get_to(s.r4);
    j.at("d0").get_to(s.d0);
    j.at("d1").get_to(s.d1);
    j.at("pc").get_to(s.pc);
    j.at("p").get_to(s.p);
    j.at("carry").get_to(s.carry);
    j.at("hexMode").get_to(s.hexMode);
    j.at("pstat").get_to(s.pstat);
    s.xm = j.at("xm").get<uint8_t>();
    s.sb = j.at("sb").get<uint8_t>();
    s.sr = j.at("sr").get<uint8_t>();
    s.mp = j.at("mp").get<uint8_t>();
    j.at("rstk").get_to(s.rstk);
    j.at("crc").get_to(s.crc);
}

void from_json(const nlohmann::ordered_json& j, SaturnState& s) {
    from_json(static_cast<const nlohmann::json&>(j), s);
}

bool SaturnState::compare(const SaturnState& other, const SaturnState& initial, std::ostream& errorStream) const {
    bool match = true;
    auto report = [&](const char* field, auto v1, auto v2, auto v3) {
        if (v1 != v2) {
            errorStream << fmt::format("Mismatch in {}: expected {:X}, got {:X}, initial: {:X}\n", field, v2, v1, v3);
            match = false;
        }
    };

    report("a", a, other.a, initial.a);
    report("b", b, other.b, initial.b);
    report("c", c, other.c, initial.c);
    report("d", d, other.d, initial.d);
    report("r0", r0, other.r0, initial.r0);
    report("r1", r1, other.r1, initial.r1);
    report("r2", r2, other.r2, initial.r2);
    report("r3", r3, other.r3, initial.r3);
    report("r4", r4, other.r4, initial.r4);
    report("d0", d0, other.d0, initial.d0);
    report("d1", d1, other.d1, initial.d1);
    report("pc", pc, other.pc, initial.pc);
    report("p", (int)p, (int)other.p, (int)initial.p);
    report("carry", carry, other.carry, initial.carry);
    report("hexMode", hexMode, other.hexMode, initial.hexMode);
    report("pstat", pstat, other.pstat, initial.pstat);
    report("xm", (int)xm, (int)other.xm, (int)initial.xm);
    report("sb", (int)sb, (int)other.sb, (int)initial.sb);
    report("sr", (int)sr, (int)other.sr, (int)initial.sr);
    report("mp", (int)mp, (int)other.mp, (int)initial.mp);
    //report("crc", crc, other.crc, initial.crc);

    for (size_t i = 0; i < rstk.size(); ++i) {
        if (rstk[i] != other.rstk[i]) {
            errorStream << fmt::format("Mismatch in rstk[{}]: expected {}, got {}\n", i, other.rstk[i], rstk[i], initial.rstk[i]);
            match = false;
        }
    }

    return match;
}

SaturnState SaturnState::random(std::mt19937_64& gen) {
    std::uniform_int_distribution<uint64_t> dist64;
    std::uniform_int_distribution<uint32_t> dist20(1, 0xFFFFF);
    std::uniform_int_distribution<uint8_t> dist4(0, 0xF);
    std::uniform_int_distribution<uint8_t> dist1(0, 1);
    std::uniform_int_distribution<uint16_t> dist16;
    std::uniform_int_distribution<int16_t> distStackP(0, 7);

    SaturnState s;
    s.a = dist64(gen);
    s.b = dist64(gen);
    s.c = dist64(gen);
    s.d = dist64(gen);
    s.r0 = dist64(gen);
    s.r1 = dist64(gen);
    s.r2 = dist64(gen);
    s.r3 = dist64(gen);
    s.r4 = dist64(gen);
    s.d0 = dist20(gen);
    s.d1 = dist20(gen);
    s.pc = dist20(gen);
    s.p = dist4(gen);
    s.carry = dist1(gen);
    s.hexMode = dist1(gen);
    s.pstat = dist16(gen);
    s.xm = dist1(gen);
    s.sb = dist1(gen);
    s.sr = dist1(gen);
    s.mp = dist1(gen);
    s.crc = dist16(gen);
    auto stackP = distStackP(gen);
    s.rstk.fill(0);
    for (size_t i = 0; i < stackP; ++i)
        s.rstk[i] = dist20(gen);
    return s;
}

} // namespace hp48
