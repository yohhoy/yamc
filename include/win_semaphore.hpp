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
// Windows semaphore
#include <windows.h>


/*
 * Semaphores in C++20 Standard Library for Windows platform
 *
 * - yamc::win::counting_semaphore<least_max_value>
 * - yamc::win::binary_semaphore
 *
 * This implementation use native Win32 semaphore.
 * https://docs.microsoft.com/en-us/windows/win32/sync/semaphore-objects
 */
namespace yamc {

namespace win {

template <std::ptrdiff_t least_max_value = 0x7FFFFFFF>
class counting_semaphore {
  ::HANDLE hsem_;

  template<class Rep, class Period>
  bool do_try_acquirewait(const std::chrono::duration<Rep, Period>& timeout)
  {
    DWORD msec = (DWORD)std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count();
    // WaitForSingleObject() timeout has millisecond precision.
    // https://docs.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-waitforsingleobject
    DWORD result = ::WaitForSingleObject(hsem_, msec);
    return (result == WAIT_OBJECT_0);
  }

public:
  static constexpr std::ptrdiff_t (max)() noexcept
  {
    // Windows.h header defines max() and min() function-like macros that make trouble,
    // we put parenthesis around `max` identifier to prevent unexpected macro expansion.
    static_assert(0 < least_max_value, "least_max_value shall be greater than zero");
    return least_max_value;
  }

  /*constexpr*/ explicit counting_semaphore(std::ptrdiff_t desired)
  {
    assert(0 <= desired && desired <= (max)());
    hsem_ = ::CreateSemaphore(NULL, (LONG)desired, (LONG)((max)()), NULL);
    assert(hsem_ != NULL);
    // counting_semaphore constructor throws nothing.
  }

  ~counting_semaphore()
  {
    ::CloseHandle(hsem_);
  }

  counting_semaphore(const counting_semaphore&) = delete;
  counting_semaphore& operator=(const counting_semaphore&) = delete;

  void release(std::ptrdiff_t update = 1)
  {
    ::ReleaseSemaphore(hsem_, (LONG)update, NULL);
  }

  void acquire()
  {
    DWORD result = ::WaitForSingleObject(hsem_, INFINITE);
    assert(result == WAIT_OBJECT_0);
  }

  bool try_acquire() noexcept
  {
    return (::WaitForSingleObject(hsem_, 0) == WAIT_OBJECT_0);
  }

  template<class Rep, class Period>
  bool try_acquire_for(const std::chrono::duration<Rep, Period>& rel_time)
  {
    return do_try_acquirewait(rel_time);
  }

  template<class Clock, class Duration>
  bool try_acquire_until(const std::chrono::time_point<Clock, Duration>& abs_time)
  {
    return do_try_acquirewait(abs_time - Clock::now());
  }
};

using binary_semaphore = counting_semaphore<1>;

} // namespace win
} // namespace yamc

#endif
