/*
 * naive_spin_mutex.hpp
 */
#ifndef YAMC_NAIVE_SIPN_MUTEX_HPP_
#define YAMC_NAIVE_SIPN_MUTEX_HPP_

#include <atomic>
#include <thread>


namespace yamc {

/*
 * naive Test-And-Sawp(TAS) spinlock implementation (with memory_order_seq_cst)
 */
namespace spin {

class mutex {
  std::atomic<int> state_{0};

public:
  mutex() = default;
  ~mutex() = default;

  mutex(const mutex&) = delete;
  mutex& operator=(const mutex&) = delete;

  void lock() {
    int expected = 0;
    while (!state_.compare_exchange_weak(expected, 1)) {
      std::this_thread::yield();
      expected = 0;
    }
  }

  bool try_lock() {
    int expected = 0;
    return state_.compare_exchange_weak(expected, 1);
  }

  void unlock() {
    state_.store(0);
  }
};

} // namespace spin


/*
 * naive Test-And-Sawp(TAS) spinlock implementation for weak hardware memory model
 */
namespace spin_weak {

class mutex {
  std::atomic<int> state_{0};

public:
  mutex() = default;
  ~mutex() = default;

  mutex(const mutex&) = delete;
  mutex& operator=(const mutex&) = delete;

  void lock() {
    int expected = 0;
    while (!state_.compare_exchange_weak(expected, 1, std::memory_order_acquire, std::memory_order_relaxed)) {
      std::this_thread::yield();
      expected = 0;
    }
  }

  bool try_lock() {
    int expected = 0;
    return state_.compare_exchange_weak(expected, 1, std::memory_order_acquire, std::memory_order_relaxed);
  }

  void unlock() {
    state_.store(0, std::memory_order_release);
  }
};

} // namespace spin_weak

} // namespace yamc

#endif
