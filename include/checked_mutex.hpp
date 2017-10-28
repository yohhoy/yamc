/*
 * checked_mutex.hpp
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
#ifndef YAMC_CHECKED_MUTEX_HPP_
#define YAMC_CHECKED_MUTEX_HPP_

#include <cassert>
#include <cstdlib>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <system_error>
#include <thread>
#include "yamc_lock_validator.hpp"


// call std::abort() when requirements violation
#ifndef YAMC_CHECKED_CALL_ABORT
#define YAMC_CHECKED_CALL_ABORT 0
#endif

// default deadlock detection mode
#ifndef YAMC_CHECKED_DETECT_DEADLOCK
#define YAMC_CHECKED_DETECT_DEADLOCK 1
#endif


namespace yamc {

/*
 * strict requirements checking mutex for debug
 *
 * - yamc::checked::mutex
 * - yamc::checked::timed_mutex
 * - yamc::checked::recursive_mutex
 * - yamc::checked::recursive_timed_mutex
 */
namespace checked {

namespace detail {

#if YAMC_CHECKED_DETECT_DEADLOCK
using validator = yamc::validator::deadlock;
#else
using validator = yamc::validator::null;
#endif


class mutex_base {
protected:
  std::thread::id owner_;
  std::condition_variable cv_;
  std::mutex mtx_;

  void dtor_precondition(const char* emsg)
  {
    std::lock_guard<decltype(mtx_)> lk(mtx_);
    if (owner_ != std::thread::id()) {
      // object liveness
#if YAMC_CHECKED_CALL_ABORT
      std::abort();
      (void)emsg;  // suppress "unused variable" warning
#else
      throw std::system_error(std::make_error_code(std::errc::resource_deadlock_would_occur), emsg);
#endif
    }
  }

  void lock()
  {
    const auto tid = std::this_thread::get_id();
    std::unique_lock<decltype(mtx_)> lk(mtx_);
    if (owner_ == tid) {
      // non-recursive semantics
#if YAMC_CHECKED_CALL_ABORT
      std::abort();
#else
      throw std::system_error(std::make_error_code(std::errc::resource_deadlock_would_occur), "recursive lock");
#endif
    }
    while (owner_ != std::thread::id()) {
      if (!validator::enqueue(reinterpret_cast<uintptr_t>(this), tid, false)) {
        // deadlock detection
#if YAMC_CHECKED_CALL_ABORT
        std::abort();
#else
        throw std::system_error(std::make_error_code(std::errc::resource_deadlock_would_occur), "deadlock");
#endif
      }
      cv_.wait(lk);
      validator::dequeue(reinterpret_cast<uintptr_t>(this), tid);
    }
    owner_ = tid;
    validator::locked(reinterpret_cast<uintptr_t>(this), tid, false);
  }

  bool try_lock()
  {
    const auto tid = std::this_thread::get_id();
    std::lock_guard<decltype(mtx_)> lk(mtx_);
    if (owner_ == tid) {
      // non-recursive semantics
#if YAMC_CHECKED_CALL_ABORT
      std::abort();
#else
      throw std::system_error(std::make_error_code(std::errc::resource_deadlock_would_occur), "recursive try_lock");
#endif
    }
    if (owner_ != std::thread::id()) {
      return false;
    }
    owner_ = tid;
    validator::locked(reinterpret_cast<uintptr_t>(this), tid, false);
    return true;
  }

  void unlock()
  {
    const auto tid = std::this_thread::get_id();
    std::lock_guard<decltype(mtx_)> lk(mtx_);
    if (owner_ != tid) {
      // owner thread
#if YAMC_CHECKED_CALL_ABORT
      std::abort();
#else
      throw std::system_error(std::make_error_code(std::errc::operation_not_permitted), "invalid unlock");
#endif
    }
    owner_ = std::thread::id();
    validator::unlocked(reinterpret_cast<uintptr_t>(this), tid, false);
    cv_.notify_all();
  }
};


class recursive_mutex_base {
protected:
  std::size_t ncount_ = 0;
  std::thread::id owner_;
  std::condition_variable cv_;
  std::mutex mtx_;

  void dtor_precondition(const char* emsg)
  {
    std::lock_guard<decltype(mtx_)> lk(mtx_);
    if (ncount_ != 0 || owner_ != std::thread::id()) {
      // object liveness
#if YAMC_CHECKED_CALL_ABORT
      std::abort();
      (void)emsg;  // suppress "unused variable" warning
#else
      throw std::system_error(std::make_error_code(std::errc::resource_deadlock_would_occur), emsg);
#endif
    }
  }

  void lock()
  {
    const auto tid = std::this_thread::get_id();
    std::unique_lock<decltype(mtx_)> lk(mtx_);
    if (owner_ == tid) {
      ++ncount_;
      return;
    }
    while (ncount_ != 0) {
      if (!validator::enqueue(reinterpret_cast<uintptr_t>(this), tid, false)) {
        // deadlock detection
#if YAMC_CHECKED_CALL_ABORT
        std::abort();
#else
        throw std::system_error(std::make_error_code(std::errc::resource_deadlock_would_occur), "deadlock");
#endif
      }
      cv_.wait(lk);
      validator::dequeue(reinterpret_cast<uintptr_t>(this), tid);
    }
    assert(owner_ == std::thread::id());
    ncount_ = 1;
    owner_ = tid;
    validator::locked(reinterpret_cast<uintptr_t>(this), tid, false);
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
      validator::locked(reinterpret_cast<uintptr_t>(this), tid, false);
      return true;
    }
    return false;
  }

  void unlock()
  {
    const auto tid = std::this_thread::get_id();
    std::lock_guard<decltype(mtx_)> lk(mtx_);
    if (owner_ != tid) {
      // owner thread
#if YAMC_CHECKED_CALL_ABORT
      std::abort();
#else
      throw std::system_error(std::make_error_code(std::errc::operation_not_permitted), "invalid unlock");
#endif
    }
    assert(0 < ncount_);
    if (--ncount_ == 0) {
      owner_ = std::thread::id();
      validator::unlocked(reinterpret_cast<uintptr_t>(this), tid, false);
      cv_.notify_all();
    }
  }
};

} // namespace detail


class mutex : private detail::mutex_base {
  using base = detail::mutex_base;

public:
  mutex()
  {
    detail::validator::ctor(reinterpret_cast<uintptr_t>(this));
  }

  ~mutex() noexcept(false)
  {
    detail::validator::dtor(reinterpret_cast<uintptr_t>(this));
    dtor_precondition("abandoned mutex");
  }

  mutex(const mutex&) = delete;
  mutex& operator=(const mutex&) = delete;

  using base::lock;
  using base::try_lock;
  using base::unlock;
};


class timed_mutex : private detail::mutex_base {
  using base = detail::mutex_base;

  template<typename Clock, typename Duration>
  bool do_try_lockwait(const std::chrono::time_point<Clock, Duration>& tp, const char* emsg)
  {
    const auto tid = std::this_thread::get_id();
    std::unique_lock<decltype(mtx_)> lk(mtx_);
    if (owner_ == tid) {
      // non-recursive semantics
#if YAMC_CHECKED_CALL_ABORT
      std::abort();
      (void)emsg;  // suppress "unused variable" warning
#else
      throw std::system_error(std::make_error_code(std::errc::resource_deadlock_would_occur), emsg);
#endif
    }
    while (owner_ != std::thread::id()) {
      if (cv_.wait_until(lk, tp) == std::cv_status::timeout) {
        if (owner_ == std::thread::id())  // re-check predicate
          break;
        return false;
      }
    }
    owner_ = tid;
    detail::validator::locked(reinterpret_cast<uintptr_t>(this), tid, false);
    return true;
  }

public:
  timed_mutex()
  {
    detail::validator::ctor(reinterpret_cast<uintptr_t>(this));
  }

  ~timed_mutex() noexcept(false)
  {
    detail::validator::dtor(reinterpret_cast<uintptr_t>(this));
    dtor_precondition("abandoned timed_mutex");
  }

  timed_mutex(const timed_mutex&) = delete;
  timed_mutex& operator=(const timed_mutex&) = delete;

  using base::lock;
  using base::try_lock;
  using base::unlock;

  template<typename Rep, typename Period>
  bool try_lock_for(const std::chrono::duration<Rep, Period>& duration)
  {
    const auto tp = std::chrono::steady_clock::now() + duration;
    return do_try_lockwait(tp, "recursive try_lock_for");
  }

  template<typename Clock, typename Duration>
  bool try_lock_until(const std::chrono::time_point<Clock, Duration>& tp)
  {
    return do_try_lockwait(tp, "recursive try_lock_until");
  }
};


class recursive_mutex : private detail::recursive_mutex_base {
  using base = detail::recursive_mutex_base;

public:
  recursive_mutex()
  {
    detail::validator::ctor(reinterpret_cast<uintptr_t>(this));
  }

  ~recursive_mutex() noexcept(false)
  {
    detail::validator::dtor(reinterpret_cast<uintptr_t>(this));
    dtor_precondition("abandoned recursive_mutex");
  }

  recursive_mutex(const recursive_mutex&) = delete;
  recursive_mutex& operator=(const recursive_mutex&) = delete;

  using base::lock;
  using base::try_lock;
  using base::unlock;
};


class recursive_timed_mutex : private detail::recursive_mutex_base {
  using base = detail::recursive_mutex_base;

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
    detail::validator::locked(reinterpret_cast<uintptr_t>(this), tid, false);
    return true;
  }

public:
  recursive_timed_mutex()
  {
    detail::validator::ctor(reinterpret_cast<uintptr_t>(this));
  }

  ~recursive_timed_mutex() noexcept(false)
  {
    detail::validator::dtor(reinterpret_cast<uintptr_t>(this));
    dtor_precondition("abandoned recursive_timed_mutex");
  }

  recursive_timed_mutex(const recursive_timed_mutex&) = delete;
  recursive_timed_mutex& operator=(const recursive_timed_mutex&) = delete;

  using base::lock;
  using base::try_lock;
  using base::unlock;

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

} // namespace checked
} // namespace yamc

#endif
