/*
 * yamc_semaphore.hpp
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
#ifndef YAMC_SEMAPHORE_HPP_
#define YAMC_SEMAPHORE_HPP_

#include <cassert>
#include <cstddef>
#include <chrono>
#include <condition_variable>
#include <limits>
#include <mutex>


/// default least_max_value of yamc::counting_semaphore
#ifndef YAMC_SEMAPHORE_LEAST_MAX_VALUE
#define YAMC_SEMAPHORE_LEAST_MAX_VALUE  ((std::numeric_limits<std::ptrdiff_t>::max)())
#endif


/*
 * Semaphores in C++20 Standard Library
 *
 * - yamc::counting_semaphore<least_max_value>
 * - yamc::binary_semaphore
 */
namespace yamc {

template <std::ptrdiff_t least_max_value = YAMC_SEMAPHORE_LEAST_MAX_VALUE>
class counting_semaphore {
  std::ptrdiff_t counter_;
  std::condition_variable cv_;
  std::mutex mtx_;

  template<typename Clock, typename Duration>
  bool do_try_acquirewait(const std::chrono::time_point<Clock, Duration>& tp)
  {
    std::unique_lock<decltype(mtx_)> lk(mtx_);
    while (counter_ <= 0) {
      if (cv_.wait_until(lk, tp) == std::cv_status::timeout) {
        if (0 < counter_)  // re-check predicate
          break;
        return false;
      }
    }
    --counter_;
    return true;
  }

public:
  static constexpr std::ptrdiff_t (max)() noexcept
  {
    static_assert(0 <= least_max_value, "least_max_value shall be non-negative");
    return least_max_value;
  }

  /*constexpr*/ explicit counting_semaphore(std::ptrdiff_t desired)
    : counter_(desired)
  {
    assert(0 <= desired && desired <= (max)());
    // counting_semaphore constructor throws nothing.
  }

  ~counting_semaphore() = default;

  counting_semaphore(const counting_semaphore&) = delete;
  counting_semaphore& operator=(const counting_semaphore&) = delete;

  void release(std::ptrdiff_t update = 1)
  {
    std::lock_guard<decltype(mtx_)> lk(mtx_);
    assert(0 <= update && update <= (max)() - counter_);
    counter_ += update;
    if (0 < counter_) {
      cv_.notify_all();
    }
  }

  void acquire()
  {
    std::unique_lock<decltype(mtx_)> lk(mtx_);
    while (counter_ <= 0) {
      cv_.wait(lk);
    }
    --counter_;
  }

  bool try_acquire() noexcept
  {
    std::unique_lock<decltype(mtx_)> lk(mtx_);
    if (counter_ <= 0) {
      // no spurious failure
      return false;
    }
    --counter_;
    return true;
  }

  template<class Rep, class Period>
  bool try_acquire_for(const std::chrono::duration<Rep, Period>& rel_time)
  {
    const auto tp = std::chrono::steady_clock::now() + rel_time;
    return do_try_acquirewait(tp);
  }

  template<class Clock, class Duration>
  bool try_acquire_until(const std::chrono::time_point<Clock, Duration>& abs_time)
  {
    return do_try_acquirewait(abs_time);
  }
};

using binary_semaphore = counting_semaphore<1>;


} // namespace yamc

#endif
