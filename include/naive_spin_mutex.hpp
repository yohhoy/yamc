/*
 * naive_spin_mutex.hpp
 *
 * MIT License
 *
 * Copyright (c) 2017 yohhoy
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
#ifndef YAMC_NAIVE_SPIN_MUTEX_HPP_
#define YAMC_NAIVE_SPIN_MUTEX_HPP_

#include <atomic>
#include "yamc_backoff_spin.hpp"


namespace yamc {

/*
 * naive Test-And-Swap(TAS) spinlock implementation (with memory_order_seq_cst)
 *
 * - yamc::spin::mutex
 * - yamc::spin::basic_mutex<BackoffPolicy>
 */
namespace spin {

template <typename BackoffPolicy>
class basic_mutex {
  std::atomic<int> state_{0};

public:
  basic_mutex() = default;
  ~basic_mutex() = default;

  basic_mutex(const basic_mutex&) = delete;
  basic_mutex& operator=(const basic_mutex&) = delete;

  void lock()
  {
    typename BackoffPolicy::state state;
    int expected = 0;
    while (!state_.compare_exchange_weak(expected, 1)) {
      BackoffPolicy::wait(state);
      expected = 0;
    }
  }

  bool try_lock()
  {
    int expected = 0;
    return state_.compare_exchange_weak(expected, 1);
  }

  void unlock()
  {
    state_.store(0);
  }
};

using mutex = basic_mutex<YAMC_BACKOFF_SPIN_DEFAULT>;

} // namespace spin


/*
 * naive Test-And-Swap(TAS) spinlock implementation for weak hardware memory model
 *
 * - yamc::spin_weak::mutex
 * - yamc::spin_weak::basic_mutex<BackoffPolicy>
 */
namespace spin_weak {

template <typename BackoffPolicy>
class basic_mutex {
  std::atomic<int> state_{0};

public:
  basic_mutex() = default;
  ~basic_mutex() = default;

  basic_mutex(const basic_mutex&) = delete;
  basic_mutex& operator=(const basic_mutex&) = delete;

  void lock()
  {
    typename BackoffPolicy::state state;
    int expected = 0;
    while (!state_.compare_exchange_weak(expected, 1, std::memory_order_acquire, std::memory_order_relaxed)) {
      BackoffPolicy::wait(state);
      expected = 0;
    }
  }

  bool try_lock()
  {
    int expected = 0;
    return state_.compare_exchange_weak(expected, 1, std::memory_order_acquire, std::memory_order_relaxed);
  }

  void unlock()
  {
    state_.store(0, std::memory_order_release);
  }
};

using mutex = basic_mutex<YAMC_BACKOFF_SPIN_DEFAULT>;

} // namespace spin_weak

} // namespace yamc

#endif
