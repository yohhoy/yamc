/*
 * win_semaphore.hpp
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
#ifndef WIN_SEMAPHORE_HPP_
#define WIN_SEMAPHORE_HPP_

#include <cassert>
#include <cstddef>
#include <chrono>
#include <system_error>
// Windows semaphore
#include <windows.h>


/// Enable acculate timeout for yamc::win::* primitives
#ifndef YAMC_WIN_ACCURATE_TIMEOUT
#define YAMC_WIN_ACCURATE_TIMEOUT  1
#endif


/*
 * Semaphores in C++20 Standard Library for Windows platform
 *
 * - yamc::win::counting_semaphore<least_max_value>
 * - yamc::win::binary_semaphore
 *
 * This implementation use native Win32 semaphore.
 * https://docs.microsoft.com/windows/win32/sync/semaphore-objects
 */
namespace yamc {

namespace win {

template <std::ptrdiff_t least_max_value = 0x7FFFFFFF>
class counting_semaphore {
  ::HANDLE hsem_ = NULL;

  void validate_native_handle(const char* what_arg)
  {
    if (hsem_ == NULL) {
      // [thread.mutex.requirements.mutex]
      // invalid_argument - if any native handle type manipulated as part of mutex construction is incorrect.
      throw std::system_error(std::make_error_code(std::errc::invalid_argument), what_arg);
    }
  }

  template<class Rep, class Period>
  bool do_try_acquirewait(const std::chrono::duration<Rep, Period>& timeout)
  {
    using namespace std::chrono;
    // round up timeout to milliseconds precision
    DWORD timeout_in_msec = static_cast<DWORD>(duration_cast<milliseconds>(timeout + nanoseconds{999999}).count());
    DWORD result = ::WaitForSingleObject(hsem_, timeout_in_msec);
#if YAMC_WIN_ACCURATE_TIMEOUT
    if (result == WAIT_TIMEOUT && 0 < timeout_in_msec) {
      // Win32 wait functions will return early than specified timeout interval by design.
      // (https://docs.microsoft.com/windows/win32/sync/wait-functions for more details)
      //
      // The current thread sleep one more "tick" to guarantee timing specification in C++ Standard,
      // that actual timeout interval shall be longer than requested timeout of try_acquire_*().
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
  static constexpr std::ptrdiff_t (max)() noexcept
  {
    // Windows.h header defines max() and min() function-like macros that make trouble,
    // we put parenthesis around `max` identifier to prevent unexpected macro expansion.
    // https://docs.microsoft.com/windows/win32/multimedia/max
    static_assert(0 <= least_max_value, "least_max_value shall be non-negative");
    return least_max_value;
  }

  /*constexpr*/ explicit counting_semaphore(std::ptrdiff_t desired)
  {
    assert(0 <= desired && desired <= (max)());
    hsem_ = ::CreateSemaphore(NULL, (LONG)desired, (LONG)((max)()), NULL);
    // counting_semaphore constructor throws nothing.
  }

  ~counting_semaphore()
  {
    if (hsem_ != NULL) {
      ::CloseHandle(hsem_);
    }
  }

  counting_semaphore(const counting_semaphore&) = delete;
  counting_semaphore& operator=(const counting_semaphore&) = delete;

  void release(std::ptrdiff_t update = 1)
  {
    validate_native_handle("counting_semaphore::release");
    BOOL result = ::ReleaseSemaphore(hsem_, (LONG)update, NULL);
    if (!result) {
      // [thread.mutex.requirements.mutex]
      // resource_unavailable_try_again - if any native handle type manipulated is not available.
      throw std::system_error(std::make_error_code(std::errc::resource_unavailable_try_again), "ReleaseSemaphore");
    }
  }

  void acquire()
  {
    validate_native_handle("counting_semaphore::acquire");
    DWORD result = ::WaitForSingleObject(hsem_, INFINITE);
    if (result != WAIT_OBJECT_0) {
      // [thread.mutex.requirements.mutex]
      // resource_unavailable_try_again - if any native handle type manipulated is not available.
      throw std::system_error(std::make_error_code(std::errc::resource_unavailable_try_again), "WaitForSingleObject");
    }
  }

  bool try_acquire() noexcept
  {
    return (hsem_ != NULL && ::WaitForSingleObject(hsem_, 0) == WAIT_OBJECT_0);
  }

  template<class Rep, class Period>
  bool try_acquire_for(const std::chrono::duration<Rep, Period>& rel_time)
  {
    validate_native_handle("counting_semaphore::try_acquire_for");
    return do_try_acquirewait(rel_time);
  }

  template<class Clock, class Duration>
  bool try_acquire_until(const std::chrono::time_point<Clock, Duration>& abs_time)
  {
    validate_native_handle("counting_semaphore::try_acquire_until");
    return do_try_acquirewait(abs_time - Clock::now());
  }
};

using binary_semaphore = counting_semaphore<1>;

} // namespace win
} // namespace yamc

#endif
