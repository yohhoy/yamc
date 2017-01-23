/*
 * fair_mutex.hpp
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
#ifndef YAMC_FAIR_MUTEX_HPP_
#define YAMC_FAIR_MUTEX_HPP_

#include <cassert>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>


namespace yamc {

/*
 * fairness (FIFO locking) mutex
 */
namespace fair {

namespace detail {

class mutex_base {
protected:
  std::size_t next_ = 0;
  std::size_t curr_ = 0;
  std::condition_variable cv_;
  std::mutex mtx_;

  void lock()
  {
    std::unique_lock<decltype(mtx_)> lk(mtx_);
    const std::size_t request = next_++;
    while (request != curr_) {
      cv_.wait(lk);
    }
  }

  bool try_lock()
  {
    std::lock_guard<decltype(mtx_)> lk(mtx_);
    if (next_ != curr_)
      return false;
    ++next_;
    return true;
  }

  void unlock()
  {
    std::lock_guard<decltype(mtx_)> lk(mtx_);
    ++curr_;
    cv_.notify_all();
  }
};


class recursive_mutex_base {
protected:
  std::size_t next_ = 0;
  std::size_t curr_ = 0;
  std::size_t ncount_ = 0;
  std::thread::id owner_;
  std::condition_variable cv_;
  std::mutex mtx_;

  void lock()
  {
    const auto tid = std::this_thread::get_id();
    std::unique_lock<decltype(mtx_)> lk(mtx_);
    if (owner_ == tid) {
      assert(0 < ncount_);
      ++ncount_;
      return;
    }
    const std::size_t request = next_++;
    while (request != curr_) {
      cv_.wait(lk);
    }
    assert(ncount_ == 0 && owner_ == std::thread::id());
    ncount_ = 1;
    owner_ = tid;
  }

  bool try_lock()
  {
    const auto tid = std::this_thread::get_id();
    std::lock_guard<decltype(mtx_)> lk(mtx_);
    if (owner_ == tid) {
      assert(0 < ncount_);
      ++ncount_;
      return true;
    }
    if (next_ != curr_)
      return false;
    ++next_;
    assert(ncount_ == 0 && owner_ == std::thread::id());
    ncount_ = 1;
    owner_ = tid;
    return true;
  }

  void unlock()
  {
    std::lock_guard<decltype(mtx_)> lk(mtx_);
    assert(0 < ncount_);
    if (--ncount_ == 0) {
      ++curr_;
      assert(owner_ == std::this_thread::get_id());
      owner_ = std::thread::id();
      cv_.notify_all();
    }
  }
};

} // namespace detail


class mutex : private detail::mutex_base {
  using base = detail::mutex_base;

public:
  mutex() = default;
  ~mutex() = default;

  mutex(const mutex&) = delete;
  mutex& operator=(const mutex&) = delete;

  using base::lock;
  using base::try_lock;
  using base::unlock;
};


class timed_mutex : private detail::mutex_base {
  using base = detail::mutex_base;

  template <typename Clock, typename Duration>
  bool do_try_lockwait(const std::chrono::time_point<Clock, Duration>& tp)
  {
    std::unique_lock<decltype(mtx_)> lk(mtx_);
    std::size_t request = next_;
    while (request != curr_) {
      if (cv_.wait_until(lk, tp) == std::cv_status::timeout) {
        if (request == curr_)  // re-check predicate
          break;
        return false;
      }
    }
    ++next_;
    return true;
  }

public:
  timed_mutex() = default;
  ~timed_mutex() = default;

  timed_mutex(const timed_mutex&) = delete;
  timed_mutex& operator=(const timed_mutex&) = delete;

  using base::lock;
  using base::try_lock;
  using base::unlock;

  template <typename Rep, typename Period>
  bool try_lock_for(const std::chrono::duration<Rep, Period>& duration)
  {
    const auto tp = std::chrono::system_clock::now() + duration;
    return do_try_lockwait(tp);
  }

  template <typename Clock, typename Duration>
  bool try_lock_until(const std::chrono::time_point<Clock, Duration>& tp)
  {
    return do_try_lockwait(tp);
  }
};


class recursive_mutex : private detail::recursive_mutex_base {
  using base = detail::recursive_mutex_base;

public:
  recursive_mutex() = default;
  ~recursive_mutex() = default;

  recursive_mutex(const recursive_mutex&) = delete;
  recursive_mutex& operator=(const recursive_mutex&) = delete;

  using base::lock;
  using base::try_lock;
  using base::unlock;
};


class recursive_timed_mutex : private detail::recursive_mutex_base {
  using base = detail::recursive_mutex_base;

  template <typename Clock, typename Duration>
  bool do_try_lockwait(const std::chrono::time_point<Clock, Duration>& tp)
  {
    const auto tid = std::this_thread::get_id();
    std::unique_lock<decltype(mtx_)> lk(mtx_);
    if (owner_ == tid) {
      assert(0 < ncount_);
      ++ncount_;
      return true;
    }
    std::size_t request = next_;
    while (request != curr_) {
      if (cv_.wait_until(lk, tp) == std::cv_status::timeout) {
        if (request == curr_)  // re-check predicate
          break;
        return false;
      }
    }
    ++next_;
    assert(ncount_ == 0 && owner_ == std::thread::id());
    ncount_ = 1;
    owner_ = tid;
    return true;
  }

public:
  recursive_timed_mutex() = default;
  ~recursive_timed_mutex() = default;

  recursive_timed_mutex(const recursive_timed_mutex&) = delete;
  recursive_timed_mutex& operator=(const recursive_timed_mutex&) = delete;

  using base::lock;
  using base::try_lock;
  using base::unlock;

  template <typename Rep, typename Period>
  bool try_lock_for(const std::chrono::duration<Rep, Period>& duration)
  {
    const auto tp = std::chrono::system_clock::now() + duration;
    return do_try_lockwait(tp);
  }

  template <typename Clock, typename Duration>
  bool try_lock_until(const std::chrono::time_point<Clock, Duration>& tp)
  {
    return do_try_lockwait(tp);
  }
};

} // namespace fair
} // namespace yamc

#endif
