/*
 * ttas_spin_mutex.hpp
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
#ifndef YAMC_TTAS_SPIN_MUTEX_HPP_
#define YAMC_TTAS_SPIN_MUTEX_HPP_

#include <atomic>
#include <thread>


namespace yamc {

/*
 * Test-and-Test-And-Swap(TTAS) spinlock implementation
 */
namespace spin_ttas {

class mutex {
  std::atomic<int> state_{0};

public:
  mutex() = default;
  ~mutex() = default;

  mutex(const mutex&) = delete;
  mutex& operator=(const mutex&) = delete;

  void lock() {
    int state;
    do {
      while (state_.load(std::memory_order_relaxed) != 0) {
        std::this_thread::yield();
      }
      state = 0;
    } while (!state_.compare_exchange_weak(state, 1, std::memory_order_acquire));
  }

  bool try_lock() {
    int state = 0;
    return state_.compare_exchange_weak(state, 1, std::memory_order_acquire);
  }

  void unlock() {
    state_.store(0, std::memory_order_release);
  }
};

} // namespace spin_ttas
} // namespace yamc

#endif
