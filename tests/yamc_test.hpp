/*
 * yamc_test.hpp
 */
#ifndef YAMC_TEST_HPP_
#define YAMC_TEST_HPP_

#include <condition_variable>
#include <memory>
#include <mutex>


namespace yamc {
namespace test {


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

} // namespace test


namespace cxx {

/// C++14 std::make_unique()
template<typename T, typename... Args>
inline
std::unique_ptr<T> make_unique(Args... args)
{
  return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

} // namespace cxx

} // namespace yamc

#endif
