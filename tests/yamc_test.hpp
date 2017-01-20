/*
 * yamc_test.hpp
 */
#ifndef YAMC_TEST_HPP_
#define YAMC_TEST_HPP_

#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>


namespace yamc {
namespace test {


/// auto join thread
class join_thread {
  std::thread thd_;

public:
  template<typename F>
  explicit join_thread(F f)
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
