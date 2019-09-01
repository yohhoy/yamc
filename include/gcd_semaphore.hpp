/*
 * gcd_semaphore.hpp
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
#ifndef GCD_SEMAPHORE_HPP_
#define GCD_SEMAPHORE_HPP_

#include <cassert>
#include <cstddef>
#include <chrono>
#include <limits>
#include <system_error>
#include <type_traits>
// Grand Central Dispatch (GCD)
#include <dispatch/dispatch.h>


namespace yamc {

/*
 * Semaphores in C++20 Standard Library for macOS/iOS families
 *
 * - yamc::gcd::counting_semaphore<least_max_value>
 * - yamc::gcd::binary_semaphore
 *
 * This implementation use dispatch semaphore of GCD runtime.
 * https://developer.apple.com/documentation/dispatch/dispatchsemaphore
 */
namespace gcd {

namespace detail {

template<typename Clock, typename Duration>
inline
int64_t
from_unix_epoch(const std::chrono::time_point<Clock, Duration>& tp)
{
  using namespace std::chrono;
  auto rel_time = tp - Clock::now();
  return duration_cast<nanoseconds>((system_clock::now() + rel_time).time_since_epoch()).count();
}

template<typename Duration>
inline
int64_t
from_unix_epoch(const std::chrono::time_point<std::chrono::system_clock, Duration>& tp)
{
  // Until C++20, the epoch of std::chrono::system_clock is unspecified,
  // but most implementation use UNIX epoch (19700101T000000Z).
  using namespace std::chrono;
  return duration_cast<nanoseconds>(tp.time_since_epoch()).count();
}

} // namespace detail


template <std::ptrdiff_t least_max_value = std::numeric_limits<long>::max()>
class counting_semaphore {
  ::dispatch_semaphore_t dsema_ = NULL;

  void validate_native_handle(const char* what_arg)
  {
    if (dsema_ == NULL) {
      // [thread.mutex.requirements.mutex]
      // invalid_argument - if any native handle type manipulated as part of mutex construction is incorrect.
      throw std::system_error(std::make_error_code(std::errc::invalid_argument), what_arg);
    }
  }

public:
  static constexpr std::ptrdiff_t max() noexcept
  {
    static_assert(0 <= least_max_value, "least_max_value shall be non-negative");
    // We assume least_max_value equals to std::numeric_limits<long>::max() here.
    // The official document says nothing about the upper limit of initial value.
    return least_max_value;
  }

  /*constexpr*/ explicit counting_semaphore(std::ptrdiff_t desired)
  {
    assert(0 <= desired && desired <= max());
    dsema_ = ::dispatch_semaphore_create((long)desired);
    // counting_semaphore constructor throws nothing.
  }

  ~counting_semaphore()
  {
    // dispatch_semaphore_create() function is declared with DISPATCH_MALLOC,
    // alias of GNU __malloc__ attribute. We need free() when no ObjC runtime.
    ::free(dsema_);
  }

  counting_semaphore(const counting_semaphore&) = delete;
  counting_semaphore& operator=(const counting_semaphore&) = delete;

  void release(std::ptrdiff_t update = 1)
  {
    validate_native_handle("counting_semaphore::release");
    while (0 < update--) {
      ::dispatch_semaphore_signal(dsema_);
      // dispatch_semaphore_signal() function return number of threads signaled.
    }
  }

  void acquire()
  {
    validate_native_handle("counting_semaphore::acquire");
    long result = ::dispatch_semaphore_wait(dsema_, DISPATCH_TIME_FOREVER);
    if (result != KERN_SUCCESS) {
      // [thread.mutex.requirements.mutex]
      // resource_unavailable_try_again - if any native handle type manipulated is not available.
      throw std::system_error(std::make_error_code(std::errc::resource_unavailable_try_again), "dispatch_semaphore_wait");
    }
  }

  bool try_acquire() noexcept
  {
    return (dsema_ != NULL && ::dispatch_semaphore_wait(dsema_, DISPATCH_TIME_NOW) == KERN_SUCCESS);
  }

  template<class Rep, class Period>
  bool try_acquire_for(const std::chrono::duration<Rep, Period>& rel_time)
  {
    using namespace std::chrono;
    validate_native_handle("counting_semaphore::try_acquire_for");
    int64_t delta = duration_cast<nanoseconds>(rel_time).count();
    auto timeout = ::dispatch_time(DISPATCH_TIME_NOW, delta);

    long result = ::dispatch_semaphore_wait(dsema_, timeout);
    if (result != KERN_SUCCESS && result != KERN_OPERATION_TIMED_OUT) {
      // [thread.mutex.requirements.mutex]
      // resource_unavailable_try_again - if any native handle type manipulated is not available.
      throw std::system_error(std::make_error_code(std::errc::resource_unavailable_try_again), "dispatch_semaphore_wait");
    }
    return (result == KERN_SUCCESS);
  }

  template<class Clock, class Duration>
  bool try_acquire_until(const std::chrono::time_point<Clock, Duration>& abs_time)
  {
    validate_native_handle("counting_semaphore::try_acquire_until");
    const struct ::timespec unix_epoch = { 0, 0 };
    auto timeout = ::dispatch_walltime(&unix_epoch, detail::from_unix_epoch(abs_time));

    long result = ::dispatch_semaphore_wait(dsema_, timeout);
    if (result != KERN_SUCCESS && result != KERN_OPERATION_TIMED_OUT) {
      // [thread.mutex.requirements.mutex]
      // resource_unavailable_try_again - if any native handle type manipulated is not available.
      throw std::system_error(std::make_error_code(std::errc::resource_unavailable_try_again), "dispatch_semaphore_wait");
    }
    return (result == KERN_SUCCESS);
  }
};

using binary_semaphore = counting_semaphore<1>;

} // namespace gcd
} // namespace yamc

#endif
