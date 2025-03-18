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

class Time final
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
    static Time fromMicroseconds(uint64_t microseconds) { return Time(microseconds / 1000000, (microseconds % 1000000) * (ticksPerSecond / 1000000)); }

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
    explicit ClockedTime(uint32_t frequency)
        : _clockFreq(frequency)
    {
    }
    void setFrequency(uint32_t frequency) { _clockFreq = frequency; }
    inline void addCycles(cycles_t cycles) { _time.addCycles(cycles, _clockFreq); }
    inline cycles_t asClockTicks() const { return _time.asClockTicks(_clockFreq); }
    uint32_t getClockFreq() const { return _clockFreq; }

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

    int64_t difference(const ClockedTime& other) const { return _time.differenceInClockTicks(other._time, _clockFreq); }
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
    void reset() { _time = Time::zero; }

private:
    uint32_t _clockFreq{};
    Time _time{};
};

class TimeGuard
{
public:
    TimeGuard(const ClockedTime& clockedTime, int64_t targetDuration_us)
        : _clockedTime(clockedTime)
        , _startTime(clockedTime)
        , _targetDuration_us(targetDuration_us)
    {
    }
    inline int64_t diffTime() const { return _startTime.difference_us(_clockedTime) / (Time::ticksPerSecond / 1000000) - _targetDuration_us; }
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

/// A new take on a time class für my emulation projects, less absurd in its value range, but hopefully more lightweight.
///
/// The time is represented as a number of clock cycles, the range is good for >100 years even with 1GHz frequency.
/// The clock frequency in Hz is stored in the class, so that the time can be converted to different units.
///
/// @note { The class is not thread safe. }
class CycleTime
{
    static constexpr uint64_t safeConvertCycles(uint64_t cycles, uint64_t fromFrequency, uint64_t toFrequency) noexcept { return (cycles / fromFrequency) * toFrequency + ((cycles % fromFrequency) * toFrequency) / fromFrequency; }

    template <typename Rep, typename Period>
    static constexpr uint64_t safeDurationToCycles(Rep count, uint64_t frequency) noexcept
    {
        constexpr auto num = Period::num;
        constexpr auto den = Period::den;
        return (count / den) * (frequency * num) + ((count % den) * (frequency * num)) / den;
    }

public:
    constexpr CycleTime()
        : _cycles(0)
        , _frequency(1)
    {
    }
    constexpr CycleTime(uint64_t cycles, uint64_t frequency)
        : _cycles(cycles)
        , _frequency(frequency)
    {
    }
    template <typename Duration>
    constexpr CycleTime(const Duration& d, uint64_t frequency)
        : _frequency(frequency)
    {
        if constexpr (std::is_integral_v<typename Duration::rep>) {
            _cycles = safeDurationToCycles<typename Duration::rep, typename Duration::period>(d.count(), frequency);
        }
        else {
            _cycles = static_cast<uint64_t>((static_cast<double>(d.count()) * Duration::period::num / Duration::period::den) * frequency);
        }
    }

    [[nodiscard]] constexpr double asSeconds() const noexcept { return static_cast<double>(_cycles) / static_cast<double>(_frequency); }
    [[nodiscard]] constexpr std::pair<uint64_t, uint64_t> asIntervals(uint64_t intervalCycles) const noexcept { return {_cycles / intervalCycles, _cycles % intervalCycles}; }
    template <typename Ratio = std::ratio<1>>
    [[nodiscard]] constexpr std::chrono::duration<uint64_t, Ratio> asDuration() const noexcept
    {
        uint64_t count = safeConvertCycles(_cycles, _frequency * Ratio::num, Ratio::den);
        return std::chrono::duration<uint64_t, Ratio>(count);
    }

    constexpr void addCycles(uint64_t cycles) noexcept { _cycles += cycles; }
    constexpr CycleTime& operator+=(int cycles) noexcept { _cycles += cycles; return *this; }
    CycleTime& operator+=(const CycleTime& other) noexcept
    {
        if (_frequency == other._frequency) {
            _cycles += other._cycles;
        }
        else {
            _cycles += safeConvertCycles(other._cycles, other._frequency, _frequency);
        }
        return *this;
    }

    constexpr std::strong_ordering operator<=>(const CycleTime& other) const noexcept
    {
        if (_frequency == other._frequency)
            return _cycles <=> other._cycles;
        uint64_t otherConverted = safeConvertCycles(other._cycles, other._frequency, _frequency);
        return _cycles <=> otherConverted;
    }

    [[nodiscard]] int64_t differenceInClockCycles(const CycleTime& other, const std::optional<uint64_t>& freqOpt = std::nullopt) const noexcept
    {
        uint64_t targetFrequency = freqOpt.value_or(_frequency);
        uint64_t thisCycles = (_frequency == targetFrequency) ? _cycles : safeConvertCycles(_cycles, _frequency, targetFrequency);
        uint64_t otherCycles = (other._frequency == targetFrequency) ? other._cycles : safeConvertCycles(other._cycles, other._frequency, targetFrequency);
        return static_cast<int64_t>(thisCycles) - static_cast<int64_t>(otherCycles);
    }

    [[nodiscard]] constexpr CycleTime convert(uint64_t newFrequency) const noexcept
    {
        uint64_t newCycles = (_frequency == newFrequency) ? _cycles : safeConvertCycles(_cycles, _frequency, newFrequency);
        return {newCycles, newFrequency};
    }

    [[nodiscard]] constexpr uint64_t getCycles() const noexcept { return _cycles; }
    [[nodiscard]] constexpr uint64_t getFrequency() const noexcept { return _frequency; }

private:
    uint64_t _cycles;
    uint64_t _frequency;
};

}  // namespace emu
