/*
 * posix_mutex.hpp
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
#ifndef POSIX_MUTEX_HPP_
#define POSIX_MUTEX_HPP_

// POSIX(pthreads) mutex
#include <pthread.h>
#include <time.h>


/// enable yamc::posix::(recursive_)timed_mutex
#ifndef YAMC_ENABLE_POSIX_TIMED_MUTEX
#if defined(__APPLE__)
// macOS doesn't have pthread_mutex_timedlock()
#define YAMC_ENABLE_POSIX_TIMED_MUTEX  0
#else
#define YAMC_ENABLE_POSIX_TIMED_MUTEX  1
#endif
#endif


namespace yamc {

/*
 * Pthreads mutex wrapper on POSIX-compatible platform
 *
 * - yamc::posix::mutex
 * - yamc::posix::recursive_mutex
 * - yamc::posix::timed_mutex [optional]
 * - yamc::posix::recursive_timed_mutex [optional]
 *
 * Some platform doesn't support locking operation with timeout.
 *
 * http://pubs.opengroup.org/onlinepubs/9699919799/basedefs/pthread.h.html
 * http://pubs.opengroup.org/onlinepubs/9699919799/functions/pthread_mutex_timedlock.html
 */
namespace posix {

class mutex {
#if defined(PTHREAD_MUTEX_INITIALIZER)
  ::pthread_mutex_t mtx_ = PTHREAD_MUTEX_INITIALIZER;
#elif defined(PTHREAD_MUTEX_INITIALIZER_NP)
  ::pthread_mutex_t mtx_ = PTHREAD_MUTEX_INITIALIZER_NP;
#endif

public:
  constexpr mutex() noexcept = default;

  ~mutex()
  {
    ::pthread_mutex_destroy(&mtx_);
  }

  mutex(const mutex&) = delete;
  mutex& operator=(const mutex&) = delete;

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

  using native_handle_type = ::pthread_mutex_t;
  native_handle_type native_handle()
  {
    return mtx_;
  }
};


class recursive_mutex {
#if defined(PTHREAD_RECURSIVE_MUTEX_INITIALIZER)
  ::pthread_mutex_t mtx_ = PTHREAD_RECURSIVE_MUTEX_INITIALIZER;
#elif defined(PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP)
  ::pthread_mutex_t mtx_ = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
#endif

public:
  recursive_mutex() = default;

  ~recursive_mutex()
  {
    ::pthread_mutex_destroy(&mtx_);
  }

  recursive_mutex(const recursive_mutex&) = delete;
  recursive_mutex& operator=(const recursive_mutex&) = delete;

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

  using native_handle_type = ::pthread_mutex_t;
  native_handle_type native_handle()
  {
    return mtx_;
  }
};


#if YAMC_ENABLE_POSIX_TIMED_MUTEX
class timed_mutex {
#if defined(PTHREAD_MUTEX_INITIALIZER)
  ::pthread_mutex_t mtx_ = PTHREAD_MUTEX_INITIALIZER;
#elif defined(PTHREAD_MUTEX_INITIALIZER_NP)
  ::pthread_mutex_t mtx_ = PTHREAD_MUTEX_INITIALIZER_NP;
#endif

  bool do_try_lockwait(const std::chrono::system_clock::time_point& tp)
  {
    using namespace std::chrono;
    struct ::timespec abs_timeout;
    abs_timeout.tv_sec = system_clock::to_time_t(tp);
    abs_timeout.tv_nsec = (long)(duration_cast<nanoseconds>(tp.time_since_epoch()).count() % 1000000000);
    return (::pthread_mutex_timedlock(&mtx_, &abs_timeout) == 0);
  }

public:
  timed_mutex() = default;

  ~timed_mutex()
  {
    ::pthread_mutex_destroy(&mtx_);
  }

  timed_mutex(const timed_mutex&) = delete;
  timed_mutex& operator=(const timed_mutex&) = delete;

  void lock()
  {
    ::pthread_mutex_lock(&mtx_);
  }

  bool try_lock()
  {
    return (::pthread_mutex_trylock(&mtx_) == 0);
  }

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

  void unlock()
  {
    ::pthread_mutex_unlock(&mtx_);
  }

  using native_handle_type = ::pthread_mutex_t;
  native_handle_type native_handle()
  {
    return mtx_;
  }
};


class recursive_timed_mutex {
#if defined(PTHREAD_RECURSIVE_MUTEX_INITIALIZER)
  ::pthread_mutex_t mtx_ = PTHREAD_RECURSIVE_MUTEX_INITIALIZER;
#elif defined(PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP)
  ::pthread_mutex_t mtx_ = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
#endif

  bool do_try_lockwait(const std::chrono::system_clock::time_point& tp)
  {
    using namespace std::chrono;
    struct ::timespec abs_timeout;
    abs_timeout.tv_sec = system_clock::to_time_t(tp);
    abs_timeout.tv_nsec = (long)(duration_cast<nanoseconds>(tp.time_since_epoch()).count() % 1000000000);
    return (::pthread_mutex_timedlock(&mtx_, &abs_timeout) == 0);
  }

public:
  recursive_timed_mutex() = default;

  ~recursive_timed_mutex()
  {
    ::pthread_mutex_destroy(&mtx_);
  }

  recursive_timed_mutex(const recursive_timed_mutex&) = delete;
  recursive_timed_mutex& operator=(const recursive_timed_mutex&) = delete;

  void lock()
  {
    ::pthread_mutex_lock(&mtx_);
  }

  bool try_lock()
  {
    return (::pthread_mutex_trylock(&mtx_) == 0);
  }

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

  void unlock()
  {
    ::pthread_mutex_unlock(&mtx_);
  }

  using native_handle_type = ::pthread_mutex_t;
  native_handle_type native_handle()
  {
    return mtx_;
  }
};
#endif // YAMC_ENABLE_POSIX_TIMED_MUTEX


} // namespace posix
} // namespace yamc

#endif
