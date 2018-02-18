/*
 * fair_shared_mutex.hpp
 *
 * MIT License
 *
 * Copyright (c) 2018 yohhoy
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


/// default shared_mutex rwlock fairness policy
#ifndef YAMC_RWLOCK_FAIRNESS_DEFAULT
#define YAMC_RWLOCK_FAIRNESS_DEFAULT yamc::rwlock::TaskFairness
#endif


#ifndef YAMC_DEBUG_TRACING
#define YAMC_DEBUG_TRACING 0
#endif

#if YAMC_DEBUG_TRACING
#include <cstdio>
#define YAMC_DEBUG_TRACE(...)   std::printf(__VA_ARGS__)
#define YAMC_DEBUG_DUMPQ(msg_)  wq_dump(msg_)
#else
#define YAMC_DEBUG_TRACE(...)   (void)0
#define YAMC_DEBUG_DUMPQ(msg_)  (void)0
#endif


namespace yamc {

/*
 * readers-writer locking fairness policy for basic_shared_(timed)_mutex<RwLockFairness>
 *
 * - yamc::rwlock::TaskFairness
 * - yamc::rwlock::PhaseFairness
 */
namespace rwlock {

struct TaskFairness {
  static constexpr bool phased = false;
};

struct PhaseFairness {
  static constexpr bool phased = true;
};

} // namespace rwlock


/*
 * fairness (FIFO locking) shared mutex
 *
 * - yamc::fair::basic_shared_mutex<RwLockFairness>
 * - yamc::fair::basic_shared_timed_mutex<RwLockFairness>
 */
namespace fair {

namespace detail {

template <typename RwLockFairness>
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
#if YAMC_DEBUG_TRACING
  void wq_dump(const char* msg)
  {
    YAMC_DEBUG_TRACE("%s [", msg);
    for (node* p = queue_.next; p != &queue_; p = p->next) {
      const char* delim = (p->next != &queue_) ? ", " : "";
      char sym = ((p->status & 1) ? 'S' : 'E') + ((p->status & 2) ? 0 : 'a'-'A');
      if (p == &locked_) {
        YAMC_DEBUG_TRACE("L%lu:%c%s", (p->status >> 2), sym, delim);
      } else {
        YAMC_DEBUG_TRACE("R:%c%s", sym, delim);
      }
    }
    YAMC_DEBUG_TRACE("]\n");
  }
#endif

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
    YAMC_DEBUG_DUMPQ(">>lock");
    if (!wq_empty()) {
      node request = {0, 0, 0};  // exclusive-lock
      wq_push_back(&request);
      YAMC_DEBUG_DUMPQ("  lock/wait");
      while (queue_.next != &request) {
        cv_.wait(lk);
      }
      wq_erase(&request);
    }
    wq_push_locknode(0);
    YAMC_DEBUG_DUMPQ("<<lock");
  }

  bool impl_try_lock()
  {
    YAMC_DEBUG_DUMPQ(">>try_lock");
    if (!wq_empty()) {
      YAMC_DEBUG_DUMPQ("<<try_lock/false");
      return false;
    }
    wq_push_locknode(0);
    YAMC_DEBUG_DUMPQ("<<try_lock/true");
    return true;
  }

  void impl_unlock()
  {
    YAMC_DEBUG_DUMPQ(">>unlock");
    assert(queue_.next == &locked_ && (locked_.status & node_status_mask) == 2);
    wq_pop_locknode();
    if (!wq_empty()) {
      // mark subsequent shared-lock nodes as 'lockable'
      if (RwLockFairness::phased) {
        // PhaseFairness: mark all queued shared-lock nodes if the next is (waiting) shared-lock.
        if ((queue_.next->status & node_status_mask) == 1) {
          for (node* p = queue_.next; p != &queue_; p = p->next) {
            if ((p->status & node_status_mask) == 1) {
              p->status |= 2;
            }
          }
        }
      } else {
        // TaskFairness: mark directly subsequent shared-lock nodes group.
        node* p = queue_.next;
        while (p != &queue_ && (p->status & node_status_mask) == 1) {
          p->status |= 2;
          p = p->next;
        }
      }
    }
    cv_.notify_all();
    YAMC_DEBUG_DUMPQ("<<unlock");
  }

  template<typename Clock, typename Duration>
  bool impl_try_lockwait(std::unique_lock<std::mutex>& lk, const std::chrono::time_point<Clock, Duration>& tp)
  {
    YAMC_DEBUG_DUMPQ(">>try_lockwait");
    if (!wq_empty()) {
      node request = {0, 0, 0};  // exclusive-lock
      wq_push_back(&request);
      YAMC_DEBUG_DUMPQ("  try_lockwait/wait");
      while (queue_.next != &request) {
        if (cv_.wait_until(lk, tp) == std::cv_status::timeout) {
          if (queue_.next == &request)  // re-check predicate
            break;
          if ((request.prev->status & node_status_mask) == 3) {
            //
            // When exclusive-lock timeout and previous shared-lock is 'lockable(-ing)',
            // mark directly subsequent 'waiting' shared-lock nodes group as 'lockable'.
            //
            node* p = request.next;
            while (p != &queue_ && (p->status & node_status_mask) == 1) {
              p->status |= 2;
              p = p->next;
            }
            cv_.notify_all();
          }
          wq_erase(&request);
          YAMC_DEBUG_DUMPQ("<<try_lockwait/false");
          return false;
        }
      }
      wq_erase(&request);
    }
    wq_push_locknode(0);
    YAMC_DEBUG_DUMPQ("<<try_lockwait/true");
    return true;
  }

  void impl_lock_shared(std::unique_lock<std::mutex>& lk)
  {
    YAMC_DEBUG_DUMPQ(">>lock_shared");
    if (!wq_shared_lockable()) {
      node request = {1, 0, 0};  // shared-lock
      wq_push_back(&request);
      YAMC_DEBUG_DUMPQ("  lock_shared/wait");
      while (request.status != 3) {
        cv_.wait(lk);
      }
      wq_erase(&request);
    }
    wq_push_locknode(1);
    YAMC_DEBUG_DUMPQ("<<lock_shared");
  }

  bool impl_try_lock_shared()
  {
    YAMC_DEBUG_DUMPQ(">>try_lock_shared");
    if (!wq_shared_lockable()) {
      YAMC_DEBUG_DUMPQ("<<try_lock_shared/false");
      return false;
    }
    wq_push_locknode(1);
    YAMC_DEBUG_DUMPQ("<<try_lock_shared/true");
    return true;
  }

  void impl_unlock_shared()
  {
    YAMC_DEBUG_DUMPQ(">>unlock_shared");
    assert(queue_.next == &locked_ && (locked_.status & node_status_mask) == 3);
    locked_.status -= node_nthread_inc;
    if (locked_.status < node_nthread_inc) {
      // all current shared-locks was unlocked
      wq_pop_locknode();
      cv_.notify_all();
    }
    YAMC_DEBUG_DUMPQ("<<unlock_shared");
  }

  template<typename Clock, typename Duration>
  bool impl_try_lockwait_shared(std::unique_lock<std::mutex>& lk, const std::chrono::time_point<Clock, Duration>& tp)
  {
    YAMC_DEBUG_DUMPQ(">>try_lockwait_shared");
    if (!wq_shared_lockable()) {
      node request = {1, 0, 0};  // shared-lock
      wq_push_back(&request);
      YAMC_DEBUG_DUMPQ("  try_lockwait_shared/wait");
      while (request.status != 3) {
        if (cv_.wait_until(lk, tp) == std::cv_status::timeout) {
          if (request.status == 3)  // re-check predicate
            break;
          wq_erase(&request);
          YAMC_DEBUG_DUMPQ("<<try_lockwait_shared/false");
          return false;
        }
      }
      wq_erase(&request);
    }
    wq_push_locknode(1);
    YAMC_DEBUG_DUMPQ("<<try_lockwait_shared/true");
    return true;
  }
};

} // namespace detail


template <typename RwLockFairness>
class basic_shared_mutex : private detail::shared_mutex_base<RwLockFairness> {
  using base = detail::shared_mutex_base<RwLockFairness>;
  using base::mtx_;

public:
  basic_shared_mutex() = default;
  ~basic_shared_mutex() = default;

  basic_shared_mutex(const basic_shared_mutex&) = delete;
  basic_shared_mutex& operator=(const basic_shared_mutex&) = delete;

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

using shared_mutex = basic_shared_mutex<YAMC_RWLOCK_FAIRNESS_DEFAULT>;


template <typename RwLockFairness>
class basic_shared_timed_mutex : private detail::shared_mutex_base<RwLockFairness> {
  using base = detail::shared_mutex_base<RwLockFairness>;
  using base::mtx_;

public:
  basic_shared_timed_mutex() = default;
  ~basic_shared_timed_mutex() = default;

  basic_shared_timed_mutex(const basic_shared_timed_mutex&) = delete;
  basic_shared_timed_mutex& operator=(const basic_shared_timed_mutex&) = delete;

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

using shared_timed_mutex = basic_shared_timed_mutex<YAMC_RWLOCK_FAIRNESS_DEFAULT>;


} // namespace fair
} // namespace yamc

#endif
