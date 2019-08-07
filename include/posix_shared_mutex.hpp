/*
 * posix_shared_mutex.hpp
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
#ifndef POSIX_SHARED_MUTEX_HPP_
#define POSIX_SHARED_MUTEX_HPP_

// POSIX(pthreads) mutex
#include <pthread.h>


/*
 * Pthreads rwlock wrapper on POSIX-compatible platform
 *
 * - yamc::posix::shared_mutex
 *
 * http://pubs.opengroup.org/onlinepubs/9699919799/basedefs/pthread.h.html
 */
namespace yamc {

namespace posix {


class shared_mutex {
#if defined(PTHREAD_RWLOCK_INITIALIZER)
  ::pthread_rwlock_t rwlock_ = PTHREAD_RWLOCK_INITIALIZER;
#elif defined(PTHREAD_RWLOCK_INITIALIZER_NP)
  ::pthread_rwlock_t rwlock_ = PTHREAD_RWLOCK_INITIALIZER_NP;
#endif

public:
  shared_mutex() = default;

  ~shared_mutex()
  {
    ::pthread_rwlock_destroy(&rwlock_);
  }

  shared_mutex(const shared_mutex&) = delete;
  shared_mutex& operator=(const shared_mutex&) = delete;

  void lock()
  {
    ::pthread_rwlock_wrlock(&rwlock_);
  }

  bool try_lock()
  {
    return (::pthread_rwlock_trywrlock(&rwlock_) == 0);
  }

  void unlock()
  {
    ::pthread_rwlock_unlock(&rwlock_);
  }

  void lock_shared()
  {
    ::pthread_rwlock_rdlock(&rwlock_);
  }

  bool try_lock_shared()
  {
    return (::pthread_rwlock_tryrdlock(&rwlock_) == 0);
  }

  void unlock_shared()
  {
    ::pthread_rwlock_unlock(&rwlock_);
  }
};

} // namespace posix
} // namespace yamc

#endif
