/*
 * fair_mutex.hpp
 */
#ifndef YAMC_FAIR_MUTEX_HPP_
#define YAMC_FAIR_MUTEX_HPP_

#include <condition_variable>
#include <mutex>


namespace yamc {

/*
 * fairness (FIFO locking) mutex
 */
namespace fair {

class mutex {
  std::size_t next_ = 0;
  std::size_t curr_ = 0;
  std::condition_variable cv_;
  std::mutex mtx_;

public:
  mutex() = default;
  ~mutex() = default;

  mutex(const mutex&) = delete;
  mutex& operator=(const mutex&) = delete;

  void lock()
  {
    std::unique_lock<decltype(mtx_)> lk(mtx_);
    const std::size_t request = next_++;
    while (request != curr_) {
      cv_.wait(lk);
    }
  }

  bool try_lock()
  {
    std::lock_guard<decltype(mtx_)> lk(mtx_);
    if (next_ != curr_)
      return false;
    ++next_;
    return true;
  }

  void unlock()
  {
    std::lock_guard<decltype(mtx_)> lk(mtx_);
    ++curr_;
    cv_.notify_all();
  }
};

} // namespace fair
} // namespace yamc

#endif
