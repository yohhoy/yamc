/*
 * fair_shared_mutex.hpp
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
#ifndef YAMC_FAIR_SHARED_MUTEX_HPP_
#define YAMC_FAIR_SHARED_MUTEX_HPP_

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>


namespace yamc {

/*
 * phase-fairness (FIFO locking) shared mutex
 *
 * - yamc::fair::shared_mutex
 * - yamc::fair::shared_timed_mutex
 */
namespace fair {

namespace detail {

class shared_mutex_base {
protected:
  const static std::size_t node_status_mask = 3;
  const static std::size_t node_nthread_inc = (1U << 2);

  struct node {
    std::size_t status;
    // bitmask:
    //   (LSB)0: 0=exclusive-lock / 1=shared-lock
    //        1: 0=waiting / 1=lockable(-ing)
    //   MSB..2: number of locking thread (locked_ member only)
    node* next;
    node* prev;
  };

  node queue_;   // q.next = front(), q.prev = back()
  node locked_;  // placeholder node of 'locked' state
  std::condition_variable cv_;
  std::mutex mtx_;

private:
  bool wq_empty()
  {
    return queue_.next == &queue_;
  }

  void wq_push_front(node* p)
  {
    node* next = queue_.next;
    next->prev = queue_.next = p;
    p->next = next;
    p->prev = &queue_;
  }

  void wq_push_back(node* p)
  {
    node* back = queue_.prev;
    back->next = queue_.prev = p;
    p->next = &queue_;
    p->prev = back;
  }

  void wq_erase(node* p)
  {
    p->next->prev = p->prev;
    p->prev->next = p->next;
  }

  bool wq_shared_lockable()
  {
    return wq_empty() || (queue_.prev->status & node_status_mask) == 3;
  }

  void wq_push_locknode(unsigned mode)
  {
    if (queue_.next != &locked_) {
      locked_.status = mode | 2;
      wq_push_front(&locked_);
    }
    assert((locked_.status & node_status_mask) == (mode | 2));
    locked_.status += node_nthread_inc;
  }

  void wq_pop_locknode()
  {
    assert(queue_.next == &locked_);
    wq_erase(&locked_);
    locked_.status = 0;
    locked_.next = locked_.prev = nullptr;
  }

protected:
  shared_mutex_base()
    : queue_{0, &queue_, &queue_} {}
  ~shared_mutex_base() = default;

  void impl_lock(std::unique_lock<std::mutex>& lk)
  {
    if (!wq_empty()) {
      node request = {0, 0, 0};  // exclusive-lock
      wq_push_back(&request);
      while (queue_.next != &request) {
        cv_.wait(lk);
      }
      wq_erase(&request);
    }
    wq_push_locknode(0);
  }

  bool impl_try_lock()
  {
    if (!wq_empty()) {
      return false;
    }
    wq_push_locknode(0);
    return true;
  }

  void impl_unlock()
  {
    assert(queue_.next == &locked_ && (locked_.status & node_status_mask) == 2);
    wq_pop_locknode();
    if (!wq_empty()) {
      // mark subsequent 'waiting' shared-lock nodes as 'lockable'
      node* p = queue_.next;
      while (p != &queue_ && (p->status & node_status_mask) == 1) {
        p->status |= 2;
        p = p->next;
      }
    }
    cv_.notify_all();
  }

  template<typename Clock, typename Duration>
  bool impl_try_lockwait(std::unique_lock<std::mutex>& lk, const std::chrono::time_point<Clock, Duration>& tp)
  {
    if (!wq_empty()) {
      node request = {0, 0, 0};  // exclusive-lock
      wq_push_back(&request);
      while (queue_.next != &request) {
        if (cv_.wait_until(lk, tp) == std::cv_status::timeout) {
          if (queue_.next == &request)  // re-check predicate
            break;
          if (request.prev == &locked_ && (locked_.status & node_status_mask) == 3) {
            //
            // Phase-marging: If previous node represents current shared-lock,
            // mark subsequent 'waiting' shared-lock nodes as 'lockable'.
            //
            node* p = request.next;
            while (p != &queue_ && (p->status & node_status_mask) == 1) {
              p->status |= 2;
              p = p->next;
            }
            cv_.notify_all();
          }
          wq_erase(&request);
          return false;
        }
      }
      wq_erase(&request);
    }
    wq_push_locknode(0);
    return true;
  }

  void impl_lock_shared(std::unique_lock<std::mutex>& lk)
  {
    if (!wq_shared_lockable()) {
      node request = {1, 0, 0};  // shared-lock
      wq_push_back(&request);
      while (request.status != 3) {
        cv_.wait(lk);
      }
      wq_erase(&request);
    }
    wq_push_locknode(1);
  }

  bool impl_try_lock_shared()
  {
    if (!wq_shared_lockable()) {
      return false;
    }
    wq_push_locknode(1);
    return true;
  }

  void impl_unlock_shared()
  {
    assert(queue_.next == &locked_ && (locked_.status & node_status_mask) == 3);
    locked_.status -= node_nthread_inc;
    if (locked_.status < node_nthread_inc) {
      // all current shared-locks was unlocked
      wq_pop_locknode();
      cv_.notify_all();
    }
  }

  template<typename Clock, typename Duration>
  bool impl_try_lockwait_shared(std::unique_lock<std::mutex>& lk, const std::chrono::time_point<Clock, Duration>& tp)
  {
    if (!wq_shared_lockable()) {
      node request = {1, 0, 0};  // shared-lock
      wq_push_back(&request);
      while (request.status != 3) {
        if (cv_.wait_until(lk, tp) == std::cv_status::timeout) {
          if (request.status == 3)  // re-check predicate
            break;
          wq_erase(&request);
          return false;
        }
      }
      wq_erase(&request);
    }
    wq_push_locknode(1);
    return true;
  }
};

} // namespace detail


class shared_mutex : private detail::shared_mutex_base {
  using base = detail::shared_mutex_base;

public:
  shared_mutex() = default;
  ~shared_mutex() = default;

  shared_mutex(const shared_mutex&) = delete;
  shared_mutex& operator=(const shared_mutex&) = delete;

  void lock()
  {
    std::unique_lock<decltype(mtx_)> lk(mtx_);
    base::impl_lock(lk);
  }

  bool try_lock()
  {
    std::lock_guard<decltype(mtx_)> lk(mtx_);
    return base::impl_try_lock();
  }

  void unlock()
  {
    std::lock_guard<decltype(mtx_)> lk(mtx_);
    base::impl_unlock();
  }

  void lock_shared()
  {
    std::unique_lock<decltype(mtx_)> lk(mtx_);
    base::impl_lock_shared(lk);
  }

  bool try_lock_shared()
  {
    std::lock_guard<decltype(mtx_)> lk(mtx_);
    return base::impl_try_lock_shared();
  }

  void unlock_shared()
  {
    std::lock_guard<decltype(mtx_)> lk(mtx_);
    base::impl_unlock_shared();
  }
};


class shared_timed_mutex : private detail::shared_mutex_base {
  using base = detail::shared_mutex_base;

public:
  shared_timed_mutex() = default;
  ~shared_timed_mutex() = default;

  shared_timed_mutex(const shared_timed_mutex&) = delete;
  shared_timed_mutex& operator=(const shared_timed_mutex&) = delete;

  void lock()
  {
    std::unique_lock<decltype(mtx_)> lk(mtx_);
    base::impl_lock(lk);
  }

  bool try_lock()
  {
    std::lock_guard<decltype(mtx_)> lk(mtx_);
    return base::impl_try_lock();
  }

  void unlock()
  {
    std::lock_guard<decltype(mtx_)> lk(mtx_);
    base::impl_unlock();
  }

  template<typename Rep, typename Period>
  bool try_lock_for(const std::chrono::duration<Rep, Period>& duration)
  {
    const auto tp = std::chrono::steady_clock::now() + duration;
    std::unique_lock<decltype(mtx_)> lk(mtx_);
    return base::impl_try_lockwait(lk, tp);
  }

  template<typename Clock, typename Duration>
  bool try_lock_until(const std::chrono::time_point<Clock, Duration>& tp)
  {
    std::unique_lock<decltype(mtx_)> lk(mtx_);
    return base::impl_try_lockwait(lk, tp);
  }

  void lock_shared()
  {
    std::unique_lock<decltype(mtx_)> lk(mtx_);
    base::impl_lock_shared(lk);
  }

  bool try_lock_shared()
  {
    std::lock_guard<decltype(mtx_)> lk(mtx_);
    return base::impl_try_lock_shared();
  }

  void unlock_shared()
  {
    std::lock_guard<decltype(mtx_)> lk(mtx_);
    base::impl_unlock_shared();
  }

  template<typename Rep, typename Period>
  bool try_lock_shared_for(const std::chrono::duration<Rep, Period>& duration)
  {
    const auto tp = std::chrono::steady_clock::now() + duration;
    std::unique_lock<decltype(mtx_)> lk(mtx_);
    return base::impl_try_lockwait_shared(lk, tp);
  }

  template<typename Clock, typename Duration>
  bool try_lock_shared_until(const std::chrono::time_point<Clock, Duration>& tp)
  {
    std::unique_lock<decltype(mtx_)> lk(mtx_);
    return base::impl_try_lockwait_shared(lk, tp);
  }
};

} // namespace fair
} // namespace yamc

#endif
