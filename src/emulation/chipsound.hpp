//---------------------------------------------------------------------------------------
// src/emulation/chipsound.hpp
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
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace emu {

class ChipSound
{
public:
    constexpr static const float fPi = 3.1415926535f;

    enum Waveform { eNONE, eSINE, ePULSE, eSAW, eNOISE, eAAPULSE, eAASQUARE, eAASAW };
    enum EnvelopeState { eIDLE, eATTACK, eDECAY, eSUSTAIN, eRELEASE };

    struct VoiceInfo
    {
        uint8_t tone;           // 0
        uint8_t pulsewidth;     // 1
        uint8_t waveform : 3;   // 2
        uint8_t control : 5;    //
        uint8_t attack : 4;     // 3
        uint8_t decay : 4;      //
        uint8_t sustain : 4;    // 4
        uint8_t release : 4;    //
        uint8_t cutoff;         // 5
        uint8_t filter : 4;     // 6
        uint8_t resonance : 4;  //
        //----
        float _frequency{0};
        float _sampleLength{0};
        float _phase{0};
        int _noiseAcc{0};
        //----
        int _envState{eIDLE};
        int _envAttackSteps;
        int _envDecaySteps;
        int _envReleaseSteps;
        float _envAttackDelta;
        float _envDecayDelta;
        float _envReleaseDelta;
        float _envStepTime;
        int _envStep{0};
        bool _noteOnEvent{false};
        bool _noteOffEvent{false};
    };

    ChipSound()
    {
        int noise = 0x7ffff8L;
        for (int i = 0; i < 0x10000; ++i) {
            noise = (noise * 196314165) + 907633515;
            _noiseBuffer[i] = (short)noise;
        }
    }

    inline float envelopeTime(uint8_t ti) { return std::clamp(std::pow(2.0f, float(ti) / 1.5f - 6.0f) / 2, 0.002f, 8.0f); }
    void updateParameters(uint8_t voiceId, const uint8_t* data)
    {
        VoiceInfo& vi = _voice[voiceId & 3];
        vi.tone = *data++;
        vi.pulsewidth = *data++;
        vi.waveform = *data >> 5;
        vi.control = *data++ & 0x1F;
        vi.attack = *data >> 4;
        vi.decay = *data++ & 0xF;
        vi.sustain = *data >> 4;
        vi.release = *data++ & 0xF;
        vi.cutoff = *data++;
        vi.filter = *data >> 4;
        vi.resonance = *data++ & 0xF;

        // init oscillator
        vi._frequency = 440;
        vi._phase = 0;
        vi._sampleLength = vi._frequency * _stepTime;

        // init adsr
        float temp = envelopeTime(vi.attack) / _stepTime;
        vi._envAttackSteps = (int)temp;
        vi._envAttackDelta = (float)(1.0f / temp);

        temp = envelopeTime(vi.decay) * 3 / _stepTime;
        vi._envDecaySteps = (int)temp;
        vi._envDecayDelta = (1.0f - vi.sustain) / temp;

        temp = envelopeTime(vi.release) * 3 / _stepTime;
        vi._envReleaseSteps = (int)temp;
        vi._envReleaseDelta = vi.sustain / temp;
        vi._noteOnEvent = true;
    }

    float envelopeStep(VoiceInfo& vi)
    {
        float value = 0.0f;
        switch (vi._envState) {
            case eIDLE:
                vi._noteOffEvent = false;
                if (vi._noteOnEvent)
                    vi._noteOnEvent = false, vi._envStep = 0, vi._envState = eATTACK;
                break;
            case eATTACK:
                value = vi._envStep * vi._envAttackDelta;
                if (vi._noteOnEvent)
                    value /= 2.0f, vi._envState = eIDLE;
                else if (vi._envStep >= vi._envAttackSteps)
                    vi._envStep = 0, vi._envState = eDECAY;
                else
                    ++vi._envStep;
                break;
            case eDECAY:
                value = 1.0f - (vi._envStep * vi._envDecayDelta);
                if (vi._noteOnEvent)
                    vi._envState = eIDLE, value /= 2.0f;
                else if (vi._envStep >= vi._envDecaySteps)
                    vi._envState = eSUSTAIN;
                else
                    ++vi._envStep;
                break;
            case eSUSTAIN:
                value = float(vi.sustain) / 15.0f;
                if (vi._noteOnEvent)
                    vi._envState = eIDLE, value /= 2.0f;
                else if (vi._noteOffEvent)
                    vi._noteOffEvent = false, vi._envStep = 0, vi._envState = eRELEASE;
                break;
            case eRELEASE:
                value = float(vi.sustain) / 15.0f - (vi._envStep * vi._envReleaseDelta);
                if (vi._noteOnEvent)
                    vi._envState = eIDLE, value /= 2.0f;
                else if (value < 0)
                    value = 0;
                if (vi._envStep >= vi._envReleaseSteps)
                    vi._envState = eIDLE;
                else
                    ++vi._envStep;
                break;
        }
        return value;
    }

    void nextSample()
    {
        float sample = 0;

        for (int i = 0; i < 4; ++i) {
            if (_voice[i]._frequency > 0.1f) {
                _voice[i]._noiseAcc = (_voice[i]._noiseAcc + (int)(_voice[i]._frequency)) & 0xfffffff;
                float val = waveformFunction(_voice[i]) * envelopeStep(_voice[i]);

                _voice[i]._phase += _voice[i]._sampleLength;
                if (_voice[i]._phase >= 1.0f) {
                    _voice[i]._phase -= 1.0f;
                }

                sample += val / 2.0f;
            }
        }
        _sample = int16_t(std::clamp(sample, -1.0f, 1.0f) * 32767.0f);
    }

    int16_t sample() const;

    float waveformFunction(VoiceInfo& vi)
    {
        switch (vi.waveform) {
            case eNONE:
                return 0;
            case eSINE:
                return std::sin(2.0f * fPi * vi._phase);
            case ePULSE:
                return vi._phase <= (float(vi.pulsewidth) / 256) ? 1.0f : -1.0f;
            case eSAW:
                return 2.0f * (vi._phase - std::floor(vi._phase + 0.5f));
            case eNOISE:
                return (float)_noiseBuffer[(vi._noiseAcc >> 12) & 0xffff] / 32768.0f;
#ifdef WITH_WAVETABLES
            case eAAPULSE: {
                if (std::fabs(_params->pulseWidth - 0.5f) < 0.001) {
                    val = _waveTables[eAASQUARE]->valueForPhase(_frequency, _phase);
                }
                else {
                    // TODO: This still creates DC offsets
                    float offset = _phase + _params->pulseWidth;
                    if (offset >= 1.0) {
                        offset -= 1.0;
                    }
                    val = _waveTables[eAASAW]->valueForPhase(_frequency, _phase) - _waveTables[eAASAW]->valueForPhase(_frequency, offset);
                }
                break;
            }
            case eAASQUARE:
                val = _waveTables[eWTSQUARE]->valueForPhase(_frequency, _phase);
                break;
            case eAASAW:
                val = _waveTables[eWTSAW]->valueForPhase(_frequency, _phase);
                break;
            case eAATRIANGLE:
                val = _waveTables[eWTTRIANGLE]->valueForPhase(_frequency, _phase);
                break;
#endif
            default:
                return 0.0f;
        }
    }

    const float _stepTime = 1.0f / 44100;
    VoiceInfo _voice[4];
    int16_t _sample;
    inline static short _noiseBuffer[0x10000];
};

}  // namespace emu