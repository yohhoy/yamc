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


/*
 * Semaphores in C++20 Standard Library for macOS/iOS families
 *
 * - yamc::gcd::counting_semaphore<least_max_value>
 * - yamc::gcd::binary_semaphore
 *
 * This implementation use dispatch semaphore of GCD runtime.
 * https://developer.apple.com/documentation/dispatch/dispatchsemaphore
 */
namespace yamc {

namespace gcd {

template <std::ptrdiff_t least_max_value = std::numeric_limits<long>::max()>
class counting_semaphore {
  ::dispatch_semaphore_t dsema_;

public:
  static constexpr std::ptrdiff_t max() noexcept
  {
    static_assert(0 < least_max_value, "least_max_value shall be greater than zero");
    // We assume least_max_value equals to std::numeric_limits<long>::max() here.
    // The official document says nothing about the upper limit of initial value.
    return least_max_value;
  }

  /*constexpr*/ explicit counting_semaphore(std::ptrdiff_t desired)
  {
    assert(0 <= desired && desired <= max());
    dsema_ = ::dispatch_semaphore_create((long)desired);
    assert(dsema_ != NULL);
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
    while (0 < update--) {
      ::dispatch_semaphore_signal(dsema_);
    }
  }

  void acquire()
  {
    ::dispatch_semaphore_wait(dsema_, DISPATCH_TIME_FOREVER);
  }

  bool try_acquire() noexcept
  {
    long result = ::dispatch_semaphore_wait(dsema_, DISPATCH_TIME_NOW);
    // dispatch_semaphore() will return KERN_SUCCESS or KERN_OPERATION_TIMED_OUT
    return (result == KERN_SUCCESS);
  }

  template<class Rep, class Period>
  bool try_acquire_for(const std::chrono::duration<Rep, Period>& rel_time)
  {
    int64_t delta = std::chrono::duration_cast<std::chrono::nanoseconds>(rel_time).count();
    auto timeout = ::dispatch_time(DISPATCH_TIME_NOW, delta);
    long result = ::dispatch_semaphore_wait(dsema_, timeout);
    return (result == KERN_SUCCESS);
  }

  template<class Clock, class Duration>
  bool try_acquire_until(const std::chrono::time_point<Clock, Duration>& abs_time)
  {
    static_assert(std::is_same<Clock, std::chrono::system_clock>::value, "support only system_clock");
    // Until C++20, the epoch of std::chrono::system_clock is unspecified,
    // but most implementation use UNIX epooch (19700101T000000Z).
    const struct ::timespec unix_epoch = { 0, 0 };
    auto from_epoch = std::chrono::duration_cast<std::chrono::nanoseconds>(abs_time.time_since_epoch()).count();
    auto timeout = ::dispatch_walltime(&unix_epoch, from_epoch);
    long result = ::dispatch_semaphore_wait(dsema_, timeout);
    return (result == KERN_SUCCESS);
  }
};

using binary_semaphore = counting_semaphore<1>;

} // namespace gcd
} // namespace yamc

#endif
