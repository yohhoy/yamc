/*
 * ttas_spin_mutex.hpp
 */
#ifndef YAMC_TTAS_SIPN_MUTEX_HPP_
#define YAMC_TTAS_SIPN_MUTEX_HPP_

#include <atomic>
#include <thread>


namespace yamc {

/*
 * Test-and-Test-And-Swap(TTAS) spinlock implementation
 */
namespace spin_ttas {

class mutex {
  std::atomic<int> state_;

public:
  mutex() = default;
  ~mutex() = default;

  mutex(const mutex&) = delete;
  mutex& operator=(const mutex&) = delete;

  void lock() {
    do {
      while (state_.load(std::memory_order_relaxed) != 0) {
        std::this_thread::yield();
      }
      int state = 0;
    } while (!state_.compare_exchange_weak(state, 1, std::memory_order_acquire));
  }

  bool try_lock() {
    int state = 0;
    return state_.compare_exchange_weak(state, 1, std::memory_order_acquire);
  }

  void unlock() {
    state_.store(0, std::memory_order_release);
  }
};

} // namespace spin_ttas
} // namespace yamc

#endif
