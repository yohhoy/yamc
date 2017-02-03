/*
 * basic_test.cpp
 */
#include <atomic>
#include <chrono>
#include <mutex>
#include <type_traits>
#include "gtest/gtest.h"
#include "naive_spin_mutex.hpp"
#include "ttas_spin_mutex.hpp"
#include "checked_mutex.hpp"
#include "fair_mutex.hpp"
#include "alternate_mutex.hpp"
#include "alternate_shared_mutex.hpp"
#include "yamc_shared_lock.hpp"
#include "yamc_testutil.hpp"


#define TEST_THREADS   8
#define TEST_ITERATION 10000u

#define TEST_NOT_TIMEOUT    std::chrono::minutes(3)
#define TEST_EXPECT_TIMEOUT std::chrono::milliseconds(500)

#define ASSERT_THORW_SYSTEM_ERROR(errorcode_, block_) \
  try { \
    block_ \
  } catch (const std::system_error& e) { \
    ASSERT_EQ(errorcode_, e.code()); \
  }


using NormalMutexTypes = ::testing::Types<
  yamc::spin::mutex,
  yamc::spin_weak::mutex,
  yamc::spin_ttas::mutex,
  yamc::checked::mutex,
  yamc::checked::timed_mutex,
  yamc::fair::mutex,
  yamc::fair::timed_mutex,
  yamc::alternate::shared_mutex
>;

template <typename Mutex>
struct NormalMutexTest : ::testing::Test {};

TYPED_TEST_CASE(NormalMutexTest, NormalMutexTypes);

// mutex::lock()
TYPED_TEST(NormalMutexTest, BasicLock)
{
  TypeParam mtx;
  std::size_t counter = 0;
  yamc::test::task_runner(
    TEST_THREADS,
    [&](std::size_t /*id*/) {
      for (std::size_t n = 0; n < TEST_ITERATION; ++n) {
        std::lock_guard<decltype(mtx)> lk(mtx);
        counter = counter + 1;
      }
    });
  ASSERT_EQ(TEST_ITERATION * TEST_THREADS, counter);
}

// mutex::try_lock()
TYPED_TEST(NormalMutexTest, TryLock)
{
  TypeParam mtx;
  std::size_t counter = 0;
  yamc::test::task_runner(
    TEST_THREADS,
    [&](std::size_t /*id*/) {
      for (std::size_t n = 0; n < TEST_ITERATION; ++n) {
        while (!mtx.try_lock()) {
          std::this_thread::yield();
        }
        std::lock_guard<decltype(mtx)> lk(mtx, std::adopt_lock);
        counter = counter + 1;
      }
    });
  ASSERT_EQ(TEST_ITERATION * TEST_THREADS, counter);
}

// mutex::try_lock() failure
TYPED_TEST(NormalMutexTest, TryLockFail)
{
  yamc::test::barrier step(2);
  TypeParam mtx;
  yamc::test::join_thread thd([&]{
    ASSERT_NO_THROW(mtx.lock());
    step.await();  // b1
    step.await();  // b2
    ASSERT_NO_THROW(mtx.unlock());
  });
  {
    step.await();  // b1
    ASSERT_EQ(false, mtx.try_lock());
    step.await();  // b2
  }
}


using RecursiveMutexTypes = ::testing::Types<
  yamc::checked::recursive_mutex,
  yamc::checked::recursive_timed_mutex,
  yamc::fair::recursive_mutex,
  yamc::fair::recursive_timed_mutex,
  yamc::alternate::recursive_mutex,
  yamc::alternate::recursive_timed_mutex
>;

template <typename Mutex>
struct RecursiveMutexTest : ::testing::Test {};

TYPED_TEST_CASE(RecursiveMutexTest, RecursiveMutexTypes);

// recursive_mutex::lock()
TYPED_TEST(RecursiveMutexTest, BasicLock)
{
  TypeParam mtx;
  std::size_t c1 = 0, c2 = 0, c3 = 0;
  yamc::test::task_runner(
    TEST_THREADS,
    [&](std::size_t /*id*/) {
      for (std::size_t n = 0; n < TEST_ITERATION; ++n) {
        std::lock_guard<decltype(mtx)> lk1(mtx);
        auto before_cnt = ++c1;
        {
          std::lock_guard<decltype(mtx)> lk2(mtx);
          ++c2;
        }
        auto after_cnt = ++c3;
        ASSERT_TRUE(before_cnt == after_cnt);
      }
    });
  ASSERT_EQ(TEST_ITERATION * TEST_THREADS, c1);
  ASSERT_EQ(TEST_ITERATION * TEST_THREADS, c2);
  ASSERT_EQ(TEST_ITERATION * TEST_THREADS, c3);
}

// recursive_mutex::try_lock()
TYPED_TEST(RecursiveMutexTest, TryLock)
{
  TypeParam mtx;
  std::size_t c1 = 0, c2 = 0, c3 = 0;
  yamc::test::task_runner(
    TEST_THREADS,
    [&](std::size_t /*id*/) {
      for (std::size_t n = 0; n < TEST_ITERATION; ++n) {
        while (!mtx.try_lock()) {
          std::this_thread::yield();
        }
        std::lock_guard<decltype(mtx)> lk1(mtx, std::adopt_lock);
        ++c1;
        {
          ASSERT_EQ(true, mtx.try_lock());
          std::lock_guard<decltype(mtx)> lk2(mtx, std::adopt_lock);
          ++c2;
        }
        ++c3;
      }
    });
  ASSERT_EQ(TEST_ITERATION * TEST_THREADS, c1);
  ASSERT_EQ(TEST_ITERATION * TEST_THREADS, c2);
  ASSERT_EQ(TEST_ITERATION * TEST_THREADS, c3);
}

// recursive_mutex::try_lock() failure
TYPED_TEST(RecursiveMutexTest, TryLockFail)
{
  yamc::test::barrier step(2);
  TypeParam mtx;
  yamc::test::join_thread thd([&]{
    ASSERT_NO_THROW(mtx.lock());
    step.await();  // b1
    step.await();  // b2
    ASSERT_NO_THROW(mtx.lock());
    step.await();  // b3
    step.await();  // b4
    ASSERT_NO_THROW(mtx.unlock());
    step.await();  // b5
    step.await();  // b6
    ASSERT_NO_THROW(mtx.unlock());
  });
  {
    step.await();  // b1
    ASSERT_EQ(false, mtx.try_lock());  // lockcnt = 1
    step.await();  // b2
    step.await();  // b3
    ASSERT_EQ(false, mtx.try_lock());  // lockcnt = 2
    step.await();  // b4
    step.await();  // b5
    ASSERT_EQ(false, mtx.try_lock());  // lockcnt = 3
    step.await();  // b6
  }
}


using TimedMutexTypes = ::testing::Types<
  yamc::checked::timed_mutex,
  yamc::checked::recursive_timed_mutex,
  yamc::fair::timed_mutex,
  yamc::fair::recursive_timed_mutex,
  yamc::alternate::timed_mutex,
  yamc::alternate::recursive_timed_mutex,
  yamc::alternate::shared_timed_mutex
>;

template <typename Mutex>
struct TimedMutexTest : ::testing::Test {};

TYPED_TEST_CASE(TimedMutexTest, TimedMutexTypes);

// (recursive_)timed_mutex::try_lock_for()
TYPED_TEST(TimedMutexTest, TryLockFor)
{
  TypeParam mtx;
  std::size_t counter = 0;
  yamc::test::task_runner(
    TEST_THREADS,
    [&](std::size_t /*id*/) {
      for (std::size_t n = 0; n < TEST_ITERATION; ++n) {
        while (!mtx.try_lock_for(TEST_NOT_TIMEOUT)) {
          std::this_thread::yield();
        }
        std::lock_guard<decltype(mtx)> lk(mtx, std::adopt_lock);
        counter = counter + 1;
      }
    });
  ASSERT_EQ(TEST_ITERATION * TEST_THREADS, counter);
}

// (recursive_)timed_mutex::try_lock_until()
TYPED_TEST(TimedMutexTest, TryLockUntil)
{
  TypeParam mtx;
  std::size_t counter = 0;
  yamc::test::task_runner(
    TEST_THREADS,
    [&](std::size_t /*id*/) {
      for (std::size_t n = 0; n < TEST_ITERATION; ++n) {
        const auto tp = std::chrono::system_clock::now() + TEST_NOT_TIMEOUT;
        while (!mtx.try_lock_until(tp)) {
          std::this_thread::yield();
        }
        std::lock_guard<decltype(mtx)> lk(mtx, std::adopt_lock);
        counter = counter + 1;
      }
    });
  ASSERT_EQ(TEST_ITERATION * TEST_THREADS, counter);
}

// (recursive_)timed_mutex::try_lock_for() timeout
TYPED_TEST(TimedMutexTest, TryLockForTimeout)
{
  yamc::test::barrier step(2);
  TypeParam mtx;
  yamc::test::join_thread thd([&]{
    ASSERT_NO_THROW(mtx.lock());
    step.await();  // b1
    step.await();  // b2
    ASSERT_NO_THROW(mtx.unlock());
  });
  {
    step.await();  // b1
    yamc::test::stopwatch<> sw;
    bool result = mtx.try_lock_for(TEST_EXPECT_TIMEOUT);
    auto elapsed = sw.elapsed();
    ASSERT_EQ(false, result);
    EXPECT_LE(TEST_EXPECT_TIMEOUT, elapsed);
    step.await();  // b2
  }
}

// (recursive_)timed_mutex::try_lock_until() timeout
TYPED_TEST(TimedMutexTest, TryLockUntilTimeout)
{
  yamc::test::barrier step(2);
  TypeParam mtx;
  yamc::test::join_thread thd([&]{
    ASSERT_NO_THROW(mtx.lock());
    step.await();  // b1
    step.await();  // b2
    ASSERT_NO_THROW(mtx.unlock());
  });
  {
    step.await();  // b1
    const auto tp = std::chrono::system_clock::now() + TEST_EXPECT_TIMEOUT;
    yamc::test::stopwatch<> sw;
    bool result = mtx.try_lock_until(tp);
    auto elapsed = sw.elapsed();
    ASSERT_EQ(false, result);
    EXPECT_LE(TEST_EXPECT_TIMEOUT, elapsed);
    step.await();  // b2
  }
}


// lockfree property of atomic<int>
TEST(AtomicTest, Lockfree)
{
  // std::atomic<int> type is always lock-free
  ASSERT_EQ(2, ATOMIC_INT_LOCK_FREE);
  // std::atomic<int> is lock-free
  std::atomic<int> i;
  ASSERT_TRUE(i.is_lock_free());
}


// backoff::exponential<100>
TEST(BackoffTest, Exponential100)
{
  using BackoffPolicy = yamc::backoff::exponential<100>;
  BackoffPolicy::state state;
  ASSERT_EQ(100u, state.initcount);
  ASSERT_EQ(100u, state.counter);
  for (int i = 0; i < 100; ++i) {
    BackoffPolicy::wait(state);  // wait 100
  }
  ASSERT_EQ(0u, state.counter);
  for (int i = 0; i < 2000; ++i) {
    BackoffPolicy::wait(state);
  }
  ASSERT_EQ(1u, state.initcount);
  ASSERT_EQ(0u, state.counter);
  BackoffPolicy::wait(state);
  ASSERT_EQ(1u, state.initcount);
  ASSERT_EQ(0u, state.counter);
}

// backoff::exponential<1>
TEST(BackoffTest, Exponential1)
{
  using BackoffPolicy = yamc::backoff::exponential<1>;
  BackoffPolicy::state state;
  ASSERT_EQ(1u, state.initcount);
  ASSERT_EQ(1u, state.counter);
  BackoffPolicy::wait(state);
  ASSERT_EQ(1u, state.initcount);
  ASSERT_EQ(0u, state.counter);
}


using MockSharedMutex = yamc::mock::shared_mutex;
using MockSharedTimedMutex = yamc::mock::shared_timed_mutex;

// shared_lock::mutex_type
TEST(SharedLockTest, MutexType)
{
  bool shall_be_true = std::is_same<MockSharedMutex, yamc::shared_lock<MockSharedMutex>::mutex_type>::value;
  ASSERT_TRUE(shall_be_true);
}

// shared_lock() noexcept
TEST(SharedLockTest, CtorDefault)
{
  yamc::shared_lock<MockSharedMutex> lk;
  ASSERT_EQ(nullptr, lk.mutex());
  ASSERT_FALSE(lk.owns_lock());
  ASSERT_TRUE(noexcept(yamc::shared_lock<MockSharedMutex>{}));
}

// explicit shared_lock(mutex_type&)
TEST(SharedLockTest, CtorMutex)
{
  MockSharedMutex mtx;
  yamc::shared_lock<MockSharedMutex> lk(mtx);
  ASSERT_EQ(&mtx, lk.mutex());
  ASSERT_TRUE(lk.owns_lock());
}

// shared_lock(mutex_type&, defer_lock_t) noexcept
TEST(SharedLockTest, CtorDeferLock)
{
  MockSharedMutex mtx;
  yamc::shared_lock<MockSharedMutex> lk(mtx, std::defer_lock);
  ASSERT_EQ(&mtx, lk.mutex());
  ASSERT_FALSE(lk.owns_lock());
  ASSERT_TRUE(noexcept(yamc::shared_lock<MockSharedMutex>(mtx, std::defer_lock)));
}

// shared_lock(mutex_type&, try_to_lock_t)
TEST(SharedLockTest, CtorTryToLock)
{
  MockSharedMutex mtx;
  yamc::shared_lock<MockSharedMutex> lk(mtx, std::try_to_lock);
  ASSERT_EQ(&mtx, lk.mutex());
  ASSERT_TRUE(lk.owns_lock());
}

// shared_lock(mutex_type&, try_to_lock_t) failure
TEST(SharedLockTest, CtorTryToLockFail)
{
  MockSharedMutex mtx;
  mtx.retval_on_trylock = false;
  yamc::shared_lock<MockSharedMutex> lk(mtx, std::try_to_lock);
  ASSERT_EQ(&mtx, lk.mutex());
  ASSERT_FALSE(lk.owns_lock());
}

// shared_lock(mutex_type&, adopt_lock_t)
TEST(SharedLockTest, CtorAdoptLock)
{
  MockSharedMutex mtx;
  mtx.lock_shared();
  yamc::shared_lock<MockSharedMutex> lk(mtx, std::adopt_lock);
  ASSERT_EQ(&mtx, lk.mutex());
  ASSERT_TRUE(lk.owns_lock());
}

// shared_lock(mutex_type&, const chrono::time_point&)
TEST(SharedLockTest, CtorTimePoint)
{
  MockSharedTimedMutex mtx;
  yamc::shared_lock<MockSharedTimedMutex> lk(mtx, std::chrono::system_clock::now());
  ASSERT_EQ(&mtx, lk.mutex());
  ASSERT_TRUE(lk.owns_lock());
}

// shared_lock(mutex_type&, const chrono::time_point&) failure
TEST(SharedLockTest, CtorTimePointFail)
{
  MockSharedTimedMutex mtx;
  mtx.retval_on_trylock = false;
  yamc::shared_lock<MockSharedTimedMutex> lk(mtx, std::chrono::system_clock::now());
  ASSERT_EQ(&mtx, lk.mutex());
  ASSERT_FALSE(lk.owns_lock());
}

// shared_lock(mutex_type&, const chrono::duration&)
TEST(SharedLockTest, CtorRelTime)
{
  MockSharedTimedMutex mtx;
  yamc::shared_lock<MockSharedTimedMutex> lk(mtx, std::chrono::milliseconds(1));
  ASSERT_EQ(&mtx, lk.mutex());
  ASSERT_TRUE(lk.owns_lock());
}

// shared_lock(mutex_type&, const chrono::duration&) failure
TEST(SharedLockTest, CtorRelTimeFail)
{
  MockSharedTimedMutex mtx;
  mtx.retval_on_trylock = false;
  yamc::shared_lock<MockSharedTimedMutex> lk(mtx, std::chrono::milliseconds(1));
  ASSERT_EQ(&mtx, lk.mutex());
  ASSERT_FALSE(lk.owns_lock());
}

// shared_lock(shared_lock&&) noexcept
TEST(SharedLockTest, MoveCtor)
{
  MockSharedMutex mtx;
  yamc::shared_lock<MockSharedMutex> lk1(mtx);
  yamc::shared_lock<MockSharedMutex> lk2(std::move(lk1));  // move-constructor
  ASSERT_EQ(nullptr, lk1.mutex());
  ASSERT_FALSE(lk1.owns_lock());
  ASSERT_EQ(&mtx, lk2.mutex());
  ASSERT_TRUE(lk2.owns_lock());
  ASSERT_TRUE(noexcept(yamc::shared_lock<MockSharedMutex>(std::move(lk2))));
}

// shared_lock& operator=(shared_lock&&) noexcept
TEST(SharedLockTest, MoveAssign)
{
  MockSharedMutex mtx;
  yamc::shared_lock<MockSharedMutex> lk1(mtx);
  yamc::shared_lock<MockSharedMutex> lk2;
  lk2 = std::move(lk1);  // move-assignment
  ASSERT_EQ(nullptr, lk1.mutex());
  ASSERT_FALSE(lk1.owns_lock());
  ASSERT_EQ(&mtx, lk2.mutex());
  ASSERT_TRUE(lk2.owns_lock());
  ASSERT_TRUE(noexcept(lk1 = std::move(lk2)));
}

// lock()
TEST(SharedLockTest, Lock)
{
  MockSharedMutex mtx;
  yamc::shared_lock<MockSharedMutex> lk(mtx, std::defer_lock);
  ASSERT_NO_THROW(lk.lock());
  ASSERT_TRUE(lk.owns_lock());
}

// lock() throw exception/operation_not_permitted
TEST(SharedLockTest, LockThrowEPERM)
{
  {
    yamc::shared_lock<MockSharedMutex> lk;
    ASSERT_THROW(lk.lock(), std::system_error);
  }
  {
    yamc::shared_lock<MockSharedMutex> lk;
    ASSERT_THORW_SYSTEM_ERROR(std::errc::operation_not_permitted, {
      lk.lock();
    });
  }
}

// lock() throw exception/resource_deadlock_would_occur
TEST(SharedLockTest, LockThrowEDEADLK)
{
  {
    yamc::shared_lock<MockSharedMutex> lk;
    ASSERT_THROW(lk.lock(), std::system_error);
  }
  {
    MockSharedMutex mtx;
    yamc::shared_lock<MockSharedMutex> lk(mtx);
    ASSERT_THORW_SYSTEM_ERROR(std::errc::resource_deadlock_would_occur, {
      lk.lock();
    });
  }
}

// try_lock()
TEST(SharedLockTest, TryLock)
{
  MockSharedMutex mtx;
  yamc::shared_lock<MockSharedMutex> lk(mtx, std::defer_lock);
  ASSERT_TRUE(lk.try_lock());
  ASSERT_TRUE(lk.owns_lock());
}

// try_lock() failure
TEST(SharedLockTest, TryLockFail)
{
  MockSharedMutex mtx;
  mtx.retval_on_trylock = false;
  yamc::shared_lock<MockSharedMutex> lk(mtx, std::defer_lock);
  ASSERT_FALSE(lk.try_lock());
  ASSERT_FALSE(lk.owns_lock());
}

// try_lock() throw exception/operation_not_permitted
TEST(SharedLockTest, TryLockThrowEPERM)
{
  {
    yamc::shared_lock<MockSharedMutex> lk;
    ASSERT_THROW(lk.try_lock(), std::system_error);
  }
  {
    yamc::shared_lock<MockSharedMutex> lk;
    ASSERT_THORW_SYSTEM_ERROR(std::errc::operation_not_permitted, {
      lk.try_lock();
    });
  }
}

// try_lock() throw exception/resource_deadlock_would_occur
TEST(SharedLockTest, TryLockThrowEDEADLK)
{
  {
    MockSharedMutex mtx;
    yamc::shared_lock<MockSharedMutex> lk(mtx);
    ASSERT_THROW(lk.try_lock(), std::system_error);
  }
  {
    MockSharedMutex mtx;
    yamc::shared_lock<MockSharedMutex> lk(mtx);
    ASSERT_THORW_SYSTEM_ERROR(std::errc::resource_deadlock_would_occur, {
      lk.try_lock();
    });
  }
}

// unlock()
TEST(SharedLockTest, Unlock)
{
  MockSharedMutex mtx;
  yamc::shared_lock<MockSharedMutex> lk(mtx);
  ASSERT_NO_THROW(lk.unlock());
  ASSERT_FALSE(lk.owns_lock());
}

// unlock() throw system_error/operation_not_permitted
TEST(SharedLockTest, UnlockThrowEPERM)
{
  {
    MockSharedMutex mtx;
    yamc::shared_lock<MockSharedMutex> lk(mtx, std::defer_lock);
    ASSERT_THROW(lk.unlock(), std::system_error);
  }
  {
    MockSharedMutex mtx;
    yamc::shared_lock<MockSharedMutex> lk(mtx, std::defer_lock);
    ASSERT_THORW_SYSTEM_ERROR(std::errc::operation_not_permitted, {
      lk.unlock();
    });
  }
}

// void swap(shared_lock&) noexcept
TEST(SharedLockTest, Swap)
{
  MockSharedMutex mtx1, mtx2;
  yamc::shared_lock<MockSharedMutex> lk1(mtx1);                   // {&mtx1, true}
  yamc::shared_lock<MockSharedMutex> lk2(mtx2, std::defer_lock);  // {&mtx2, false}
  lk1.swap(lk2);
  ASSERT_EQ(&mtx2, lk1.mutex());
  ASSERT_FALSE(lk1.owns_lock());
  ASSERT_EQ(&mtx1, lk2.mutex());
  ASSERT_TRUE(lk2.owns_lock());
  ASSERT_TRUE(noexcept(lk1.swap(lk2)));
}

// void swap(shared_lock&, shared_lock&) noexcept
TEST(SharedLockTest, SwapNonMember)
{
  MockSharedMutex mtx1, mtx2;
  yamc::shared_lock<MockSharedMutex> lk1(mtx1);                   // {&mtx1, true}
  yamc::shared_lock<MockSharedMutex> lk2(mtx2, std::defer_lock);  // {&mtx2, false}
  std::swap(lk1, lk2);
  ASSERT_EQ(&mtx2, lk1.mutex());
  ASSERT_FALSE(lk1.owns_lock());
  ASSERT_EQ(&mtx1, lk2.mutex());
  ASSERT_TRUE(lk2.owns_lock());
  ASSERT_TRUE(noexcept(std::swap(lk1, lk2)));
}

// mutex_type* release() noexcept
TEST(SharedLockTest, Release)
{
  MockSharedMutex mtx;
  yamc::shared_lock<MockSharedMutex> lk(mtx);
  ASSERT_EQ(&mtx, lk.release());
  ASSERT_EQ(nullptr, lk.mutex());
  ASSERT_FALSE(lk.owns_lock());
  ASSERT_TRUE(noexcept(lk.release()));
}

// bool owns_lock() const noexcept
TEST(SharedLockTest, OwnsLock)
{
  MockSharedMutex mtx;
  const yamc::shared_lock<MockSharedMutex> lk(mtx);
  ASSERT_TRUE(lk.owns_lock());
  ASSERT_TRUE(noexcept(lk.owns_lock()));
}

// explicit operator bool () const noexcept
TEST(SharedLockTest, OperatorBool)
{
  {
    MockSharedMutex mtx;
    const yamc::shared_lock<MockSharedMutex> lk(mtx);
    if (lk) {  // shall be true
      SUCCEED();
    } else {
      FAIL();
    }
  }
  {
    MockSharedMutex mtx;
    const yamc::shared_lock<MockSharedMutex> lk(mtx, std::defer_lock);
    if (lk) {  // shall be false
      FAIL();
    } else {
      SUCCEED();
    }
  }
}

// mutex_type* mutex() const noexcept
TEST(SharedLockTest, Mutex)
{
  MockSharedMutex mtx;
  const yamc::shared_lock<MockSharedMutex> lk(mtx);
  ASSERT_EQ(&mtx, lk.mutex());
  ASSERT_TRUE(noexcept(lk.mutex()));
}
