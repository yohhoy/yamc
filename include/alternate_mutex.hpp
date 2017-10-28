/*
 * alternate_mutex.hpp
 *
 * MIT License
 *
 * Copyright (c) 2017 yohhoy
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#ifndef YAMC_ALTERNATE_MUTEX_HPP_
#define YAMC_ALTERNATE_MUTEX_HPP_

#include <cassert>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>


namespace yamc {

/*
 * alternate implementation of mutex variants
 *
 * - yamc::alternate::mutex (alias of std::mutex)
 * - yamc::alternate::recursive_mutex
 * - yamc::alternate::timed_mutex
 * - yamc::alternate::recursive_timed_mutex
 */
namespace alternate {

// declare for consistency
using mutex = std::mutex;


class recursive_mutex {
  std::size_t ncount_ = 0;
  std::atomic<std::thread::id> owner_ = {};
  std::mutex mtx_;

public:
  recursive_mutex() = default;
  ~recursive_mutex()
  {
    assert(ncount_ == 0 && owner_ == std::thread::id());
  }

  recursive_mutex(const recursive_mutex&) = delete;
  recursive_mutex& operator=(const recursive_mutex&) = delete;

  void lock()
  {
    const auto tid = std::this_thread::get_id();
    if (owner_.load(std::memory_order_relaxed) == tid) {
      ++ncount_;
    } else {
      mtx_.lock();
      owner_.store(tid, std::memory_order_relaxed);
      ncount_ = 1;
    }
  }

  bool try_lock()
  {
    const auto tid = std::this_thread::get_id();
    if (owner_.load(std::memory_order_relaxed) == tid) {
      ++ncount_;
    } else {
      if (!mtx_.try_lock())
        return false;
      owner_.store(tid, std::memory_order_relaxed);
      ncount_ = 1;
    }
    return true;
  }

  void unlock()
  {
    assert(0 < ncount_ && owner_ == std::this_thread::get_id());
    if (--ncount_ == 0) {
      owner_.store(std::thread::id(), std::memory_order_relaxed);
      mtx_.unlock();
    }
  }
};


class timed_mutex {
  int state_ = 0;
  std::condition_variable cv_;
  std::mutex mtx_;

  template<typename Clock, typename Duration>
  bool do_try_lockwait(const std::chrono::time_point<Clock, Duration>& tp)
  {
    std::unique_lock<decltype(mtx_)> lk(mtx_);
    while (state_ != 0) {
      if (cv_.wait_until(lk, tp) == std::cv_status::timeout) {
        if (state_ == 0)  // re-check predicate
          break;
        return false;
      }
    }
    state_ = 1;
    return true;
  }

public:
  timed_mutex() = default;
  ~timed_mutex()
  {
    assert(state_ == 0);
  }

  timed_mutex(const timed_mutex&) = delete;
  timed_mutex& operator=(const timed_mutex&) = delete;

  void lock()
  {
    std::unique_lock<decltype(mtx_)> lk(mtx_);
    while (state_ != 0) {
      cv_.wait(lk);
    }
    state_ = 1;
  }

  bool try_lock()
  {
    std::lock_guard<decltype(mtx_)> lk(mtx_);
    if (state_ != 0)
      return false;
    state_ = 1;
    return true;
  }

  void unlock()
  {
    std::lock_guard<decltype(mtx_)> lk(mtx_);
    assert(state_ == 1);
    state_ = 0;
    cv_.notify_one();
  }

  template<typename Rep, typename Period>
  bool try_lock_for(const std::chrono::duration<Rep, Period>& duration)
  {
    const auto tp = std::chrono::steady_clock::now() + duration;
    return do_try_lockwait(tp);
  }

  template<typename Clock, typename Duration>
  bool try_lock_until(const std::chrono::time_point<Clock, Duration>& tp)
  {
    return do_try_lockwait(tp);
  }
};


class recursive_timed_mutex {
  std::size_t ncount_ = 0;
  std::thread::id owner_ = {};
  std::condition_variable cv_;
  std::mutex mtx_;

  template<typename Clock, typename Duration>
  bool do_try_lockwait(const std::chrono::time_point<Clock, Duration>& tp)
  {
    const auto tid = std::this_thread::get_id();
    std::unique_lock<decltype(mtx_)> lk(mtx_);
    if (owner_ == tid) {
      ++ncount_;
      return true;
    }
    while (ncount_ != 0) {
      if (cv_.wait_until(lk, tp) == std::cv_status::timeout) {
        if (ncount_ == 0)  // re-check predicate
          break;
        return false;
      }
    }
    assert(owner_ == std::thread::id());
    ncount_ = 1;
    owner_ = tid;
    return true;
  }

public:
  recursive_timed_mutex() = default;
  ~recursive_timed_mutex()
  {
    assert(ncount_ == 0 && owner_ == std::thread::id());
  }

  recursive_timed_mutex(const recursive_timed_mutex&) = delete;
  recursive_timed_mutex& operator=(const recursive_timed_mutex&) = delete;

  void lock()
  {
    const auto tid = std::this_thread::get_id();
    std::unique_lock<decltype(mtx_)> lk(mtx_);
    if (owner_ == tid) {
      ++ncount_;
      return;
    }
    while (ncount_ != 0) {
      cv_.wait(lk);
    }
    assert(owner_ == std::thread::id());
    ncount_ = 1;
    owner_ = tid;
  }

  bool try_lock()
  {
    const auto tid = std::this_thread::get_id();
    std::lock_guard<decltype(mtx_)> lk(mtx_);
    if (owner_ == tid) {
      ++ncount_;
      return true;
    }
    if (ncount_ == 0) {
      assert(owner_ == std::thread::id());
      ncount_ = 1;
      owner_ = tid;
      return true;
    }
    return false;
  }

  void unlock()
  {
    std::lock_guard<decltype(mtx_)> lk(mtx_);
    assert(0 < ncount_ && owner_ == std::this_thread::get_id());
    if (--ncount_ == 0) {
      owner_ = std::thread::id();
      cv_.notify_one();
    }
  }

  template<typename Rep, typename Period>
  bool try_lock_for(const std::chrono::duration<Rep, Period>& duration)
  {
    const auto tp = std::chrono::steady_clock::now() + duration;
    return do_try_lockwait(tp);
  }

  template<typename Clock, typename Duration>
  bool try_lock_until(const std::chrono::time_point<Clock, Duration>& tp)
  {
    return do_try_lockwait(tp);
  }
};

} // namespace alternate
} // namespace yamc

#endif
