//---------------------------------------------------------------------------------------
// src/emulation/time.hpp
//---------------------------------------------------------------------------------------
//
// Copyright (c) 2015, Steffen Schümann <s.schuemann@pobox.com>
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

#include <emulation/config.hpp>
#include <emulation/math.hpp>

namespace emu {

class Time
{
public:
    using seconds_t = uint32_t;
    using ticks_t = uint64_t;
    static constexpr int subsecondBits = 48;
    static constexpr seconds_t maxSeconds = 1L << 30;                // about 34 years
    static constexpr ticks_t ticksPerSecond = 1LL << subsecondBits;  // 1 tick is about 3.5 femtoseconds

    Time()
        : _seconds(0)
        , _ticks(0)
    {
    }
    Time(seconds_t sec, ticks_t ticks)
        : _seconds(sec)
        , _ticks(ticks)
    {
        normalize();
    }
    explicit Time(double seconds)
        : _seconds(seconds_t(seconds))
        , _ticks(ticks_t((seconds - _seconds) * ticksPerSecond + 0.5))
    {
    }

    bool isZero() const { return _seconds == 0 && _ticks == 0; }

    bool isNever() const { return _seconds >= maxSeconds; }

    seconds_t seconds() const { return _seconds; }
    seconds_t secondsRounded() const { return _seconds + (_ticks >= (ticksPerSecond >> 1) ? 1 : 0); }
    ticks_t ticks() const { return _ticks; }

    double asSeconds() const { return double(_seconds) + double(_ticks) / ticksPerSecond; }

    void addCycles(cycles_t cycles, uint32_t frequency);
    cycles_t asClockTicks(uint32_t frequency) const;
    int64_t differenceInClockTicks(const Time& other, uint32_t frequency) const;

    const Time& operator+=(const Time& other)
    {
        _seconds += other._seconds;
        _ticks += other._ticks;
        if (_ticks > ticksPerSecond) {
            normalize();
        }
        return *this;
    }

    virtual Time operator+(const Time& other)
    {
        Time result(*this);
        result += other;
        return result;
    }

    virtual Time& operator*=(uint32_t factor)
    {
        uint32_t ticklo, tickhi, reslo, reshi;
        tickhi = math::split64(_ticks, ticklo);
        uint64_t temp = math::mulu32by32(ticklo, factor);
        temp = math::split64(temp, reslo);
        temp += math::mulu32by32(tickhi, factor);
        temp = math::split64(temp, reshi);
        _ticks = math::combineToUint64(reshi, reslo);
        temp += math::mulu32by32(_seconds, factor);
        if (temp >= maxSeconds) {
            _seconds = maxSeconds;
            _ticks = 0;
        }
        else {
            _seconds = uint32_t(temp << (64 - subsecondBits));
            normalize();
        }
        return *this;
    }

    void normalize()
    {
        if (_ticks >= ticksPerSecond) {
            _seconds += ticksInSeconds(_ticks);
            _ticks = _ticks & (ticksPerSecond - 1);
            if (_seconds >= maxSeconds) {
                _seconds = maxSeconds;
                _ticks = 0;
            }
        }
    }

    bool operator<(const Time& other) const { return std::tie(_seconds, _ticks) < std::tie(other._seconds, other._ticks); }

    bool operator>=(const Time& other) const { return !(*this < other); }

    bool operator>(const Time& other) const { return other < *this; }

    bool operator<=(const Time& other) const { return other >= *this; }

    bool operator==(const Time& other) const { return std::tie(_seconds, _ticks) == std::tie(other._seconds, other._ticks); }

    bool operator!=(const Time& other) const { return std::tie(_seconds, _ticks) != std::tie(other._seconds, other._ticks); }

    std::string asString() const;

    static Time fromSeconds(double seconds) { return Time(seconds); }
    static Time fromMicroseconds(uint64_t microseconds) { return Time(microseconds/1000000, (microseconds%1000000) * (ticksPerSecond/1000000)); }

    static Time fromCycles(cycles_t cycles, uint32_t frequency)
    {
        ticks_t ticksPerCycle = ticks_t(ticksPerSecond) / frequency + (ticks_t(ticksPerSecond) % frequency != 0);
        if (cycles < frequency) {
            return Time(0, cycles * ticksPerCycle);
        }
        uint32_t remainder;
        seconds_t secs = seconds_t(math::divu64by32(cycles, frequency, remainder));
        return Time(secs, uint64_t(remainder) * ticksPerSecond);
    }

    static seconds_t ticksInSeconds(ticks_t ticks) { return seconds_t(ticks >> subsecondBits); }

    static const Time zero;
    static const Time never;

private:
    seconds_t _seconds;
    ticks_t _ticks;
};

inline Time operator*(const Time& left, uint32_t factor)
{
    Time result = left;
    result *= factor;
    return result;
}

inline Time operator*(uint32_t factor, const Time& right)
{
    Time result = right;
    result *= factor;
    return result;
}

inline void Time::addCycles(cycles_t cycles, uint32_t frequency)
{
    *this += Time::fromCycles(cycles, frequency);
}

inline cycles_t Time::asClockTicks(uint32_t frequency) const
{
    uint32_t fraction = (Time(0, _ticks) * frequency).secondsRounded();
    return cycles_t(math::mulu32by32(_seconds, frequency) + fraction);
}

inline int64_t Time::differenceInClockTicks(const Time& other, uint32_t frequency) const
{
    seconds_t diffSeconds = 0;
    ticks_t diffTicks = 0;
    if (*this < other) {
        diffTicks = (other._ticks - _ticks) & (ticksPerSecond - 1);
        diffSeconds = other._seconds - _seconds - (diffTicks > other._ticks ? 1 : 0);
        return -(int64_t)(Time(diffSeconds, diffTicks).asClockTicks(frequency));
    }
    else {
        diffTicks = (_ticks - other._ticks) & (ticksPerSecond - 1);
        diffSeconds = _seconds - other._seconds - (diffTicks > _ticks ? 1 : 0);
        return Time(diffSeconds, diffTicks).asClockTicks(frequency);
    }
}

class ClockedTime
{
public:
    using seconds_t = Time::seconds_t;
    using ticks_t = Time::ticks_t;

    ClockedTime() = delete;
    explicit ClockedTime(uint32_t frequency) : _clockFreq(frequency) {}
    void setFrequency(uint32_t frequency) { _clockFreq = frequency; }
    inline void addCycles(cycles_t cycles)
    {
        _time.addCycles(cycles, _clockFreq);
    }
    inline cycles_t asClockTicks() const
    {
        return _time.asClockTicks(_clockFreq);
    }
    uint32_t getClockFreq() const
    {
        return _clockFreq;
    }

    inline bool isZero() const { return _time.isZero(); }
    inline bool isNever() const { return _time.isNever(); }
    inline seconds_t seconds() const { return _time.seconds(); }
    inline seconds_t secondsRounded() const { return _time.secondsRounded(); }
    inline ticks_t ticks() const { return _time.ticks(); }
    inline double asSeconds() const { return _time.asSeconds(); }

    virtual ClockedTime operator+(const Time& other)
    {
        ClockedTime result{_clockFreq};
        result._time = _time + other;
        return result;
    }

    bool operator<(const Time& other) const { return _time < other; }
    bool operator<(const ClockedTime& other) const { return _time < other._time; }

    bool operator>=(const ClockedTime& other) const { return !(*this < other); }
    bool operator>=(const Time& other) const { return !(*this < other); }

    bool operator>(const ClockedTime& other) const { return other < *this; }
    bool operator>(const Time& other) const { return other < this->_time; }

    bool operator<=(const ClockedTime& other) const { return other >= *this; }
    bool operator<=(const Time& other) const { return other >= this->_time; }

    bool operator==(const ClockedTime& other) const { return _time == other._time && _clockFreq && other._clockFreq; }

    bool operator!=(const ClockedTime& other) const { return !(*this == other); }

    std::string asString() const { return _time.asString(); }

    int64_t difference(const ClockedTime& other) const
    {
        return _time.differenceInClockTicks(other._time, _clockFreq);
    }
    int64_t difference_us(const ClockedTime& other) const
    {
        auto cycleDiff = difference(other);
        return cycleDiff < 0x80000000000ll ? difference(other) * 1000000 / _clockFreq : difference(other) / _clockFreq * 1000000;
    }
    int64_t excessTime_us(const ClockedTime& endTime, int64_t targetDuration) const
    {
        auto t = difference_us(endTime) - targetDuration;
        return t > 0 ? t : 0;
    }
    void reset()
    {
        _time = Time::zero;
    }
private:
    uint32_t _clockFreq{};
    Time _time{};
};

class TimeGuard {
public:
    TimeGuard(const ClockedTime& clockedTime, int64_t targetDuration_us)
        : _clockedTime(clockedTime)
        , _startTime(clockedTime)
        , _targetDuration_us(targetDuration_us)
    {
    }
    inline int64_t diffTime() const
    {
        return _startTime.difference_us(_clockedTime)/ (Time::ticksPerSecond/1000000) - _targetDuration_us;
    }
    inline int64_t excessTime() const
    {
        auto t = diffTime();
        return t > 0 ? t : 0;
    }
private:
    const ClockedTime& _clockedTime;
    ClockedTime _startTime;
    int64_t _targetDuration_us;
};

}  // namespace emu
