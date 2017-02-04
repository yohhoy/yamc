/*
 * yamc_testutil.hpp
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
#ifndef YAMC_TESTUTIL_HPP_
#define YAMC_TESTUTIL_HPP_

#include <cassert>
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>


// platform
#if defined(_WIN32)
#define TEST_PLATFORM_LINUX 0
#define TEST_PLATFORM_OSX 0
#define TEST_PLATFORM_WINDOWS 1
#elif defined(__APPLE__)
#define TEST_PLATFORM_LINUX 0
#define TEST_PLATFORM_OSX 1
#define TEST_PLATFORM_WINDOWS 0
#elif defined(__linux)
#define TEST_PLATFORM_LINUX 1
#define TEST_PLATFORM_OSX 0
#define TEST_PLATFORM_WINDOWS 0
#endif


// C++ compiler
#if defined(__clang__)
#define TEST_COMPILER_CLANG 1
#define TEST_COMPILER_GCC 0
#define TEST_COMPILER_MSVC 0
#elif defined(__GNUC__)
#define TEST_COMPILER_CLANG 0
#define TEST_COMPILER_GCC 1
#define TEST_COMPILER_MSVC
#elif defined(_MSC_VER)
#define TEST_COMPILER_CLANG 0
#define TEST_COMPILER_GCC 0
#define TEST_COMPILER_MSVC 1
#endif


namespace yamc {
namespace test {


/// auto join thread
class join_thread {
  std::thread thd_;

public:
  template<typename F>
  explicit join_thread(F&& f)
    : thd_(std::forward<F>(f)) {}

  ~join_thread() noexcept(false)
  {
    thd_.join();
  }

  join_thread(const join_thread&) = delete;
  join_thread& operator=(const join_thread&) = delete;
  join_thread(join_thread&&) = default;
  join_thread& operator=(join_thread&&) = default;
};


/// randzvous point primitive
class barrier {
  std::size_t nthread_;
  std::size_t count_;
  std::size_t step_ = 0;
  std::condition_variable cv_;
  std::mutex mtx_;

public:
  explicit barrier(std::size_t n)
    : nthread_(n), count_(n) {}

  barrier(const barrier&) = delete;
  barrier& operator=(const barrier&) = delete;

  bool await()
  {
    std::unique_lock<std::mutex> lk(mtx_);
    std::size_t step = step_;
    if (--count_ == 0) {
      count_ = nthread_;
      ++step_;
      cv_.notify_all();
      return true;
    }
    while (step == step_) {
      cv_.wait(lk);
    }
    return false;
  }
};


/// phase control primitive
class phaser {
  std::size_t sentinel_ = 0;
  std::vector<std::size_t> phase_;
  std::condition_variable cv_;
  std::mutex mtx_;

  void do_advance(std::size_t id, std::size_t n)
  {
    std::lock_guard<std::mutex> lk(mtx_);
    phase_[id] += n;
    sentinel_ = *std::min_element(phase_.begin(), phase_.end());
    cv_.notify_all();
  }

  void do_await(std::size_t id)
  {
    std::unique_lock<std::mutex> lk(mtx_);
    phase_[id] += 1;
    sentinel_ = *std::min_element(phase_.begin(), phase_.end());
    while (sentinel_ != phase_[id]) {
      cv_.wait(lk);
    }
    cv_.notify_all();
  }

public:
  explicit phaser(std::size_t n)
    : phase_(n, 0u) {}

  class proxy {
    phaser* phaser_;
    std::size_t id_;

    friend class phaser;
    proxy(phaser* p, std::size_t id)
      : phaser_(p), id_(id) {}

  public:
    void advance(std::size_t n)
      { phaser_->do_advance(id_, n); }
    void await()
      { phaser_->do_await(id_); }
  };

  proxy get(std::size_t id)
  {
    assert(id < phase_.size());
    return {this, id};
  }
};


/// parallel task runner
template<typename F>
void task_runner(std::size_t nthread, F f)
{
  barrier gate(1 + nthread);
  std::vector<std::thread> thds;
  for (std::size_t n = 0; n < nthread; ++n) {
    thds.emplace_back([f,n,&gate]{
      gate.await();
      f(n);
    });
  }
  gate.await();  // start
  for (auto& t: thds) {
    t.join();
  }
}


/// stopwatch
template <typename Duration = std::chrono::microseconds>
class stopwatch {
  using ClockType = std::chrono::system_clock;
  ClockType::time_point start_;

public:
  stopwatch()
    : start_(ClockType::now()) {}

  stopwatch(const stopwatch&) = delete;
  stopwatch& operator=(const stopwatch&) = delete;

  Duration elapsed()
  {
    auto end = ClockType::now();
    return std::chrono::duration_cast<Duration>(end - start_);
  }
};

} // namespace test


namespace cxx {

/// C++14 std::make_unique()
template<typename T, typename... Args>
inline
std::unique_ptr<T> make_unique(Args&&... args)
{
  return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

} // namespace cxx


// mock classes of mutex types
namespace mock {

struct mutex {
  bool retval_on_trylock = true;

  mutex() = default;
  ~mutex() = default;

  mutex(const mutex&) = delete;
  mutex& operator=(const mutex&) = delete;

  void lock() {}
  bool try_lock() { return retval_on_trylock; }
  void unlock() {}
};

struct recursive_mutex {
  bool retval_on_trylock = true;

  recursive_mutex() = default;
  ~recursive_mutex() = default;

  recursive_mutex(const recursive_mutex&) = delete;
  recursive_mutex& operator=(const recursive_mutex&) = delete;

  void lock() {}
  bool try_lock() { return retval_on_trylock; }
  void unlock() {}
};

struct timed_mutex {
  bool retval_on_trylock = true;

  timed_mutex() = default;
  ~timed_mutex() = default;

  timed_mutex(const timed_mutex&) = delete;
  timed_mutex& operator=(const timed_mutex&) = delete;

  void lock() {}
  bool try_lock() { return retval_on_trylock; }
  void unlock() {}

  template<typename Rep, typename Period>
  bool try_lock_for(const std::chrono::duration<Rep, Period>&)
    { return retval_on_trylock; }

  template<typename Clock, typename Duration>
  bool try_lock_until(const std::chrono::time_point<Clock, Duration>&)
    { return retval_on_trylock; }
};

struct recursive_timed_mutex {
  bool retval_on_trylock = true;

  recursive_timed_mutex() = default;
  ~recursive_timed_mutex() = default;

  recursive_timed_mutex(const recursive_timed_mutex&) = delete;
  recursive_timed_mutex& operator=(const recursive_timed_mutex&) = delete;

  void lock() {}
  bool try_lock() { return retval_on_trylock; }
  void unlock() {}

  template<typename Rep, typename Period>
  bool try_lock_for(const std::chrono::duration<Rep, Period>&)
    { return retval_on_trylock; }

  template<typename Clock, typename Duration>
  bool try_lock_until(const std::chrono::time_point<Clock, Duration>&)
    { return retval_on_trylock; }
};

struct shared_mutex {
  bool retval_on_trylock = true;

  shared_mutex() = default;
  ~shared_mutex() = default;

  shared_mutex(const shared_mutex&) = delete;
  shared_mutex& operator=(const shared_mutex&) = delete;

  void lock() {}
  bool try_lock() { return retval_on_trylock; }
  void unlock() {}

  void lock_shared() {}
  bool try_lock_shared() { return retval_on_trylock; }
  void unlock_shared() {}
};

struct shared_timed_mutex {
  bool retval_on_trylock = true;

  shared_timed_mutex() = default;
  ~shared_timed_mutex() = default;

  shared_timed_mutex(const shared_timed_mutex&) = delete;
  shared_timed_mutex& operator=(const shared_timed_mutex&) = delete;

  void lock() {}
  bool try_lock() { return retval_on_trylock; }
  void unlock() {}

  template<typename Rep, typename Period>
  bool try_lock_for(const std::chrono::duration<Rep, Period>&)
    { return retval_on_trylock; }

  template<typename Clock, typename Duration>
  bool try_lock_until(const std::chrono::time_point<Clock, Duration>&)
    { return retval_on_trylock; }

  void lock_shared() {}
  bool try_lock_shared() { return retval_on_trylock; }
  void unlock_shared() {}

  template<typename Rep, typename Period>
  bool try_lock_shared_for(const std::chrono::duration<Rep, Period>&)
    { return retval_on_trylock; }

  template<typename Clock, typename Duration>
  bool try_lock_shared_until(const std::chrono::time_point<Clock, Duration>&)
    { return retval_on_trylock; }
};

} // namespace mock

} // namespace yamc

#endif
