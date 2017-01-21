/*
 * compile_test.hpp
 */
#include <chrono>
#include <mutex>
#include "naive_spin_mutex.hpp"
#include "ttas_spin_mutex.hpp"
#include "checked_mutex.hpp"
#include "fair_mutex.hpp"
#include "alternate_mutex.hpp"


template <typename Mutex>
void test_requirements()
{
  Mutex mtx;
  // Mutex::lock(), unlock()
  {
    std::lock_guard<Mutex> lk(mtx);
  }
  {
    std::unique_lock<Mutex> lk(mtx);
  }
  // Mutex::try_lock()
  {
    std::unique_lock<Mutex> lk(mtx, std::try_to_lock);
  }
}


template <typename TimedMutex>
void test_requirements_timed()
{
  test_requirements<TimedMutex>();
  TimedMutex mtx;
  // TimedMutex::try_lock_for()
  if (mtx.try_lock_for(std::chrono::nanoseconds(1))) {
    mtx.unlock();
  }
  if (mtx.try_lock_for(std::chrono::seconds(1))) {
    mtx.unlock();
  }
  if (mtx.try_lock_for(std::chrono::hours(1))) {  // shall immediately return 'true'...
    mtx.unlock();
  }
  // TimedMutex::try_lock_until()
  if (mtx.try_lock_until(std::chrono::system_clock::now())) {
    mtx.unlock();
  }
  if (mtx.try_lock_until(std::chrono::steady_clock::now())) {
    mtx.unlock();
  }
  if (mtx.try_lock_until(std::chrono::high_resolution_clock::now())) {
    mtx.unlock();
  }
}


int main()
{
  test_requirements<std::mutex>();

  test_requirements<yamc::spin::mutex>();
  test_requirements<yamc::spin_weak::mutex>();
  test_requirements<yamc::spin_ttas::mutex>();
  // spinlock mutex with yamc::backoff::* policy
  test_requirements<yamc::spin::basic_mutex<yamc::backoff::exponential<1000>>>();
  test_requirements<yamc::spin_weak::basic_mutex<yamc::backoff::exponential<1000>>>();
  test_requirements<yamc::spin_ttas::basic_mutex<yamc::backoff::exponential<1000>>>();
  test_requirements<yamc::spin::basic_mutex<yamc::backoff::yield>>();
  test_requirements<yamc::spin_weak::basic_mutex<yamc::backoff::yield>>();
  test_requirements<yamc::spin_ttas::basic_mutex<yamc::backoff::yield>>();
  test_requirements<yamc::spin::basic_mutex<yamc::backoff::busy>>();
  test_requirements<yamc::spin_weak::basic_mutex<yamc::backoff::busy>>();
  test_requirements<yamc::spin_ttas::basic_mutex<yamc::backoff::busy>>();

  test_requirements<yamc::checked::mutex>();
  test_requirements<yamc::checked::recursive_mutex>();
  test_requirements_timed<yamc::checked::timed_mutex>();
  test_requirements_timed<yamc::checked::recursive_timed_mutex>();
  test_requirements<yamc::fair::mutex>();
  test_requirements<yamc::fair::recursive_mutex>();
  test_requirements<yamc::alternate::recursive_mutex>();
  return 0;
}
