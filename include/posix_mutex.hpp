/*
 * posix_mutex.hpp
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
#ifndef POSIX_MUTEX_HPP_
#define POSIX_MUTEX_HPP_

// POSIX(pthreads) mutex
#include <pthread.h>


/*
 * Pthreads mutex wrapper on POSIX-compatible platform
 *
 * - yamc::posix::mutex
 * - yamc::posix::recursive_mutex
 *
 * http://pubs.opengroup.org/onlinepubs/9699919799/basedefs/pthread.h.html
 */
namespace yamc {

namespace posix {


class mutex {
#if defined(PTHREAD_MUTEX_INITIALIZER)
  ::pthread_mutex_t mtx_ = PTHREAD_MUTEX_INITIALIZER;
#elif defined(PTHREAD_MUTEX_INITIALIZER_NP)
  ::pthread_mutex_t mtx_ = PTHREAD_MUTEX_INITIALIZER_NP;
#endif

public:
  constexpr mutex() = default;

  ~mutex()
  {
    ::pthread_mutex_destroy(&mtx_);
  }

  mutex(const mutex&) = delete;
  mutex& operator=(const mutex&) = delete;

  void lock()
  {
    ::pthread_mutex_lock(&mtx_);
  }

  bool try_lock()
  {
    return (::pthread_mutex_trylock(&mtx_) == 0);
  }

  void unlock()
  {
    ::pthread_mutex_unlock(&mtx_);
  }
};


class recursive_mutex {
#if defined(PTHREAD_RECURSIVE_MUTEX_INITIALIZER)
  ::pthread_mutex_t mtx_ = PTHREAD_RECURSIVE_MUTEX_INITIALIZER;
#elif defined(PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP)
  ::pthread_mutex_t mtx_ = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
#endif

public:
  recursive_mutex() = default;

  ~recursive_mutex()
  {
    ::pthread_mutex_destroy(&mtx_);
  }

  recursive_mutex(const recursive_mutex&) = delete;
  recursive_mutex& operator=(const recursive_mutex&) = delete;

  void lock()
  {
    ::pthread_mutex_lock(&mtx_);
  }

  bool try_lock()
  {
    return (::pthread_mutex_trylock(&mtx_) == 0);
  }

  void unlock()
  {
    ::pthread_mutex_unlock(&mtx_);
  }
};


} // namespace posix
} // namespace yamc

#endif
