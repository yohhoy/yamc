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
#include "alternate_mutex.hpp"
#include "alternate_shared_mutex.hpp"
#include "yamc_shared_lock.hpp"


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
  SharedMutex mtx;
  // SharedMutex::lock(), unlock()
  {
    std::lock_guard<SharedMutex> lk(mtx);
  }
  {
    std::unique_lock<SharedMutex> lk(mtx);
  }
  // SharedMutex::try_lock()
  {
    std::unique_lock<SharedMutex> lk(mtx, std::try_to_lock);
  }
  // SharedMutex::lock_shared(), unlock_shared()
  {
    yamc::shared_lock<SharedMutex> lk(mtx);
  }
  // SharedMutex::try_lock_shared()
  {
    yamc::shared_lock<SharedMutex> lk(mtx, std::try_to_lock);
  }
}


template <typename SharedTimedMutex>
void test_requirements_shared_timed()
{
  test_requirements_shared<SharedTimedMutex>();
  SharedTimedMutex mtx;
  // SharedTimedMutex::try_lock_for()
  if (mtx.try_lock_for(std::chrono::nanoseconds(1))) {
    mtx.unlock();
  }
  if (mtx.try_lock_for(std::chrono::seconds(1))) {
    mtx.unlock();
  }
  if (mtx.try_lock_for(std::chrono::hours(1))) {  // shall immediately return 'true'...
    mtx.unlock();
  }
  // SharedTimedMutex::try_lock_until()
  if (mtx.try_lock_until(std::chrono::system_clock::now())) {
    mtx.unlock();
  }
  if (mtx.try_lock_until(std::chrono::steady_clock::now())) {
    mtx.unlock();
  }
  if (mtx.try_lock_until(std::chrono::high_resolution_clock::now())) {
    mtx.unlock();
  }
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


void test_shared_lock()
{
  {
    using mutex_type = yamc::alternate::shared_mutex;
    using shared_lock = yamc::shared_lock<mutex_type>;
    mutex_type mtx;
    // default-ctor
    {
      shared_lock lk;
      static_assert(noexcept(shared_lock{}), "noexcept(default constructor");
    }
    // ctor(mutex)
    {
      shared_lock lk(mtx);
    }
    // ctor(defer_lock) + lock,try_lock,unlock
    {
      shared_lock lk(mtx, std::defer_lock);
      static_assert(noexcept(shared_lock(mtx, std::defer_lock)), "noexcept(defer_lock constructor)");
      lk.lock();
      lk.unlock();
      lk.try_lock();
    }
    // ctor(try_to_lock)
    {
      shared_lock lk(mtx, std::try_to_lock);
    }
    // ctor(adopt_lock) + getters
    {
      mtx.lock_shared();
      shared_lock lk(mtx, std::adopt_lock);
      lk.owns_lock();
      static_assert(noexcept(lk.owns_lock()), "noexcept(owns_lock)");
      lk.mutex();
      static_assert(noexcept(lk.mutex()), "noexcept(mutex)");
      if (lk) {}  // operator bool()
      static_assert(noexcept(static_cast<bool>(lk)), "noexcept(operator bool)");
    }
    // move-ctor + setters
    {
      shared_lock lk1;
      shared_lock lk2 = std::move(lk1);
      static_assert(noexcept(shared_lock(std::move(lk1))), "noexcept(move constructor)");
      lk1.swap(lk2);
      static_assert(noexcept(lk1.swap(lk2)), "noexcept(swap)");
      lk1.release();
      static_assert(noexcept(lk1.release()), "noexcept(release)");
    }
    // move-assignment + std::swap overloading
    {
      shared_lock lk1;
      shared_lock lk2;
      lk2 = std::move(lk1);
      static_assert(noexcept(lk2 = std::move(lk1)), "noexcept(move assingment)");
      std::swap(lk1, lk2);
      static_assert(noexcept(std::swap(lk1, lk2)), "noexcept(std::swap)");
    }
  }
  {
    using mutex_type = yamc::alternate::shared_timed_mutex;
    using shared_lock = yamc::shared_lock<mutex_type>;
    using Clock = std::chrono::system_clock;
    mutex_type mtx;
    // ctor(rel_time)
    {
      shared_lock lk(mtx, std::chrono::seconds(1));
    }
    // ctor(abs_time)
    {
      shared_lock lk(mtx, Clock::now() + std::chrono::seconds(1));
    }
    // try_lock_for,try_lock_until
    {
      shared_lock lk(mtx, std::defer_lock);
      lk.try_lock_for(std::chrono::seconds(1));
      lk.unlock();
      lk.try_lock_until(Clock::now() + std::chrono::seconds(1));
    }
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
  test_requirements<yamc::checked::shared_mutex>();
  test_requirements_timed<yamc::checked::shared_timed_mutex>();

  test_requirements<yamc::fair::mutex>();
  test_requirements<yamc::fair::recursive_mutex>();
  test_requirements_timed<yamc::fair::timed_mutex>();
  test_requirements_timed<yamc::fair::recursive_timed_mutex>();

  test_requirements<yamc::alternate::recursive_mutex>();
  test_requirements_timed<yamc::alternate::timed_mutex>();
  test_requirements_timed<yamc::alternate::recursive_timed_mutex>();

  test_shared_lock();
  test_requirements_shared<yamc::alternate::shared_mutex>();
  test_requirements_shared_timed<yamc::alternate::shared_timed_mutex>();
  // shared_(timed_)mutex with yamc::rwlock::* policy
  test_requirements_shared<yamc::alternate::basic_shared_mutex<yamc::rwlock::ReaderPrefer>>();
  test_requirements_shared<yamc::alternate::basic_shared_mutex<yamc::rwlock::WriterPrefer>>();
  test_requirements_shared_timed<yamc::alternate::basic_shared_timed_mutex<yamc::rwlock::ReaderPrefer>>();
  test_requirements_shared_timed<yamc::alternate::basic_shared_timed_mutex<yamc::rwlock::WriterPrefer>>();
  return 0;
}
