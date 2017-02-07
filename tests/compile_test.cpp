/*
 * compile_test.hpp
 */
#include <chrono>
#include <mutex>
#include <utility>  // std::{move,swap}
#include "naive_spin_mutex.hpp"
#include "ttas_spin_mutex.hpp"
#include "checked_mutex.hpp"
#include "checked_shared_mutex.hpp"
#include "fair_mutex.hpp"
#include "fair_shared_mutex.hpp"
#include "alternate_mutex.hpp"
#include "alternate_shared_mutex.hpp"
#include "yamc_testutil.hpp"


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


template <typename SharedMutex>
void test_requirements_shared()
{
  test_requirements<SharedMutex>();
  SharedMutex mtx;
  // SharedMutex::lock_shared(), unlock_shared()
  mtx.lock_shared();
  mtx.unlock_shared();
  // SharedMutex::try_lock_shared()
  if (mtx.try_lock_shared()) {
    mtx.unlock_shared();
  }
}


template <typename SharedTimedMutex>
void test_requirements_shared_timed()
{
  test_requirements_shared<SharedTimedMutex>();
  test_requirements_timed<SharedTimedMutex>();
  SharedTimedMutex mtx;
  // SharedTimedMutex::try_lock_shared_for()
  if (mtx.try_lock_shared_for(std::chrono::nanoseconds(1))) {
    mtx.unlock_shared();
  }
  if (mtx.try_lock_shared_for(std::chrono::seconds(1))) {
    mtx.unlock_shared();
  }
  if (mtx.try_lock_shared_for(std::chrono::hours(1))) {  // shall immediately return 'true'...
    mtx.unlock_shared();
  }
  // SharedTimedMutex::try_lock_shared_until()
  if (mtx.try_lock_shared_until(std::chrono::system_clock::now())) {
    mtx.unlock_shared();
  }
  if (mtx.try_lock_shared_until(std::chrono::steady_clock::now())) {
    mtx.unlock_shared();
  }
  if (mtx.try_lock_shared_until(std::chrono::high_resolution_clock::now())) {
    mtx.unlock_shared();
  }
}


int main()
{

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
  test_requirements_shared<yamc::checked::shared_mutex>();
  test_requirements_shared_timed<yamc::checked::shared_timed_mutex>();

  test_requirements<yamc::fair::mutex>();
  test_requirements<yamc::fair::recursive_mutex>();
  test_requirements_timed<yamc::fair::timed_mutex>();
  test_requirements_timed<yamc::fair::recursive_timed_mutex>();
  test_requirements_shared<yamc::fair::shared_mutex>();
  test_requirements_shared_timed<yamc::fair::shared_timed_mutex>();

  test_requirements<yamc::alternate::mutex>();
  test_requirements<yamc::alternate::recursive_mutex>();
  test_requirements_timed<yamc::alternate::timed_mutex>();
  test_requirements_timed<yamc::alternate::recursive_timed_mutex>();

  test_requirements_shared<yamc::alternate::shared_mutex>();
  test_requirements_shared_timed<yamc::alternate::shared_timed_mutex>();
  // shared_(timed_)mutex with yamc::rwlock::* policy
  test_requirements_shared<yamc::alternate::basic_shared_mutex<yamc::rwlock::ReaderPrefer>>();
  test_requirements_shared<yamc::alternate::basic_shared_mutex<yamc::rwlock::WriterPrefer>>();
  test_requirements_shared_timed<yamc::alternate::basic_shared_timed_mutex<yamc::rwlock::ReaderPrefer>>();
  test_requirements_shared_timed<yamc::alternate::basic_shared_timed_mutex<yamc::rwlock::WriterPrefer>>();
  return 0;
}
