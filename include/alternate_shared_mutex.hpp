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
#include "yamc_rwlock_sched.hpp"


namespace yamc {

namespace alternate {

template <typename RwLockPolicy>
class basic_shared_mutex {
  typename RwLockPolicy::state state_;
  std::condition_variable cv_;
  std::mutex mtx_;

public:
  basic_shared_mutex() = default;
  ~basic_shared_mutex() = default;

  basic_shared_mutex(const basic_shared_mutex&) = delete;
  basic_shared_mutex& operator=(const basic_shared_mutex&) = delete;

  void lock()
  {
    std::unique_lock<decltype(mtx_)> lk(mtx_);
    RwLockPolicy::before_wait_wlock(state_);
    while (RwLockPolicy::wait_wlock(state_)) {
      cv_.wait(lk);
    }
    RwLockPolicy::after_wait_wlock(state_);
    RwLockPolicy::acquire_wlock(state_);
  }

  bool try_lock()
  {
    std::lock_guard<decltype(mtx_)> lk(mtx_);
    if (RwLockPolicy::wait_wlock(state_))
      return false;
    RwLockPolicy::acquire_wlock(state_);
    return true;
  }

  void unlock()
  {
    std::lock_guard<decltype(mtx_)> lk(mtx_);
    RwLockPolicy::release_wlock(state_);
    cv_.notify_all();
  }

  void lock_shared()
  {
    std::unique_lock<decltype(mtx_)> lk(mtx_);
    while (RwLockPolicy::wait_rlock(state_)) {
      cv_.wait(lk);
    }
    RwLockPolicy::acquire_rlock(state_);
  }

  bool try_lock_shared()
  {
    std::lock_guard<decltype(mtx_)> lk(mtx_);
    if (RwLockPolicy::wait_rlock(state_))
      return false;
    RwLockPolicy::acquire_rlock(state_);
    return true;
  }

  void unlock_shared()
  {
    std::lock_guard<decltype(mtx_)> lk(mtx_);
    if (RwLockPolicy::release_rlock(state_)) {
      cv_.notify_all();
    }
  }
};

using shared_mutex = basic_shared_mutex<YAMC_RWLOCK_SCHED_DEFAULT>;

} // namespace alternate
} // namespace yamc

#endif
