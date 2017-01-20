/*
 * alternate_mutex.hpp
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
#ifndef YAMC_ALTERNATE_MUTEX_HPP_
#define YAMC_ALTERNATE_MUTEX_HPP_

#include <cassert>
#include <atomic>
#include <mutex>
#include <thread>


namespace yamc {
namespace alternate {

/*
 * alternate recursive_mutex implementation
 */
class recursive_mutex {
  std::size_t ncount_ = 0;
  std::atomic<std::thread::id> owner_ = {};
  std::mutex mtx_;

public:
  recursive_mutex() = default;
  ~recursive_mutex() = default;

  recursive_mutex(const recursive_mutex&) = delete;
  recursive_mutex& operator=(const recursive_mutex&) = delete;

  void lock()
  {
    const auto tid = std::this_thread::get_id();
    if (owner_.load(std::memory_order_relaxed) == tid) {
      ++ncount_;
    } else {
      mtx_.lock();
      owner_.store(tid, std::memory_order_relaxed);
      ncount_ = 1;
    }
  }

  bool try_lock()
  {
    const auto tid = std::this_thread::get_id();
    if (owner_.load(std::memory_order_relaxed) == tid) {
      ++ncount_;
    } else {
      if (!mtx_.try_lock())
        return false;
      owner_.store(tid, std::memory_order_relaxed);
      ncount_ = 1;
    }
    return true;
  }

  void unlock()
  {
    assert(0 < ncount_ && owner_ == std::this_thread::get_id());
    if (--ncount_ == 0) {
      owner_.store(std::thread::id(), std::memory_order_relaxed);
      mtx_.unlock();
    }
  }
};

} // namespace alternate
} // namespace yamc

#endif
