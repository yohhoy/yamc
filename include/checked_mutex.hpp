/*
 * checked_mutex.hpp
 */
#ifndef YAMC_CHECKED_MUTEX_HPP_
#define YAMC_CHECKED_MUTEX_HPP_

#include <cassert>
#include <condition_variable>
#include <mutex>
#include <system_error>
#include <thread>


namespace yamc {

/*
 * strict requirements checking for debug
 */
namespace checked {

class mutex {
  std::thread::id owner_;
  std::condition_variable cv_;
  std::mutex mtx_;

public:
  mutex() = default;
  ~mutex() noexcept(false)
  {
    if (owner_ != std::thread::id()) {
      // object liveness
      throw std::system_error(std::make_error_code(std::errc::resource_deadlock_would_occur), "abandoned mutex");
    }
  }

  mutex(const mutex&) = delete;
  mutex& operator=(const mutex&) = delete;

  void lock()
  {
    const auto tid = std::this_thread::get_id();
    std::unique_lock<std::mutex> lk(mtx_);
    if (owner_ == tid) {
      // non-recursive semantics
      throw std::system_error(std::make_error_code(std::errc::resource_deadlock_would_occur), "recursive lock");
    }
    while (owner_ != std::thread::id()) {
      cv_.wait(lk);
    }
    owner_ = tid;
  }

  bool try_lock()
  {
    const auto tid = std::this_thread::get_id();
    std::lock_guard<std::mutex> lk(mtx_);
    if (owner_ == tid) {
      // non-recursive semantics
      throw std::system_error(std::make_error_code(std::errc::resource_deadlock_would_occur), "recursive try_lock");
    }
    if (owner_ != std::thread::id()) {
      return false;
    }
    owner_ = tid;
    return true;
  }

  void unlock()
  {
    std::lock_guard<std::mutex> lk(mtx_);
    if (owner_ != std::this_thread::get_id()) {
      // owner thread
      throw std::system_error(std::make_error_code(std::errc::operation_not_permitted), "invalid unlock");
    }
    owner_ = std::thread::id();
    cv_.notify_all();
  }
};


class recursive_mutex {
  std::size_t ncount_ = 0;
  std::thread::id owner_;
  std::condition_variable cv_;
  std::mutex mtx_;

public:
  recursive_mutex() = default;
  ~recursive_mutex() noexcept(false)
  {
    if (ncount_ != 0 || owner_ != std::thread::id()) {
      // object liveness
      throw std::system_error(std::make_error_code(std::errc::resource_deadlock_would_occur), "abandoned recursive_mutex");
    }
  }

  recursive_mutex(const recursive_mutex&) = delete;
  recursive_mutex& operator=(const recursive_mutex&) = delete;

  void lock()
  {
    const auto tid = std::this_thread::get_id();
    std::unique_lock<std::mutex> lk(mtx_);
    while (owner_ != std::thread::id() && owner_ != tid) {
      cv_.wait(lk);
    }
    if (ncount_ == 0) {
      assert(owner_ == std::thread::id());
      ncount_ = 1;
      owner_ = tid;
    } else {
      assert(owner_ == tid);
      ++ncount_;
    }
  }

  bool try_lock()
  {
    const auto tid = std::this_thread::get_id();
    std::lock_guard<std::mutex> lk(mtx_);
    if (owner_ == tid) {
      ++ncount_;
      return true;
    }
    else if (owner_ != std::thread::id()) {
      return false;
    }
    ncount_ = 1;
    owner_ = tid;
    return true;
  }

  void unlock()
  {
    std::lock_guard<std::mutex> lk(mtx_);
    if (owner_ != std::this_thread::get_id()) {
      // owner thread
      throw std::system_error(std::make_error_code(std::errc::operation_not_permitted), "invalid unlock");
    }
    assert(0 < ncount_);
    if (--ncount_ == 0) {
      owner_ = std::thread::id();
      cv_.notify_all();
    }
  }
};


} // namespace checked
} // namespace yamc

#endif
