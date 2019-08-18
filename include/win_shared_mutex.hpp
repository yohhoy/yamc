/*
 * win_shared_mutex.hpp
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
#ifndef WIN_SHARED_MUTEX_HPP_
#define WIN_SHARED_MUTEX_HPP_

// Windows mutex
#include <windows.h>


namespace yamc {

/*
 * Windows Slim Reader/Writer(SRW) lock wrapper on Windows platform
 *
 * - yamc::win::shared_mutex
 *
 * SRW lock does not support lock acquisition with timeout.
 * https://docs.microsoft.com/windows/win32/sync/slim-reader-writer--srw--locks
 */
namespace win {

class shared_mutex {
  ::SRWLOCK srwlock_ = SRWLOCK_INIT;

public:
  shared_mutex() = default;
  ~shared_mutex() = default;

  shared_mutex(const shared_mutex&) = delete;
  shared_mutex& operator=(const shared_mutex&) = delete;

  void lock()
  {
    ::AcquireSRWLockExclusive(&srwlock_);
  }

  bool try_lock()
  {
    return ::TryAcquireSRWLockExclusive(&srwlock_);
  }

  void unlock()
  {
    ::ReleaseSRWLockExclusive(&srwlock_);
  }

  void lock_shared()
  {
    ::AcquireSRWLockShared(&srwlock_);
  }

  bool try_lock_shared()
  {
    return ::TryAcquireSRWLockShared(&srwlock_);
  }

  void unlock_shared()
  {
    ::ReleaseSRWLockShared(&srwlock_);
  }

  using native_handle_type = ::SRWLOCK*;
  native_handle_type native_handle()
  {
    return &srwlock_;
  }
};

} // namespace win
} // namespace yamc

#endif
