//---------------------------------------------------------------------------------------
// src/emulation/filter.hpp
//---------------------------------------------------------------------------------------
//
// Copyright (c) 2025, Steffen Schümann <s.schuemann@pobox.com>
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
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>

class HighPassOnePole
{
public:
    HighPassOnePole(uint32_t sampleRateHz, uint32_t cutoffHz)
    {
        setFrequencies(sampleRateHz, cutoffHz);
        reset();
    }

    void setFrequencies(double sampleRate, double cutoffHz)
    {
        _sampleRate = sampleRate;
        _cutoffHz   = cutoffHz;

        const double pi   = std::numbers::pi_v<double>;
        const double theta = pi * _cutoffHz / _sampleRate;
        const double c     = std::tan(theta);

        // High-pass (1st order) – RBJ-style one-pole
        const double a0Inv = 1.0 / (1.0 + c);
        _a0 =  a0Inv;
        _a1 = -a0Inv;
        _b1 = (1.0 - c) * a0Inv;
    }

    void reset()
    {
        _x1 = 0.0;
        _y1 = 0.0;
    }

    int16_t operator()(int16_t sample)
    {
        const double x = static_cast<double>(sample);
        const double y = _a0 * x + _a1 * _x1 + _b1 * _y1;

        _x1 = x;
        _y1 = y;

        long yi = std::lround(y);
        yi = std::clamp<long>(yi, static_cast<long>(std::numeric_limits<int16_t>::min()), static_cast<long>(std::numeric_limits<int16_t>::max()));
        return static_cast<int16_t>(yi);
    }

private:
    double _sampleRate {44100.0};
    double _cutoffHz   {100.0};

    // Direct Form I coefficients: y[n] = a0*x[n] + a1*x[n-1] + b1*y[n-1]
    double _a0 {0.0};
    double _a1 {0.0};
    double _b1 {0.0};

    double _x1 {0.0};
    double _y1 {0.0};
};

class ButterworthFilter
{
public:
    enum class Mode { LOW_PASS, HIGH_PASS };
    ButterworthFilter(uint32_t sampleRateHz, uint32_t cutoffFrequencyHz, Mode mode)
    {
        setFrequencies(sampleRateHz, cutoffFrequencyHz, mode);
    }

    void setFrequencies(double sampleRateHz, double cutoffFrequencyHz, Mode mode)
    {
        _fs = sampleRateHz;
        _fc = cutoffFrequencyHz;
        _mode = mode;

        if (_fs <= 0.0 || _fc <= 0.0) {
            _b0 = 1.0;
            _b1 = 0.0;
            _b2 = 0.0;
            _a1 = 0.0;
            _a2 = 0.0;
            return;
        }

        const auto nyquist = _fs * 0.5;
        const auto minFc = mode == Mode::LOW_PASS ? 5.0 : 0.1;  // arbitrary, just to avoid degenerate filters
        const auto maxFc = nyquist * 0.9;
        _fc = std::clamp(_fc, minFc, maxFc);

        initButterworth(_fc, mode);
    }

    int16_t operator()(int16_t sample)
    {
        const double x = static_cast<double>(sample);
        const double y = _b0 * x + _b1 * _x1 + _b2 * _x2 - _a1 * _y1 - _a2 * _y2;

        _x2 = _x1;
        _x1 = x;
        _y2 = _y1;
        _y1 = y;

        long yi = std::lround(y);
        yi = std::clamp<long>(yi, static_cast<long>(std::numeric_limits<int16_t>::min()), static_cast<long>(std::numeric_limits<int16_t>::max()));
        return static_cast<int16_t>(yi);
    }

    void reset()
    {
        _x1 = _x2 = 0.0;
        _y1 = _y2 = 0.0;
    }

    void reset(int16_t steadyStateSample)
    {
        const double x = static_cast<double>(steadyStateSample);
        _x1 = _x2 = x;
        _y1 = _y2 = _mode == Mode::LOW_PASS ? x : 0.0;
    }

    void initButterworth(double cutoffHz, Mode mode)
    {
        const auto omega = 2.0 * std::numbers::pi_v<double> * cutoffHz / _fs;
        const auto sinOmega = std::sin(omega);
        const auto cosOmega = std::cos(omega);
        const auto alpha = sinOmega / (2.0 * std::sqrt(0.5));
        _a0 = 1.0 + alpha;
        double b0, b1, b2;
        if (mode == Mode::LOW_PASS) {
            // 2nd-order Butterworth low-pass design via bilinear transform.
            // |H(0)| = 1, -3 dB at cutoff.
            b0 = (1.0 - cosOmega) * 0.5;
            b1 = 1.0 - cosOmega;
            b2 = (1.0 - cosOmega) * 0.5;
        }
        else {
            // 2nd-order Butterworth high-pass (RBJ cookbook style).
            b0 = (1.0 + cosOmega) * 0.5;
            b1 = -(1.0 + cosOmega);
            b2 = (1.0 + cosOmega) * 0.5;
        }
        auto a1 = -2.0 * cosOmega;
        auto a2 = 1.0 - alpha;
        _b0 = b0 / _a0;
        _b1 = b1 / _a0;
        _b2 = b2 / _a0;
        _a1 = a1 / _a0;
        _a2 = a2 / _a0;
    }

    double _fs = 1.0;
    double _fc = 1.0;
    Mode _mode = Mode::LOW_PASS;

    // biquad coefficients
    double _b0 = 1.0, _b1 = 0.0, _b2 = 0.0;
    double _a0 = 0.0, _a1 = 0.0, _a2 = 0.0;

    // state
    double _x1 = 0.0, _x2 = 0.0;
    double _y1 = 0.0, _y2 = 0.0;
};

class LowPassButterworthFilter : public ButterworthFilter
{
public:
    LowPassButterworthFilter(uint32_t sampleRateHz, uint32_t cutoffFrequencyHz) : ButterworthFilter(sampleRateHz, cutoffFrequencyHz, Mode::LOW_PASS) {}
};

class HighPassButterworthFilter : public ButterworthFilter
{
public:
    HighPassButterworthFilter(uint32_t sampleRateHz, uint32_t cutoffFrequencyHz) : ButterworthFilter(sampleRateHz, cutoffFrequencyHz, Mode::HIGH_PASS) {}
};


class SoftClipper
{
public:
    explicit SoftClipper(double drive)
    {
        setDrive(drive);
    }

    void setDrive(double drive)
    {
        _drive = drive;
        _scale = std::tanh(_drive);
        if (_scale == 0.0) {
            _scale = 1.0; // avoid division by zero for drive == 0
        }
    }

    int16_t operator()(int16_t sample)
    {
        if (_drive == 0.0) {
            return sample;
        }
        constexpr double invMax = 1.0 / 32768.0;
        double x = static_cast<double>(sample) * invMax;
        x = std::clamp(x, -1.0, 1.0);
        double y = std::tanh(_drive * x) / _scale;
        double scaled = y * 32767.0;
        long rounded = std::lround(scaled);
        if (rounded > 32767) {
            rounded = 32767;
        } else if (rounded < -32768) {
            rounded = -32768;
        }
        return static_cast<int16_t>(rounded);
    }

private:
    double _drive{1.5};
    double _scale{std::tanh(1.5)};
};


#if 0
class LowPassFilter
{
public:
    LowPassFilter(uint32_t sampleRateHz, uint32_t dataFrequencyHz, double marginFactor = 1.3) { setFrequencies(sampleRateHz, dataFrequencyHz, marginFactor); }

    // You can call this if you ever change the data rate
    void setFrequencies(double sampleRateHz, double dataFrequencyHz, double marginFactor = 2.0)
    {
        _fs = sampleRateHz;
        _fd = dataFrequencyHz;

        if (_fs <= 0.0 || _fd <= 0.0) {
            // Fallback: passthrough
            _b0 = 1.0;
            _b1 = 0.0;
            _b2 = 0.0;
            _a1 = 0.0;
            _a2 = 0.0;
            return;
        }

        // Cutoff slightly above data frequency so the fundamental is not
        // significantly attenuated, but high-frequency noise is.
        auto fc = _fd * marginFactor;

        // Clamp cutoff to a safe range relative to Nyquist
        const auto nyquist = _fs * 0.5;
        const auto minFc = 5.0;  // arbitrary, just to avoid degenerate filters
        const auto maxFc = nyquist * 0.9;
        fc = std::clamp(fc, minFc, maxFc);

        initButterworthLowpass(fc);
    }

    // Process one sample
    int16_t operator()(int16_t sample)
    {
        const double x = static_cast<double>(sample);

        // Direct Form I / II-style biquad
        const double y = _b0 * x + _b1 * _x1 + _b2 * _x2 - _a1 * _y1 - _a2 * _y2;

        // shift state
        _x2 = _x1;
        _x1 = x;
        _y2 = _y1;
        _y1 = y;

        long yi = std::lround(y);
        yi = std::clamp<long>(yi, static_cast<long>(std::numeric_limits<int16_t>::min()), static_cast<long>(std::numeric_limits<int16_t>::max()));
        return static_cast<int16_t>(yi);
    }

    // Optional: reset filter history (e.g. between files)
    void reset()
    {
        _x1 = _x2 = 0.0;
        _y1 = _y2 = 0.0;
    }

private:
    void initButterworthLowpass(double cutoffHz)
    {
        // 2nd-order Butterworth low-pass design via bilinear transform.
        // |H(0)| = 1, -3 dB at cutoff.
        // const auto Q = std::sqrt(0.5);  // 1/sqrt(2)
        const auto omega = 2.0 * std::numbers::pi_v<double> * cutoffHz / _fs;
        const auto sinOmega = std::sin(omega);
        const auto cosOmega = std::cos(omega);
        const auto alpha = sinOmega / (2.0 * std::sqrt(0.5));
        auto a0 = 1.0 + alpha;
        auto b0 = (1.0 - cosOmega) * 0.5;
        auto b1 = 1.0 - cosOmega;
        auto b2 = (1.0 - cosOmega) * 0.5;
        auto a1 = -2.0 * cosOmega;
        auto a2 = 1.0 - alpha;
        _b0 = b0 / a0;
        _b1 = b1 / a0;
        _b2 = b2 / a0;
        _a1 = a1 / a0;
        _a2 = a2 / a0;
    }

    // sample rate and data frequency (for reference)
    double _fs = 1.0;
    double _fd = 1.0;

    // biquad coefficients
    double _b0 = 1.0, _b1 = 0.0, _b2 = 0.0;
    double _a1 = 0.0, _a2 = 0.0;

    // state
    double _x1 = 0.0, _x2 = 0.0;
    double _y1 = 0.0, _y2 = 0.0;
};

class HighPassFilter
{
public:
    HighPassFilter(uint32_t sampleRateHz, uint32_t dataFrequencyHz, double marginFactor = 2.0) { setFrequencies(sampleRateHz, dataFrequencyHz, marginFactor); }

    // You can call this if you ever change the data rate
    void setFrequencies(double sampleRateHz, double dataFrequencyHz, double marginFactor = 1.5)
    {
        _fs = sampleRateHz;
        _fd = dataFrequencyHz;

        if (_fs <= 0.0 || _fd <= 0.0) {
            // Fallback: passthrough
            _b0 = 1.0;
            _b1 = 0.0;
            _b2 = 0.0;
            _a1 = 0.0;
            _a2 = 0.0;
            return;
        }

        // Here dataFrequencyHz is the LOWEST useful data freq.
        // We pick a cutoff slightly BELOW it so the data band is not
        // significantly attenuated, but DC / slow drift is removed.
        //
        // Example: marginFactor = 1.3 => fc = fd / 1.3
        auto fc = _fd / marginFactor;

        // Clamp cutoff to a safe range relative to Nyquist
        const auto nyquist = _fs * 0.5;
        const auto minFc = 0.1;  // avoid degenerate near-DC filters
        const auto maxFc = nyquist * 0.9;
        fc = std::clamp(fc, minFc, maxFc);

        initButterworthHighpass(fc);
    }

    // Process one sample
    int16_t operator()(int16_t sample)
    {
        const double x = static_cast<double>(sample);

        const double y = _b0 * x + _b1 * _x1 + _b2 * _x2 - _a1 * _y1 - _a2 * _y2;

        _x2 = _x1;
        _x1 = x;
        _y2 = _y1;
        _y1 = y;

        long yi = std::lround(y);
        yi = std::clamp<long>(yi, static_cast<long>(std::numeric_limits<int16_t>::min()), static_cast<long>(std::numeric_limits<int16_t>::max()));
        return static_cast<int16_t>(yi);
    }

    void reset()
    {
        _x1 = _x2 = 0.0;
        _y1 = _y2 = 0.0;
    }

private:
    void initButterworthHighpass(double cutoffHz)
    {
        // 2nd-order Butterworth high-pass (RBJ cookbook style).
        // constexpr auto Q = std::sqrt(0.5);  // 1/sqrt(2)
        const auto omega = 2.0 * std::numbers::pi_v<double> * cutoffHz / _fs;
        const auto sinOmega = std::sin(omega);
        const auto cosOmega = std::cos(omega);
        const auto alpha = sinOmega / (2.0 * std::sqrt(0.5));
        auto a0 = 1.0 + alpha;
        auto b0 = (1.0 + cosOmega) * 0.5;
        auto b1 = -(1.0 + cosOmega);
        auto b2 = (1.0 + cosOmega) * 0.5;
        auto a1 = -2.0 * cosOmega;
        auto a2 = 1.0 - alpha;
        _b0 = b0 / a0;
        _b1 = b1 / a0;
        _b2 = b2 / a0;
        _a1 = a1 / a0;
        _a2 = a2 / a0;
    }

    // sample rate and data frequency (for reference)
    double _fs = 1.0;
    double _fd = 1.0;

    // biquad coefficients
    double _b0 = 1.0, _b1 = 0.0, _b2 = 0.0;
    double _a1 = 0.0, _a2 = 0.0;

    // state
    double _x1 = 0.0, _x2 = 0.0;
    double _y1 = 0.0, _y2 = 0.0;
};

#endif
