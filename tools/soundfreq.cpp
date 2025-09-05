//---------------------------------------------------------------------------------------
// src/soundfreq.cpp
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
#include <iostream>
#include <iomanip>
#include <cmath>

float closestNote(float freq) {
    float bestFit = 0;
    for(int i = -60; i <= 60; ++i) {
        float tone = 440.0f * std::pow(1.059463094359f, i);
        if(std::fabs(tone - freq) < std::fabs(bestFit - freq)) {
            bestFit = tone;
        }
    }
    return bestFit;
}

std::pair<std::string, float> closestNoteX(float freq) {
    if (!(freq > 0.0f) || std::isnan(freq)) {
        throw std::invalid_argument("Frequency must be a positive, finite number.");
    }

    // Equal-tempered semitone distance from A4
    const double n = 12.0 * std::log2(static_cast<double>(freq) / 440.0);

    // Nearest semitone (integer) relative to A4
    const long nearest = std::lround(n);

    // Convert to MIDI note number (A4 = 440 Hz = MIDI 69)
    const int midi = static_cast<int>(69 + nearest);

    // Note names (use sharps)
    static constexpr std::array<const char*, 12> names{
        "C-","C#","D-","D#","E-","F-","F#","G-","G#","A-","A#","B-"
    };

    // Index within octave [0..11], handle negatives safely
    const int note_index = ( (midi % 12) + 12 ) % 12;

    // MIDI octave: C-1 = 0 -> octave = floor(midi/12) - 1
    const int octave = static_cast<int>(std::floor(midi / 12.0)) - 1;

    std::string note = std::string(names[note_index]) + std::to_string(octave);

    // Fractional difference in semitones from the returned note
    const float frac = static_cast<float>(n - nearest);

    return { note, frac };
}

int main()
{
    for(int i = 0; i < 256; ++i) {
        auto freq = 4000 * std::pow(2.0f, (i - 64) / 48.0f) / 128;
        std::cout << std::setw(3) << i << ": " << std::fixed << std::setprecision(2);
        auto [note, frac] = closestNoteX(freq);
        std::cout << std::left << std::setw(4) << note << std::right << " (" << std::setw(5) << frac << "), ";
        auto [note2, frac2] = closestNoteX(freq * 2);
        std::cout << std::left << std::setw(4) << note2 << std::right << " (" << std::setw(5) << frac2 << "), ";
        auto [note3, frac3] = closestNoteX(freq * 4);
        std::cout << std::left << std::setw(4) << note3 << std::right << " (" << std::setw(5) << frac3 << ") ";
        std::cout << std::setprecision(4) << (freq * 128) << " " << (freq / 44100.0) << " " << (freq / 48000.0) << std::endl;
        // std::cout << i << " : " << freq << " Hz - (" << closestNote(freq) << "Hz, " << closestNote(freq*2) << "Hz, " << closestNote(freq*4) << "Hz)" << std::endl;
    }
}