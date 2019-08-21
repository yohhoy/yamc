/*
 * posix_native_mutex.hpp
 *
 * MIT License
 *
 * Copyright (c) 2019 yohhoy
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
#ifndef POSIX_NATIVE_MUTEX_HPP_
#define POSIX_NATIVE_MUTEX_HPP_

#include <chrono>
// POSIX(pthreads) mutex
#include <pthread.h>
#include <time.h>


#if defined(__APPLE__)
// macOS doesn't have timed locking functions
#define YAMC_POSIX_TIMEOUT_SUPPORTED   0
// macOS doesn't provide pthread_spinlock_t
#define YAMC_POSIX_SPINLOCK_SUPPORTED  0
#else
#define YAMC_POSIX_TIMEOUT_SUPPORTED   1
#define YAMC_POSIX_SPINLOCK_SUPPORTED  1
#endif


namespace yamc {

/*
 * Pthreads mutex wrapper on POSIX-compatible platform
 *
 * - yamc::posix::native_mutex
 * - yamc::posix::native_recursive_mutex
 * - yamc::posix::rwlock
 * - yamc::posix::spinlock [conditional]
 *
 * Some platform doesn't support locking operation with timeout.
 * Some platform doesn't provide spinlock object (pthread_spinlock_t).
 * https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/pthread.h.html
 * https://pubs.opengroup.org/onlinepubs/9699919799/functions/pthread_mutex_timedlock.html
 * https://pubs.opengroup.org/onlinepubs/9699919799/functions/pthread_rwlock_timedrdlock.html
 * https://pubs.opengroup.org/onlinepubs/9699919799/functions/pthread_rwlock_timedwrlock.html
 * https://pubs.opengroup.org/onlinepubs/9699919799/functions/pthread_spin_init.html
 */
namespace posix {

class native_mutex {
#if defined(PTHREAD_MUTEX_INITIALIZER)
  // POSIX.1 defines PTHREAD_MUTEX_INITIALIZER macro to initialize default mutex.
  // https://pubs.opengroup.org/onlinepubs/9699919799/functions/pthread_mutex_destroy.html
  ::pthread_mutex_t mtx_ = PTHREAD_MUTEX_INITIALIZER;
#elif defined(PTHREAD_MUTEX_INITIALIZER_NP)
  ::pthread_mutex_t mtx_ = PTHREAD_MUTEX_INITIALIZER_NP;
#endif

#if YAMC_POSIX_TIMEOUT_SUPPORTED
  bool do_try_lockwait(const std::chrono::system_clock::time_point& tp)
  {
    using namespace std::chrono;
    struct ::timespec abs_timeout;
    abs_timeout.tv_sec = system_clock::to_time_t(tp);
    abs_timeout.tv_nsec = (long)(duration_cast<nanoseconds>(tp.time_since_epoch()).count() % 1000000000);
    return (::pthread_mutex_timedlock(&mtx_, &abs_timeout) == 0);
  }
#endif

public:
  constexpr native_mutex() noexcept = default;

  ~native_mutex()
  {
    ::pthread_mutex_destroy(&mtx_);
  }

  native_mutex(const native_mutex&) = delete;
  native_mutex& operator=(const native_mutex&) = delete;

  void lock()
  {
    ::pthread_mutex_lock(&mtx_);
  }

  bool try_lock()
  {
    return (::pthread_mutex_trylock(&mtx_) == 0);
  }

  void unlock()
  {
    ::pthread_mutex_unlock(&mtx_);
  }

#if YAMC_POSIX_TIMEOUT_SUPPORTED
 template<class Rep, class Period>
  bool try_lock_for(const std::chrono::duration<Rep, Period>& rel_time)
  {
    // C++ Standard says '_for'-suffixed timeout function shall use steady clock,
    // but we use std::chrono::system_clock which may or may not be steady.
    const auto tp = std::chrono::system_clock::now() + rel_time;
    return do_try_lockwait(tp);
  }

  template<class Clock, class Duration>
  bool try_lock_until(const std::chrono::time_point<Clock, Duration>& abs_time)
  {
    static_assert(std::is_same<Clock, std::chrono::system_clock>::value, "support only system_clock");
    return do_try_lockwait(abs_time);
  }
#endif

  using native_handle_type = ::pthread_mutex_t*;
  native_handle_type native_handle()
  {
    return &mtx_;
  }
};


class native_recursive_mutex {
  // POSIX.1 does NOT define PTHREAD_RECURSIVE_MUTEX_INITIALIZER-like macro,
  // - Linux defines PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP macro,
  // - macOS defines PTHREAD_RECURSIVE_MUTEX_INITIALIZER macro.
#if defined(PTHREAD_RECURSIVE_MUTEX_INITIALIZER)
  ::pthread_mutex_t mtx_ = PTHREAD_RECURSIVE_MUTEX_INITIALIZER;
#elif defined(PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP)
  ::pthread_mutex_t mtx_ = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
#else
  ::pthread_mutex_t mtx_;
#endif

#if YAMC_POSIX_TIMEOUT_SUPPORTED
  bool do_try_lockwait(const std::chrono::system_clock::time_point& tp)
  {
    using namespace std::chrono;
    struct ::timespec abs_timeout;
    abs_timeout.tv_sec = system_clock::to_time_t(tp);
    abs_timeout.tv_nsec = (long)(duration_cast<nanoseconds>(tp.time_since_epoch()).count() % 1000000000);
    return (::pthread_mutex_timedlock(&mtx_, &abs_timeout) == 0);
  }
#endif

public:
  native_recursive_mutex()
  {
#if !defined(PTHREAD_RECURSIVE_MUTEX_INITIALIZER) && !defined(PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP)
    ::pthread_mutexattr_t attr;
    ::pthread_mutexattr_init(&attr);
    ::pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    ::pthread_mutex_init(&mtx_, &attr);
    ::pthread_mutexattr_destroy(&attr);
#endif
  }

  ~native_recursive_mutex()
  {
    ::pthread_mutex_destroy(&mtx_);
  }

  native_recursive_mutex(const native_recursive_mutex&) = delete;
  native_recursive_mutex& operator=(const native_recursive_mutex&) = delete;

  void lock()
  {
    ::pthread_mutex_lock(&mtx_);
  }

  bool try_lock()
  {
    return (::pthread_mutex_trylock(&mtx_) == 0);
  }

  void unlock()
  {
    ::pthread_mutex_unlock(&mtx_);
  }

#if YAMC_POSIX_TIMEOUT_SUPPORTED
  template<class Rep, class Period>
  bool try_lock_for(const std::chrono::duration<Rep, Period>& rel_time)
  {
    // C++ Standard says '_for'-suffixed timeout function shall use steady clock,
    // but we use std::chrono::system_clock which may or may not be steady.
    const auto tp = std::chrono::system_clock::now() + rel_time;
    return do_try_lockwait(tp);
  }

  template<class Clock, class Duration>
  bool try_lock_until(const std::chrono::time_point<Clock, Duration>& abs_time)
  {
    static_assert(std::is_same<Clock, std::chrono::system_clock>::value, "support only system_clock");
    return do_try_lockwait(abs_time);
  }
#endif // YAMC_POSIX_TIMEOUT_SUPPORTED

  using native_handle_type = ::pthread_mutex_t*;
  native_handle_type native_handle()
  {
    return &mtx_;
  }
};


class rwlock {
#if defined(PTHREAD_RWLOCK_INITIALIZER)
  // POSIX.1-2001/SUSv3 once deleted PTHREAD_RWLOCK_INITIALIZER macro,
  // POSIX.1-2008/SUSv4 defines PTHREAD_RWLOCK_INITIALIZER macro again.
  // https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/pthread.h.html
  ::pthread_rwlock_t rwlock_ = PTHREAD_RWLOCK_INITIALIZER;
#elif defined(PTHREAD_RWLOCK_INITIALIZER_NP)
  ::pthread_rwlock_t rwlock_ = PTHREAD_RWLOCK_INITIALIZER_NP;
#endif

#if YAMC_POSIX_TIMEOUT_SUPPORTED
  bool do_try_lockwait(const std::chrono::system_clock::time_point& tp)
  {
    using namespace std::chrono;
    struct ::timespec abs_timeout;
    abs_timeout.tv_sec = system_clock::to_time_t(tp);
    abs_timeout.tv_nsec = (long)(duration_cast<nanoseconds>(tp.time_since_epoch()).count() % 1000000000);
    return (::pthread_rwlock_timedwrlock(&rwlock_, &abs_timeout) == 0);
  }

  bool do_try_lock_sharedwait(const std::chrono::system_clock::time_point& tp)
  {
    using namespace std::chrono;
    struct ::timespec abs_timeout;
    abs_timeout.tv_sec = system_clock::to_time_t(tp);
    abs_timeout.tv_nsec = (long)(duration_cast<nanoseconds>(tp.time_since_epoch()).count() % 1000000000);
    return (::pthread_rwlock_timedrdlock(&rwlock_, &abs_timeout) == 0);
  }
#endif

public:
  rwlock() = default;
  ~rwlock()
  {
    ::pthread_rwlock_destroy(&rwlock_);
  }

  rwlock(const rwlock&) = delete;
  rwlock& operator=(const rwlock&) = delete;

  void lock()
  {
    ::pthread_rwlock_wrlock(&rwlock_);
  }

  bool try_lock()
  {
    return (::pthread_rwlock_trywrlock(&rwlock_) == 0);
  }

  void unlock()
  {
    ::pthread_rwlock_unlock(&rwlock_);
  }

  void lock_shared()
  {
    ::pthread_rwlock_rdlock(&rwlock_);
  }

  bool try_lock_shared()
  {
    return (::pthread_rwlock_tryrdlock(&rwlock_) == 0);
  }

  void unlock_shared()
  {
    ::pthread_rwlock_unlock(&rwlock_);
  }

#if YAMC_POSIX_TIMEOUT_SUPPORTED
  template<typename Rep, typename Period>
  bool try_lock_for(const std::chrono::duration<Rep, Period>& rel_time)
  {
    // C++ Standard says '_for'-suffixed timeout function shall use steady clock,
    // but we use std::chrono::system_clock which may or may not be steady.
    const auto tp = std::chrono::system_clock::now() + rel_time;
    return do_try_lockwait(tp);
  }

  template<typename Clock, typename Duration>
  bool try_lock_until(const std::chrono::time_point<Clock, Duration>& abs_time)
  {
    static_assert(std::is_same<Clock, std::chrono::system_clock>::value, "support only system_clock");
    return do_try_lockwait(abs_time);
  }

  template<typename Rep, typename Period>
  bool try_lock_shared_for(const std::chrono::duration<Rep, Period>& rel_time)
  {
    // C++ Standard says '_for'-suffixed timeout function shall use steady clock,
    // but we use std::chrono::system_clock which may or may not be steady.
    const auto tp = std::chrono::system_clock::now() + rel_time;
    return do_try_lock_sharedwait(tp);
  }

  template<typename Clock, typename Duration>
  bool try_lock_shared_until(const std::chrono::time_point<Clock, Duration>& abs_time)
  {
    static_assert(std::is_same<Clock, std::chrono::system_clock>::value, "support only system_clock");
    return do_try_lock_sharedwait(abs_time);
  }
#endif // YAMC_POSIX_TIMEOUT_SUPPORTED

  using native_handle_type = ::pthread_rwlock_t*;
  native_handle_type native_handle()
  {
    return &rwlock_;
  }
};


#if YAMC_POSIX_SPINLOCK_SUPPORTED
class spinlock {
  ::pthread_spinlock_t slock_;

public:
  /*constexpr*/ spinlock() noexcept
  {
    ::pthread_spin_init(&slock_, 0);
  }

  ~spinlock()
  {
    ::pthread_spin_destroy(&slock_);
  }

  spinlock(const spinlock&) = delete;
  spinlock& operator=(const spinlock&) = delete;

  void lock()
  {
    ::pthread_spin_lock(&slock_);
  }

  bool try_lock()
  {
    return (::pthread_spin_trylock(&slock_) == 0);
  }

  void unlock()
  {
    ::pthread_spin_unlock(&slock_);
  }

  using native_handle_type = ::pthread_spinlock_t*;
  native_handle_type native_handle()
  {
    return &slock_;
  }
};
#endif // YAMC_POSIX_SPINLOCK_SUPPORTED


using mutex = native_mutex;
using recursive_mutex = native_recursive_mutex;
using timed_mutex = native_mutex;
using recursive_timed_mutex = native_recursive_mutex;

using shared_mutex = rwlock;
using shared_timed_mutex = rwlock;

} // namespace posix
} // namespace yamc

#endif
