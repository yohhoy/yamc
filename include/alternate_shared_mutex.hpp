/*
 * alternate_shared_mutex.hpp
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
#ifndef YAMC_ALTERNATE_SHARED_MUTEX_HPP_
#define YAMC_ALTERNATE_SHARED_MUTEX_HPP_


#include <cassert>
#include <condition_variable>
#include <mutex>


namespace yamc {

namespace alternate {

/*
 * alternate implementation of shared_mutex
 */
class shared_mutex {
  bool writer_ = false;
  std::size_t nreader_ = 0;
  std::condition_variable cv_;
  std::mutex mtx_;

public:
  shared_mutex() = default;
  ~shared_mutex() = default;

  shared_mutex(const shared_mutex&) = delete;
  shared_mutex& operator=(const shared_mutex&) = delete;

  void lock()
  {
    std::unique_lock<decltype(mtx_)> lk(mtx_);
    while (writer_ || 0 < nreader_) {
      cv_.wait(lk);
    }
    writer_ = true;
  }

  bool try_lock()
  {
    std::lock_guard<decltype(mtx_)> lk(mtx_);
    if (writer_ || 0 < nreader_)
      return false;
    writer_ = true;
    return true;
  }

  void unlock()
  {
    std::lock_guard<decltype(mtx_)> lk(mtx_);
    writer_ = false;
    cv_.notify_all();
  }

  void lock_shared()
  {
    std::unique_lock<decltype(mtx_)> lk(mtx_);
    while (writer_) {
      cv_.wait(lk);
    }
    ++nreader_;
  }

  bool try_lock_shared()
  {
    std::lock_guard<decltype(mtx_)> lk(mtx_);
    if (writer_)
      return false;
    ++nreader_;
    return true;
  }

  void unlock_shared()
  {
    std::lock_guard<decltype(mtx_)> lk(mtx_);
    assert(0 < nreader_);
    --nreader_;
    cv_.notify_all();
  }
};

} // namespace alternate
} // namespace yamc

#endif
