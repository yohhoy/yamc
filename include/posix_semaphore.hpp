/*
 * posix_semaphore.hpp
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
#ifndef POSIX_SEMAPHORE_HPP_
#define POSIX_SEMAPHORE_HPP_

#include <cassert>
#include <cstddef>
#include <chrono>
#include <system_error>
#include <type_traits>
// POSIX semaphore
#include <limits.h>  // SEM_VALUE_MAX
#include <semaphore.h>
#include <time.h>    // timespec struct


/*
 * Semaphores in C++20 Standard Library for POSIX-compatible platform
 *
 * - yamc::posix::counting_semaphore<least_max_value>
 * - yamc::posix::binary_semaphore
 *
 * This implementation use POSIX unnamed semaphore.
 * http://pubs.opengroup.org/onlinepubs/9699919799/basedefs/semaphore.h.html
 */
namespace yamc {

namespace posix {

template <std::ptrdiff_t least_max_value = SEM_VALUE_MAX>
class counting_semaphore {
  ::sem_t sem_;

  static void throw_errno(const char* what_arg)
  {
    throw std::system_error(std::error_code(errno, std::generic_category()), what_arg);
  }

  bool do_try_acquirewait(const std::chrono::system_clock::time_point& tp)
  {
    using namespace std::chrono;
    // convert C++ system_clock to POSIX struct timespec
    struct ::timespec abs_timeout;
    abs_timeout.tv_sec = system_clock::to_time_t(tp);
    abs_timeout.tv_nsec = (long)(duration_cast<nanoseconds>(tp.time_since_epoch()).count() % 1000000000);

    errno = 0;
    if (::sem_timedwait(&sem_, &abs_timeout) == 0) {
      return true;
    } else if (errno != ETIMEDOUT) {
      throw_errno("sem_timedwait");
    }
    return false;
  }

public:
  static constexpr std::ptrdiff_t max() noexcept
  {
    static_assert(0 < least_max_value, "least_max_value shall be greater than zero");
    static_assert(least_max_value <= SEM_VALUE_MAX, "least_max_value shall be less than or equal to SEM_VALUE_MAX");
    return least_max_value;
  }

  /*constexpr*/ explicit counting_semaphore(std::ptrdiff_t desired)
  {
    assert(0 <= desired && desired <= max());
    ::sem_init(&sem_, 0, (unsigned int)desired);
    // counting_semaphore constructor throws nothing.
  }

  ~counting_semaphore()
  {
    ::sem_destroy(&sem_);
  }

  counting_semaphore(const counting_semaphore&) = delete;
  counting_semaphore& operator=(const counting_semaphore&) = delete;

  void release(std::ptrdiff_t update = 1)
  {
    errno = 0;
    while (0 < update--) {
      if (::sem_post(&sem_) != 0) {
        throw_errno("sem_post");
      }
    }
  }

  void acquire()
  {
    errno = 0;
    if (::sem_wait(&sem_) != 0) {
      throw_errno("sem_wait");
    }
  }

  bool try_acquire() noexcept
  {
    return (::sem_trywait(&sem_) == 0);
  }

  template<class Rep, class Period>
  bool try_acquire_for(const std::chrono::duration<Rep, Period>& rel_time)
  {
    // C++ Standard says '_for'-suffixed timeout function shall use steady clock,
    // but we use std::chrono::system_clock which may or may not be steady.
    const auto tp = std::chrono::system_clock::now() + rel_time;
    return do_try_acquirewait(tp);
  }

  template<class Clock, class Duration>
  bool try_acquire_until(const std::chrono::time_point<Clock, Duration>& abs_time)
  {
    static_assert(std::is_same<Clock, std::chrono::system_clock>::value, "support only system_clock");
    return do_try_acquirewait(abs_time);
  }
};

using binary_semaphore = counting_semaphore<1>;

} // namespace posix
} // namespace yamc

#endif
