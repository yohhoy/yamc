/*
 * checked_shared_mutex.hpp
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
#ifndef YAMC_CHECKED_SHARED_MUTEX_HPP_
#define YAMC_CHECKED_SHARED_MUTEX_HPP_


#include <cassert>
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <system_error>
#include <thread>
#include <vector>
#include "yamc_rwlock_sched.hpp"
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
 * strict requirements checking shared mutex for debug
 *
 * - yamc::checked::shared_mutex
 * - yamc::checked::shared_timed_mutex
 * - yamc::checked::basic_shared_mutex<RwLockPolicy>
 * - yamc::checked::basic_shared_timed_mutex<RwLockPolicy>
 */
namespace checked {

namespace detail {

#if YAMC_CHECKED_DETECT_DEADLOCK
using validator = yamc::validator::deadlock;
#else
using validator = yamc::validator::null;
#endif


template <typename RwLockPolicy>
class shared_mutex_base {
protected:
  typename RwLockPolicy::state state_;
  std::thread::id e_owner_;  // exclusive ownership thread
  std::vector<std::thread::id> s_owner_;  // shared ownership threads
  std::condition_variable cv_;
  std::mutex mtx_;

  bool is_shared_owner(std::thread::id tid)
  {
    return std::find(s_owner_.begin(), s_owner_.end(), tid) != s_owner_.end();
  }

  void dtor_precondition(const char* emsg)
  {
    std::lock_guard<decltype(mtx_)> lk(mtx_);
    if (e_owner_ != std::thread::id() || !s_owner_.empty()) {
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
    if (e_owner_ == tid || is_shared_owner(tid)) {
      // non-recursive semantics
#if YAMC_CHECKED_CALL_ABORT
      std::abort();
#else
      throw std::system_error(std::make_error_code(std::errc::resource_deadlock_would_occur), "recursive lock");
#endif
    }
    RwLockPolicy::before_wait_wlock(state_);
    while (RwLockPolicy::wait_wlock(state_)) {
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
    RwLockPolicy::after_wait_wlock(state_);
    RwLockPolicy::acquire_wlock(state_);
    e_owner_ = tid;
    validator::locked(reinterpret_cast<uintptr_t>(this), tid, false);
  }

  bool try_lock()
  {
    const auto tid = std::this_thread::get_id();
    std::lock_guard<decltype(mtx_)> lk(mtx_);
    if (e_owner_ == tid || is_shared_owner(tid)) {
      // non-recursive semantics
#if YAMC_CHECKED_CALL_ABORT
      std::abort();
#else
      throw std::system_error(std::make_error_code(std::errc::resource_deadlock_would_occur), "recursive try_lock");
#endif
    }
    if (RwLockPolicy::wait_wlock(state_))
      return false;
    RwLockPolicy::acquire_wlock(state_);
    e_owner_ = tid;
    validator::locked(reinterpret_cast<uintptr_t>(this), tid, false);
    return true;
  }

  void unlock()
  {
    const auto tid = std::this_thread::get_id();
    std::lock_guard<decltype(mtx_)> lk(mtx_);
    if (e_owner_ != tid) {
      // owner thread
#if YAMC_CHECKED_CALL_ABORT
      std::abort();
#else
      throw std::system_error(std::make_error_code(std::errc::operation_not_permitted), "invalid unlock");
#endif
    }
    e_owner_ = {};
    RwLockPolicy::release_wlock(state_);
    validator::unlocked(reinterpret_cast<uintptr_t>(this), tid, false);
    cv_.notify_all();
  }

  void lock_shared()
  {
    const auto tid = std::this_thread::get_id();
    std::unique_lock<decltype(mtx_)> lk(mtx_);
    if (e_owner_ == tid || is_shared_owner(tid)) {
      // non-recursive semantics
#if YAMC_CHECKED_CALL_ABORT
      std::abort();
#else
      throw std::system_error(std::make_error_code(std::errc::resource_deadlock_would_occur), "recursive lock_shared");
#endif
    }
    while (RwLockPolicy::wait_rlock(state_)) {
      if (!validator::enqueue(reinterpret_cast<uintptr_t>(this), tid, true)) {
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
    RwLockPolicy::acquire_rlock(state_);
    s_owner_.push_back(tid);
    validator::locked(reinterpret_cast<uintptr_t>(this), tid, true);
  }

  bool try_lock_shared()
  {
    const auto tid = std::this_thread::get_id();
    std::lock_guard<decltype(mtx_)> lk(mtx_);
    if (e_owner_ == tid || is_shared_owner(tid)) {
      // non-recursive semantics
#if YAMC_CHECKED_CALL_ABORT
      std::abort();
#else
      throw std::system_error(std::make_error_code(std::errc::resource_deadlock_would_occur), "recursive try_lock_shared");
#endif
    }
    if (RwLockPolicy::wait_rlock(state_))
      return false;
    RwLockPolicy::acquire_rlock(state_);
    s_owner_.push_back(tid);
    validator::locked(reinterpret_cast<uintptr_t>(this), tid, true);
    return true;
  }

  void unlock_shared()
  {
    const auto tid = std::this_thread::get_id();
    std::lock_guard<decltype(mtx_)> lk(mtx_);
    if (!is_shared_owner(tid)) {
      // owner thread
#if YAMC_CHECKED_CALL_ABORT
      std::abort();
#else
      throw std::system_error(std::make_error_code(std::errc::operation_not_permitted), "invalid unlock_shared");
#endif
    }
    if (RwLockPolicy::release_rlock(state_)) {
      cv_.notify_all();
    }
    auto result = std::remove(s_owner_.begin(), s_owner_.end(), tid);
    s_owner_.erase(result, s_owner_.end());
    validator::unlocked(reinterpret_cast<uintptr_t>(this), tid, true);
  }
};

} // namespace detail


template <typename RwLockPolicy>
class basic_shared_mutex : private detail::shared_mutex_base<RwLockPolicy> {
  using base = detail::shared_mutex_base<RwLockPolicy>;

public:
  basic_shared_mutex()
  {
    detail::validator::ctor(reinterpret_cast<uintptr_t>(this));
  }

  ~basic_shared_mutex() noexcept(false)
  {
    detail::validator::dtor(reinterpret_cast<uintptr_t>(this));
    base::dtor_precondition("abandoned shared_mutex");
  }

  basic_shared_mutex(const basic_shared_mutex&) = delete;
  basic_shared_mutex& operator=(const basic_shared_mutex&) = delete;

  using base::lock;
  using base::try_lock;
  using base::unlock;

  using base::lock_shared;
  using base::try_lock_shared;
  using base::unlock_shared;
};

using shared_mutex = basic_shared_mutex<YAMC_RWLOCK_SCHED_DEFAULT>;


template <typename RwLockPolicy>
class basic_shared_timed_mutex : private detail::shared_mutex_base<RwLockPolicy> {
  using base = detail::shared_mutex_base<RwLockPolicy>;

  using base::state_;
  using base::e_owner_;
  using base::s_owner_;
  using base::cv_;
  using base::mtx_;

  template<typename Clock, typename Duration>
  bool do_try_lockwait(const std::chrono::time_point<Clock, Duration>& tp, const char* emsg)
  {
    const auto tid = std::this_thread::get_id();
    std::unique_lock<decltype(mtx_)> lk(mtx_);
    if (e_owner_ == tid || base::is_shared_owner(tid)) {
      // non-recursive semantics
#if YAMC_CHECKED_CALL_ABORT
      std::abort();
      (void)emsg; // suppress "unused variable" warning
#else
      throw std::system_error(std::make_error_code(std::errc::resource_deadlock_would_occur), emsg);
#endif
    }
    RwLockPolicy::before_wait_wlock(state_);
    while (RwLockPolicy::wait_wlock(state_)) {
      if (cv_.wait_until(lk, tp) == std::cv_status::timeout) {
        if (!RwLockPolicy::wait_wlock(state_))  // re-check predicate
          break;
        RwLockPolicy::after_wait_wlock(state_);
        return false;
      }
    }
    RwLockPolicy::after_wait_wlock(state_);
    RwLockPolicy::acquire_wlock(state_);
    e_owner_ = tid;
    detail::validator::locked(reinterpret_cast<uintptr_t>(this), tid, false);
    return true;
  }

  template<typename Clock, typename Duration>
  bool do_try_lock_sharedwait(const std::chrono::time_point<Clock, Duration>& tp, const char* emsg)
  {
    const auto tid = std::this_thread::get_id();
    std::unique_lock<decltype(mtx_)> lk(mtx_);
    if (e_owner_ == tid || base::is_shared_owner(tid)) {
      // non-recursive semantics
#if YAMC_CHECKED_CALL_ABORT
      std::abort();
      (void)emsg; // suppress "unused variable" warning
#else
      throw std::system_error(std::make_error_code(std::errc::resource_deadlock_would_occur), emsg);
#endif
    }
    while (RwLockPolicy::wait_rlock(state_)) {
      if (cv_.wait_until(lk, tp) == std::cv_status::timeout) {
        if (!RwLockPolicy::wait_rlock(state_))  // re-check predicate
          break;
        return false;
      }
    }
    RwLockPolicy::acquire_rlock(state_);
    s_owner_.push_back(tid);
    detail::validator::locked(reinterpret_cast<uintptr_t>(this), tid, true);
    return true;
  }

public:
  basic_shared_timed_mutex()
  {
    detail::validator::ctor(reinterpret_cast<uintptr_t>(this));
  }

  ~basic_shared_timed_mutex() noexcept(false)
  {
    detail::validator::dtor(reinterpret_cast<uintptr_t>(this));
    base::dtor_precondition("abandoned shared_timed_mutex");
  }

  basic_shared_timed_mutex(const basic_shared_timed_mutex&) = delete;
  basic_shared_timed_mutex& operator=(const basic_shared_timed_mutex&) = delete;

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

  using base::lock_shared;
  using base::try_lock_shared;
  using base::unlock_shared;

  template<typename Rep, typename Period>
  bool try_lock_shared_for(const std::chrono::duration<Rep, Period>& duration)
  {
    const auto tp = std::chrono::steady_clock::now() + duration;
    return do_try_lock_sharedwait(tp, "recursive try_lock_shared_for");
  }

  template<typename Clock, typename Duration>
  bool try_lock_shared_until(const std::chrono::time_point<Clock, Duration>& tp)
  {
    return do_try_lock_sharedwait(tp, "recursive try_lock_shared_until");
  }
};

using shared_timed_mutex = basic_shared_timed_mutex<YAMC_RWLOCK_SCHED_DEFAULT>;

} // namespace checked
} // namespace yamc

#endif
