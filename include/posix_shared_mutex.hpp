/*
 * posix_shared_mutex.hpp
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
#ifndef POSIX_SHARED_MUTEX_HPP_
#define POSIX_SHARED_MUTEX_HPP_

// POSIX(pthreads) mutex
#include <pthread.h>
#include <time.h>


/// enable yamc::posix::shared_timed_mutex
#ifndef YAMC_ENABLE_POSIX_SHARED_TIMED_MUTEX
#if defined(__APPLE__)
// macOS doesn't have pthread_rwlock_timed{rd,wr}lock()
#define YAMC_ENABLE_POSIX_SHARED_TIMED_MUTEX  0
#else
#define YAMC_ENABLE_POSIX_SHARED_TIMED_MUTEX  1
#endif
#endif


namespace yamc {

/*
 * Pthreads rwlock wrapper on POSIX-compatible platform
 *
 * - yamc::posix::shared_mutex
 * - yamc::posix::shared_timed_mutex [optional]
 *
 * Some platform doesn't support locking operation with timeout.
 *
 * http://pubs.opengroup.org/onlinepubs/9699919799/basedefs/pthread.h.html
 * https://pubs.opengroup.org/onlinepubs/9699919799/functions/pthread_rwlock_timedrdlock.html
 * https://pubs.opengroup.org/onlinepubs/9699919799/functions/pthread_rwlock_timedwrlock.html
 */
namespace posix {

namespace detail {

class shared_mutex_base {
protected:
#if defined(PTHREAD_RWLOCK_INITIALIZER)
  ::pthread_rwlock_t rwlock_ = PTHREAD_RWLOCK_INITIALIZER;
#elif defined(PTHREAD_RWLOCK_INITIALIZER_NP)
  ::pthread_rwlock_t rwlock_ = PTHREAD_RWLOCK_INITIALIZER_NP;
#endif

  shared_mutex_base() = default;
  ~shared_mutex_base()
  {
    ::pthread_rwlock_destroy(&rwlock_);
  }

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
};

} // namespace detail

class shared_mutex : private detail::shared_mutex_base {
  using base = detail::shared_mutex_base;

public:
  shared_mutex() = default;
  ~shared_mutex() = default;

  shared_mutex(const shared_mutex&) = delete;
  shared_mutex& operator=(const shared_mutex&) = delete;

  using base::lock;
  using base::try_lock;
  using base::unlock;

  using base::lock_shared;
  using base::try_lock_shared;
  using base::unlock_shared;
};

#if YAMC_ENABLE_POSIX_SHARED_TIMED_MUTEX
class shared_timed_mutex : private detail::shared_mutex_base {
  using base = detail::shared_mutex_base;

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

public:
  shared_timed_mutex() = default;
  ~shared_timed_mutex() = default;

  shared_timed_mutex(const shared_timed_mutex&) = delete;
  shared_timed_mutex& operator=(const shared_timed_mutex&) = delete;

  using base::lock;
  using base::try_lock;
  using base::unlock;

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

  using base::lock_shared;
  using base::try_lock_shared;
  using base::unlock_shared;

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
};
#endif // YAMC_ENABLE_POSIX_SHARED_TIMED_MUTEX

} // namespace posix
} // namespace yamc

#endif
