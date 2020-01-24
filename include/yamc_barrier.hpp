/*
 * yamc_barrier.hpp
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
#ifndef YAMC_BARRIER_HPP_
#define YAMC_BARRIER_HPP_

#include <cassert>
#include <cstddef>
#include <condition_variable>
#include <limits>
#include <mutex>


/*
 * Barriers in C++20 Standard Library
 *
 * - yamc::barrier<CompletionFunction>
 */
namespace yamc {

namespace detail {

/// default CompletionFunction of yamc::barrier class template
class default_barrier_completion {
public:
  void operator()() const {
    // no effect
  }
};

struct barrier_arrival_token {
  unsigned phase_;
};

} // namespace detail


template <class CompletionFunction = detail::default_barrier_completion>
class barrier {
  std::ptrdiff_t init_count_;
  std::ptrdiff_t counter_;
  unsigned phase_ = 0;
  CompletionFunction completion_;
  mutable std::condition_variable cv_;
  mutable std::mutex mtx_;

  void phase_completion_step(bool drop)
  {
    completion_();
    if (drop) {
      --init_count_;
    }
    counter_ = init_count_;
    ++phase_;
    cv_.notify_all();
  }

public:
  using arrival_token = detail::barrier_arrival_token;

  static constexpr ptrdiff_t (max)() noexcept
  {
    return (std::numeric_limits<ptrdiff_t>::max)();
  }

  /*constexpr*/ explicit barrier(std::ptrdiff_t expected, CompletionFunction f = CompletionFunction())
    : init_count_(expected)
    , counter_(expected)
    , completion_(std::move(f))
  {
    assert(0 <= expected && expected < (max()));
  }

  ~barrier() = default;

  barrier(const barrier&) = delete;
  barrier& operator=(const barrier&) = delete;

#if 201703L <= __cplusplus
  [[nodiscard]]
#endif
  arrival_token arrive(std::ptrdiff_t update = 1)
  {
    std::lock_guard<decltype(mtx_)> lk(mtx_);
    assert(0 < update && update <= counter_);
    arrival_token token{phase_};
    counter_ -= update;
    if (counter_ == 0) {
      phase_completion_step(false);
    }
    return token;
  }

  void wait(arrival_token&& arrival) const
  {
    std::unique_lock<decltype(mtx_)> lk(mtx_);
    while (phase_ <= arrival.phase_) {
      cv_.wait(lk);
    }
  }

  void arrive_and_wait()
  {
    // equivalent to wait(arrive())
    std::unique_lock<decltype(mtx_)> lk(mtx_);
    assert(0 < counter_);
    auto arrival_phase = phase_;
    counter_ -= 1;
    if (counter_ == 0) {
      phase_completion_step(false);
    }
    while (phase_ <= arrival_phase) {
      cv_.wait(lk);
    }
  }

  void arrive_and_drop()
  {
    std::unique_lock<decltype(mtx_)> lk(mtx_);
    assert(0 < counter_);
    auto arrival_phase = phase_;
    counter_ -= 1;
    if (counter_ == 0) {
      phase_completion_step(true);
    }
    while (phase_ <= arrival_phase) {
      cv_.wait(lk);
    }
  }
};

} // namespace yamc

#endif
