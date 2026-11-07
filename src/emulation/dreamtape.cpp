//---------------------------------------------------------------------------------------
// src/emulation/dreamtape.cpp
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

#include "dreamtape.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace emu {
namespace {
struct ToneRun
{
    int tone;
    double start;
    double end;
};
}

DreamTapeImage decodeDreamTape(std::span<const int16_t> samples, uint32_t sampleRate,
                               std::optional<double> baud)
{
    if (sampleRate < 8000 || samples.empty())
        throw std::runtime_error("Audio is empty or its sample rate is below 8000 Hz");
    if (baud && (!std::isfinite(*baud) || *baud < 270 || *baud > 330))
        throw std::runtime_error("Baud must be between 270 and 330");
    int peak = 0;
    for (auto value : samples)
        peak = std::max(peak, std::abs(static_cast<int>(value)));
    if (peak < 32)
        throw std::runtime_error("No cassette carrier found");
    const double threshold = peak * 0.05;
    std::vector<double> times;
    std::vector<int> tones;
    std::optional<double> previous;
    int cyclePeak = 0;
    // Classify full periods at rising zero crossings. Timestamp each at its
    // centre; interpolation reduces quantization at lower WAV sample rates.
    for (size_t index = 1; index < samples.size(); ++index) {
        cyclePeak = std::max(cyclePeak, std::abs(static_cast<int>(samples[index])));
        if (samples[index - 1] <= 0 && samples[index] > 0) {
            const double crossing = index - static_cast<double>(samples[index]) / (samples[index] - samples[index - 1]);
            if (previous) {
                const double period = crossing - *previous;
                int tone = -1; // Silence, low-level noise, or out-of-band signal.
                if (cyclePeak >= threshold && sampleRate / 3500.0 < period && period < sampleRate / 800.0)
                    tone = period < sampleRate / 1700.0 ? 1 : 0;
                times.push_back((crossing + *previous) / 2);
                tones.push_back(tone);
            }
            previous = crossing;
            cyclePeak = 0;
        }
    }
    if (tones.size() < 20)
        throw std::runtime_error("No cassette carrier found");

    std::vector<ToneRun> runs;
    double start = times.front();
    int tone = tones.front();
    for (size_t index = 1; index < tones.size(); ++index) {
        if (tones[index] != tone) {
            const double end = (times[index - 1] + times[index]) / 2;
            runs.push_back({tone, start, end});
            start = end;
            tone = tones[index];
        }
    }
    runs.push_back({tone, start, times.back()});

    if (!baud) {
        // Pair low and high runs: the discriminator's opposite transition
        // delays cancel. Exclude leader, silence and implausibly short runs.
        std::vector<double> periods;
        const double nominal = sampleRate / 300.0;
        for (size_t index = 1; index < runs.size(); ++index) {
            const auto& first = runs[index - 1];
            const auto& second = runs[index];
            const double length = second.end - first.start;
            if (first.tone >= 0 && second.tone >= 0 && nominal * 1.5 < length && length < nominal * 9)
                periods.push_back(length / std::round(length / nominal));
        }
        if (periods.empty())
            throw std::runtime_error("No cassette data found after carrier");
        std::sort(periods.begin(), periods.end());
        const auto middle = periods.size() / 2;
        const double median = periods.size() % 2 ? periods[middle] : (periods[middle - 1] + periods[middle]) / 2;
        baud = sampleRate / median;
        if (*baud < 270 || *baud > 330)
            throw std::runtime_error("Implausible cassette speed: " + std::to_string(*baud));
    }
    const double bitPeriod = sampleRate / *baud;
    const auto bitAt = [&](double center) {
        const auto lo = std::lower_bound(times.begin(), times.end(), center - bitPeriod * 0.20);
        const auto hi = std::upper_bound(times.begin(), times.end(), center + bitPeriod * 0.20);
        if (lo == hi)
            return -1;
        int sum = 0;
        for (auto index = lo - times.begin(); index < hi - times.begin(); ++index) {
            if (tones[index] < 0)
                return -1;
            sum += tones[index];
        }
        return sum * 2 >= hi - lo ? 1 : 0;
    };

    DreamTapeImage result{{}, *baud};
    double nextStart = -1;
    bool leader = false;
    for (size_t index = 1; index < runs.size(); ++index) {
        const auto& previousRun = runs[index - 1];
        const auto& current = runs[index];
        if (previousRun.tone == 1 && previousRun.end - previousRun.start >= bitPeriod * 30)
            leader = true;
        if (!leader || previousRun.tone != 1 || current.tone != 0)
            continue;
        start = current.start;
        if (start < nextStart)
            continue;
        if (bitAt(start - bitPeriod * 0.5) != 1 || bitAt(start + bitPeriod * 0.5) != 0)
            continue;
        uint8_t value = 0;
        for (int bit = 0; bit < 9; ++bit) {
            const int decoded = bitAt(start + bitPeriod * (1.5 + bit));
            if (decoded < 0 || (bit == 8 && decoded != 1))
                throw std::runtime_error("Invalid or truncated byte " + std::to_string(result.data.size())
                                         + " at " + std::to_string(start / sampleRate) + "s");
            if (bit < 8)
                value |= decoded << bit;
        }
        result.data.push_back(value);
        // Sample each asynchronous byte independently: drift cannot accumulate.
        nextStart = start + bitPeriod * 9.6;
    }
    if (result.data.empty())
        throw std::runtime_error("No serial bytes found after a 2400 Hz leader");
    return result;
}

}
