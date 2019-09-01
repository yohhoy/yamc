/*
 * apple_native_mutex.hpp
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
#ifndef APPLE_NATIVE_MUTEX_HPP_
#define APPLE_NATIVE_MUTEX_HPP_

// macOS/iOS mutex
#include <os/lock.h>


namespace yamc {

/*
 * Native mutex wrapper on macOS/iOS families
 *
 * - yamc::apple::unfair_lock
 *
 * https://developer.apple.com/documentation/os/1646466-os_unfair_lock_lock
 */
namespace apple {

class unfair_lock {
  ::os_unfair_lock oslock_ = OS_UNFAIR_LOCK_INIT;

public:
  constexpr unfair_lock() noexcept = default;
  ~unfair_lock() = default;

  unfair_lock(const unfair_lock&) = delete;
  unfair_lock& operator=(const unfair_lock&) = delete;

  void lock()
  {
    ::os_unfair_lock_lock(&oslock_);
  }

  bool try_lock()
  {
    return ::os_unfair_lock_trylock(&oslock_);
  }

  void unlock()
  {
    ::os_unfair_lock_unlock(&oslock_);
  }

  using native_handle_type = ::os_unfair_lock_t;
  native_handle_type native_handle()
  {
    return &oslock_;
  }
};

} // namespace apple
} // namespace yamc

#endif
