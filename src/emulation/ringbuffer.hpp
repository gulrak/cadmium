//---------------------------------------------------------------------------------------
// src/emulation/ringbuffer.hpp
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
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace emu {
template<typename SampleType = std::uint16_t, std::size_t Capacity = 48000>
class RingBuffer {
    static_assert(Capacity > 0, "Capacity must be greater than 0");
    static_assert(std::is_trivially_copyable_v<SampleType>, "SampleType must be trivially copyable");

public:
    RingBuffer() noexcept : _head{0}, _tail{0} {}

    RingBuffer(const RingBuffer&) = delete;
    RingBuffer& operator=(const RingBuffer&) = delete;
    RingBuffer(RingBuffer&&) = delete;
    RingBuffer& operator=(RingBuffer&&) = delete;

    // Single sample push - returns true if inserted, false if buffer full
    [[nodiscard]] bool push(SampleType sample) noexcept {
        const auto currentHead = _head.load(std::memory_order_relaxed);
        const auto nextHead = increment(currentHead);

        if (nextHead == _tail.load(std::memory_order_acquire)) {
            return false;
        }

        _buffer[currentHead] = sample;
        _head.store(nextHead, std::memory_order_release);
        return true;
    }

    // Block-wise push - returns number of samples actually inserted
    std::size_t push(std::span<const SampleType> samples) noexcept {
        if (samples.empty()) {
            return 0;
        }

        const auto currentHead = _head.load(std::memory_order_relaxed);
        const auto currentTail = _tail.load(std::memory_order_acquire);

        const auto available = availableForWrite(currentHead, currentTail);
        const auto toWrite = std::min(available, samples.size());

        if (toWrite == 0) {
            return 0;
        }

        const auto firstChunk = std::min(toWrite, kBufferSize - currentHead);
        const auto secondChunk = toWrite - firstChunk;

        std::copy_n(samples.data(), firstChunk, _buffer + currentHead);
        if (secondChunk > 0) {
            std::copy_n(samples.data() + firstChunk, secondChunk, _buffer);
        }

        _head.store((currentHead + toWrite) % kBufferSize, std::memory_order_release);
        return toWrite;
    }

    // Single sample pop - returns nullopt if buffer empty
    [[nodiscard]] std::optional<SampleType> pop() noexcept {
        const auto currentTail = _tail.load(std::memory_order_relaxed);

        if (currentTail == _head.load(std::memory_order_acquire)) {
            return std::nullopt;
        }

        const auto sample = _buffer[currentTail];
        _tail.store(increment(currentTail), std::memory_order_release);
        return sample;
    }

    // Block-wise pop - returns number of samples actually popped
    std::size_t pop(SampleType* outputBuffer, std::size_t count) noexcept {
        if (count == 0 || outputBuffer == nullptr) {
            return 0;
        }

        const auto currentTail = _tail.load(std::memory_order_relaxed);
        const auto currentHead = _head.load(std::memory_order_acquire);

        const auto available = availableForRead(currentHead, currentTail);
        const auto toRead = std::min(available, count);

        if (toRead == 0) {
            return 0;
        }

        const auto firstChunk = std::min(toRead, kBufferSize - currentTail);
        const auto secondChunk = toRead - firstChunk;

        std::copy_n(_buffer + currentTail, firstChunk, outputBuffer);
        if (secondChunk > 0) {
            std::copy_n(_buffer, secondChunk, outputBuffer + firstChunk);
        }

        _tail.store((currentTail + toRead) % kBufferSize, std::memory_order_release);
        return toRead;
    }

    // Block-wise pop with span output
    std::size_t pop(std::span<SampleType> output) noexcept {
        return pop(output.data(), output.size());
    }

    [[nodiscard]] std::size_t size() const noexcept {
        const auto currentHead = _head.load(std::memory_order_acquire);
        const auto currentTail = _tail.load(std::memory_order_acquire);
        return availableForRead(currentHead, currentTail);
    }

    [[nodiscard]] std::size_t available() const noexcept {
        const auto currentHead = _head.load(std::memory_order_acquire);
        const auto currentTail = _tail.load(std::memory_order_acquire);
        return availableForWrite(currentHead, currentTail);
    }

    [[nodiscard]] bool empty() const noexcept {
        return _head.load(std::memory_order_acquire) == _tail.load(std::memory_order_acquire);
    }

    [[nodiscard]] bool full() const noexcept {
        const auto currentHead = _head.load(std::memory_order_relaxed);
        return increment(currentHead) == _tail.load(std::memory_order_acquire);
    }

    [[nodiscard]] static constexpr std::size_t capacity() noexcept {
        return Capacity;
    }

    // Reset buffer - only safe when no concurrent operations
    void reset() noexcept {
        _head.store(0, std::memory_order_relaxed);
        _tail.store(0, std::memory_order_release);
    }

private:
    static constexpr std::size_t kCacheLineSize = 64;
    static constexpr std::size_t kBufferSize = Capacity + 1;

    [[nodiscard]] static constexpr std::size_t increment(std::size_t index) noexcept {
        return (index + 1) % kBufferSize;
    }

    [[nodiscard]] static constexpr std::size_t availableForWrite(std::size_t head, std::size_t tail) noexcept {
        return (tail - head - 1 + kBufferSize) % kBufferSize;
    }

    [[nodiscard]] static constexpr std::size_t availableForRead(std::size_t head, std::size_t tail) noexcept {
        return (head - tail + kBufferSize) % kBufferSize;
    }

    alignas(kCacheLineSize) std::atomic<std::size_t> _head;
    alignas(kCacheLineSize) std::atomic<std::size_t> _tail;
    alignas(kCacheLineSize) SampleType _buffer[kBufferSize];
};

}
