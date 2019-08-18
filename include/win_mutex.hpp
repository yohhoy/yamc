/*
 * win_mutex.hpp
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
#ifndef WIN_MUTEX_HPP_
#define WIN_MUTEX_HPP_

#include <chrono>
#include <system_error>
// Windows mutex
#include <windows.h>


/// Enable acculate timeout for yamc::win::* primitives
#ifndef YAMC_WIN_ACCURATE_TIMEOUT
#define YAMC_WIN_ACCURATE_TIMEOUT  1
#endif


namespace yamc {

/*
 * Windows mutex (critical section) wrapper on Windows platform
 *
 * - yamc::win::mutex
 * - yamc::win::recursive_mutex
 * - yamc::win::timed_mutex
 * - yamc::win::recursive_timed_mutex
 *
 * Windows' mutex objects and critical section objects support recursive locking.
 * https://docs.microsoft.com/windows/win32/sync/critical-section-objects
 * https://docs.microsoft.com/windows/win32/sync/mutex-objects
 */
namespace win {

namespace detail {

class mutex_base {
protected:
  ::CRITICAL_SECTION cs_;

  mutex_base() noexcept
  {
    ::InitializeCriticalSection(&cs_);
  }

  ~mutex_base()
  {
    ::DeleteCriticalSection(&cs_);
  }

  void lock()
  {
    ::EnterCriticalSection(&cs_);
  }

  bool try_lock()
  {
    return ::TryEnterCriticalSection(&cs_);
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


class timed_mutex_base {
protected:
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

  timed_mutex_base()
  {
    hmtx_ = ::CreateMutex(NULL, FALSE, NULL);
    if (hmtx_ == NULL) {
      // [thread.mutex.requirements.mutex]
      // invalid_argument - if any native handle type manipulated as part of mutex construction is incorrect.
      throw std::system_error(std::make_error_code(std::errc::invalid_argument), "CreateMutex");
    }
  }

  ~timed_mutex_base()
  {
    ::CloseHandle(hmtx_);
  }

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

} // namespace detail


class mutex : private detail::mutex_base {
  using base = detail::mutex_base;

public:
  /*constexpr*/ mutex() noexcept = default;
  ~mutex() = default;

  mutex(const mutex&) = delete;
  mutex& operator=(const mutex&) = delete;

  using base::lock;
  using base::try_lock;
  using base::unlock;

  using base::native_handle_type;
  using base::native_handle;
};


class recursive_mutex : private detail::mutex_base {
  using base = detail::mutex_base;

public:
  recursive_mutex() = default;
  ~recursive_mutex() = default;

  recursive_mutex(const recursive_mutex&) = delete;
  recursive_mutex& operator=(const recursive_mutex&) = delete;

  using base::lock;
  using base::try_lock;
  using base::unlock;

  using base::native_handle_type;
  using base::native_handle;
};


class timed_mutex : private detail::timed_mutex_base {
  using base = detail::timed_mutex_base;

public:
  timed_mutex() = default;
  ~timed_mutex() = default;

  timed_mutex(const timed_mutex&) = delete;
  timed_mutex& operator=(const timed_mutex&) = delete;

  using base::lock;
  using base::try_lock;
  using base::try_lock_for;
  using base::try_lock_until;
  using base::unlock;

  using base::native_handle_type;
  using base::native_handle;
};


class recursive_timed_mutex : private detail::timed_mutex_base {
  using base = detail::timed_mutex_base;

public:
  recursive_timed_mutex() = default;
  ~recursive_timed_mutex() = default;

  recursive_timed_mutex(const recursive_timed_mutex&) = delete;
  recursive_timed_mutex& operator=(const recursive_timed_mutex&) = delete;

  using base::lock;
  using base::try_lock;
  using base::try_lock_for;
  using base::try_lock_until;
  using base::unlock;

  using base::native_handle_type;
  using base::native_handle;
};

} // namespace win
} // namespace yamc

#endif
