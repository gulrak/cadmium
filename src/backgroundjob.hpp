//---------------------------------------------------------------------------------------
// src/backgroundjob.hpp
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
#pragma once

#include <chrono>
#include <functional>
#include <optional>
#include <utility>

#ifndef PLATFORM_WEB
#include <future>
#include <threadpool.hpp>
#endif

// Runs one expensive job at a time without blocking the UI loop. On desktop it runs on a
// thread pool, on the web (no threads) it runs synchronously one frame after it was started,
// so a frame showing that work is going on gets drawn first. Starting a new job replaces the
// previous one, the result of a replaced job is dropped.
template<typename Result>
class BackgroundJob
{
public:
#ifndef PLATFORM_WEB
    explicit BackgroundJob(ThreadPool& pool) : _pool(pool) {}
#else
    BackgroundJob() = default;
#endif

    void start(std::function<Result()> work)
    {
#ifndef PLATFORM_WEB
        _future = _pool.enqueue(std::move(work));
#else
        _work = std::move(work);
        _framesWaited = 0;
#endif
        _busy = true;
    }

    bool busy() const { return _busy; }

    // Call once per frame, returns the result of the current job once it is done.
    std::optional<Result> poll()
    {
        if (!_busy)
            return {};
#ifndef PLATFORM_WEB
        if (_future.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
            return {};
        _busy = false;
        return _future.get();
#else
        if (_framesWaited++ < 1)
            return {};
        _busy = false;
        return std::exchange(_work, {})();
#endif
    }

    // Blocks until the current job is done, for when its result is needed right now.
    std::optional<Result> wait()
    {
        if (!_busy)
            return {};
#ifndef PLATFORM_WEB
        _future.wait();
#else
        _framesWaited = 1;
#endif
        return poll();
    }

private:
#ifndef PLATFORM_WEB
    ThreadPool& _pool;
    std::future<Result> _future;
#else
    std::function<Result()> _work;
    int _framesWaited{0};
#endif
    bool _busy{false};
};
