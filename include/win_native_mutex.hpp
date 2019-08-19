/*
 * win_native_mutex.hpp
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
#ifndef WIN_NATIVE_MUTEX_HPP_
#define WIN_NATIVE_MUTEX_HPP_

#include <chrono>
#include <system_error>
// Windows mutex
#include <windows.h>


/// Enable accurate timeout for yamc::win::* primitives
#ifndef YAMC_WIN_ACCURATE_TIMEOUT
#define YAMC_WIN_ACCURATE_TIMEOUT  1
#endif


namespace yamc {

/*
 * Native Mutex/CriticalSection/SlimRWLock wrapper on Windows platform
 *
 * - yamc::win::native_mutex
 * - yamc::win::critical_section
 * - yamc::win::slim_rwlock
 *
 * Characteristics:
 *   |                  | recursive |  timed  |  shared |
 *   +------------------+-----------+---------+---------+
 *   | native_mutex     |  support  | support |   N/A   |
 *   | critical_section |  support  |   N/A   |   N/A   |
 *   | slim_rwlock      |    N/A    |   N/A   | support |
 *
 * https://docs.microsoft.com/windows/win32/sync/mutex-objects
 * https://docs.microsoft.com/windows/win32/sync/critical-section-objects
 * https://docs.microsoft.com/windows/win32/sync/slim-reader-writer--srw--locks
 */
namespace win {

class native_mutex {
  ::HANDLE hmtx_ = NULL;

  template<class Rep, class Period>
  bool do_try_lockwait(const std::chrono::duration<Rep, Period>& timeout)
  {
    using namespace std::chrono;
    // round up timeout to milliseconds precision
    DWORD timeout_in_msec = static_cast<DWORD>(duration_cast<milliseconds>(timeout + nanoseconds{999999}).count());
    DWORD result = ::WaitForSingleObject(hmtx_, timeout_in_msec);
#if YAMC_WIN_ACCURATE_TIMEOUT
    if (result == WAIT_TIMEOUT && 0 < timeout_in_msec) {
      // Win32 wait functions will return early than specified timeout interval by design.
      // (https://docs.microsoft.com/windows/win32/sync/wait-functions for more details)
      //
      // The current thread sleep one more "tick" to guarantee timing specification in C++ Standard,
      // that actual timeout interval shall be longer than requested timeout of try_lock_*().
      ::Sleep(1);
    }
#endif
    if (result != WAIT_OBJECT_0 && result != WAIT_TIMEOUT) {
      // [thread.mutex.requirements.mutex]
      // resource_unavailable_try_again - if any native handle type manipulated is not available.
      throw std::system_error(std::make_error_code(std::errc::resource_unavailable_try_again), "WaitForSingleObject");
    }
    return (result == WAIT_OBJECT_0);
  }

public:
  native_mutex()
  {
    hmtx_ = ::CreateMutex(NULL, FALSE, NULL);
    if (hmtx_ == NULL) {
      // [thread.mutex.requirements.mutex]
      // invalid_argument - if any native handle type manipulated as part of mutex construction is incorrect.
      throw std::system_error(std::make_error_code(std::errc::invalid_argument), "CreateMutex");
    }
  }

  ~native_mutex()
  {
    ::CloseHandle(hmtx_);
  }

  native_mutex(const native_mutex&) = delete;
  native_mutex& operator=(const native_mutex&) = delete;

  void lock()
  {
    DWORD result = ::WaitForSingleObject(hmtx_, INFINITE);
    if (result != WAIT_OBJECT_0) {
      // [thread.mutex.requirements.mutex]
      // resource_unavailable_try_again - if any native handle type manipulated is not available.
      throw std::system_error(std::make_error_code(std::errc::resource_unavailable_try_again), "WaitForSingleObject");
    }
  }

  bool try_lock()
  {
    return (::WaitForSingleObject(hmtx_, 0) == WAIT_OBJECT_0);
  }

  template<class Rep, class Period>
  bool try_lock_for(const std::chrono::duration<Rep, Period>& rel_time)
  {
    return do_try_lockwait(rel_time);
  }

  template<class Clock, class Duration>
  bool try_lock_until(const std::chrono::time_point<Clock, Duration>& abs_time)
  {
    return do_try_lockwait(abs_time - Clock::now());
  }

  void unlock()
  {
    ::ReleaseMutex(hmtx_);
  }

  using native_handle_type = ::HANDLE;
  native_handle_type native_handle()
  {
    return &hmtx_;
  }
};


class critical_section {
  ::CRITICAL_SECTION cs_;

public:
  critical_section() noexcept
  {
    ::InitializeCriticalSection(&cs_);
  }

  ~critical_section()
  {
    ::DeleteCriticalSection(&cs_);
  }

  critical_section(const critical_section&) = delete;
  critical_section& operator=(const critical_section&) = delete;

  void lock()
  {
    ::EnterCriticalSection(&cs_);
  }

  bool try_lock()
  {
    return (::TryEnterCriticalSection(&cs_) != 0);
    // explicit comparison to 0 to suppress "warning C4800"
  }

  void unlock()
  {
    ::LeaveCriticalSection(&cs_);
  }

  using native_handle_type = ::CRITICAL_SECTION*;
  native_handle_type native_handle()
  {
    return &cs_;
  }
};


class slim_rwlock {
  ::SRWLOCK srwlock_ = SRWLOCK_INIT;

public:
  slim_rwlock() = default;
  ~slim_rwlock() = default;

  slim_rwlock(const slim_rwlock&) = delete;
  slim_rwlock& operator=(const slim_rwlock&) = delete;

  void lock()
  {
    ::AcquireSRWLockExclusive(&srwlock_);
  }

  bool try_lock()
  {
    return (::TryAcquireSRWLockExclusive(&srwlock_) != 0);
    // explicit comparison to 0 to suppress "warning C4800"
  }

  void unlock()
  {
    ::ReleaseSRWLockExclusive(&srwlock_);
  }

  void lock_shared()
  {
    ::AcquireSRWLockShared(&srwlock_);
  }

  bool try_lock_shared()
  {
    return (::TryAcquireSRWLockShared(&srwlock_) != 0);
    // explicit comparison to 0 to suppress "warning C4800"
  }

  void unlock_shared()
  {
    ::ReleaseSRWLockShared(&srwlock_);
  }

  using native_handle_type = ::SRWLOCK*;
  native_handle_type native_handle()
  {
    return &srwlock_;
  }
};


using mutex = critical_section;
using recursive_mutex = critical_section;
using timed_mutex = native_mutex;
using recursive_timed_mutex = native_mutex;

using shared_mutex = slim_rwlock;
// Windows have no native primitives equivalent to shared_timed_mutex


} // namespace win
} // namespace yamc

#endif
