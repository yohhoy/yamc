/*
 * basic_test.cpp
 */
#include <chrono>
#include <mutex>
#include <type_traits>
#include "gtest/gtest.h"
#include "checked_mutex.hpp"
#include "checked_shared_mutex.hpp"
#include "fair_mutex.hpp"
#include "fair_shared_mutex.hpp"
#include "alternate_mutex.hpp"
#include "alternate_shared_mutex.hpp"
#include "yamc_shared_lock.hpp"
#include "yamc_testutil.hpp"


#define TEST_THREADS   8
#define TEST_ITERATION 10000u

#define TEST_NOT_TIMEOUT    std::chrono::minutes(3)
#define TEST_EXPECT_TIMEOUT std::chrono::milliseconds(300)


#define EXPECT_THORW_SYSTEM_ERROR(errorcode_, block_) \
  try { \
    block_ \
    FAIL(); \
  } catch (const std::system_error& e) { \
    EXPECT_EQ(std::make_error_code(errorcode_), e.code()); \
  }


using NormalMutexTypes = ::testing::Types<
  yamc::checked::mutex,
  yamc::checked::timed_mutex,
  yamc::fair::mutex,
  yamc::fair::timed_mutex,
  yamc::fair::shared_mutex,
  yamc::alternate::mutex,
  yamc::alternate::timed_mutex,
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
        std::lock_guard<TypeParam> lk(mtx);
        counter = counter + 1;
        std::this_thread::yield();  // provoke lock contention
      }
    });
  EXPECT_EQ(TEST_ITERATION * TEST_THREADS, counter);
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
        std::lock_guard<TypeParam> lk(mtx, std::adopt_lock);
        counter = counter + 1;
        std::this_thread::yield();  // provoke lock contention
      }
    });
  EXPECT_EQ(TEST_ITERATION * TEST_THREADS, counter);
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
    EXPECT_FALSE(mtx.try_lock());
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
        std::lock_guard<TypeParam> lk1(mtx);
        auto before_cnt = ++c1;
        {
          std::lock_guard<TypeParam> lk2(mtx);
          ++c2;
        }
        auto after_cnt = ++c3;
        ASSERT_TRUE(before_cnt == after_cnt);
      }
    });
  EXPECT_EQ(TEST_ITERATION * TEST_THREADS, c1);
  EXPECT_EQ(TEST_ITERATION * TEST_THREADS, c2);
  EXPECT_EQ(TEST_ITERATION * TEST_THREADS, c3);
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
        std::lock_guard<TypeParam> lk1(mtx, std::adopt_lock);
        ++c1;
        {
          ASSERT_TRUE(mtx.try_lock());
          std::lock_guard<TypeParam> lk2(mtx, std::adopt_lock);
          ++c2;
        }
        ++c3;
      }
    });
  EXPECT_EQ(TEST_ITERATION * TEST_THREADS, c1);
  EXPECT_EQ(TEST_ITERATION * TEST_THREADS, c2);
  EXPECT_EQ(TEST_ITERATION * TEST_THREADS, c3);
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
    EXPECT_FALSE(mtx.try_lock());  // lockcnt = 1
    step.await();  // b2
    step.await();  // b3
    EXPECT_FALSE(mtx.try_lock());  // lockcnt = 2
    step.await();  // b4
    step.await();  // b5
    EXPECT_FALSE(mtx.try_lock());  // lockcnt = 1
    step.await();  // b6
  }
}


using TimedMutexTypes = ::testing::Types<
  yamc::checked::timed_mutex,
  yamc::checked::recursive_timed_mutex,
  yamc::checked::shared_timed_mutex,
  yamc::fair::timed_mutex,
  yamc::fair::recursive_timed_mutex,
  yamc::fair::shared_timed_mutex,
  yamc::alternate::timed_mutex,
  yamc::alternate::recursive_timed_mutex,
  yamc::alternate::shared_timed_mutex
>;

template <typename Mutex>
struct TimedMutexTest : ::testing::Test {};

TYPED_TEST_CASE(TimedMutexTest, TimedMutexTypes);

// timed_mutex::try_lock_for()
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
        std::lock_guard<TypeParam> lk(mtx, std::adopt_lock);
        counter = counter + 1;
      }
    });
  EXPECT_EQ(TEST_ITERATION * TEST_THREADS, counter);
}

// timed_mutex::try_lock_until()
TYPED_TEST(TimedMutexTest, TryLockUntil)
{
  TypeParam mtx;
  std::size_t counter = 0;
  yamc::test::task_runner(
    TEST_THREADS,
    [&](std::size_t /*id*/) {
      for (std::size_t n = 0; n < TEST_ITERATION; ++n) {
        while (!mtx.try_lock_until(std::chrono::system_clock::now() + TEST_NOT_TIMEOUT)) {
          std::this_thread::yield();
        }
        std::lock_guard<TypeParam> lk(mtx, std::adopt_lock);
        counter = counter + 1;
      }
    });
  EXPECT_EQ(TEST_ITERATION * TEST_THREADS, counter);
}

// timed_mutex::try_lock_for() timeout
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
    EXPECT_FALSE(mtx.try_lock_for(TEST_EXPECT_TIMEOUT));
    EXPECT_LE(TEST_EXPECT_TIMEOUT, sw.elapsed());
    step.await();  // b2
  }
}

// timed_mutex::try_lock_until() timeout
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
    yamc::test::stopwatch<> sw;
    EXPECT_FALSE(mtx.try_lock_until(std::chrono::system_clock::now() + TEST_EXPECT_TIMEOUT));
    EXPECT_LE(TEST_EXPECT_TIMEOUT, sw.elapsed());
    step.await();  // b2
  }
}


using RecursiveTimedMutexTypes = ::testing::Types<
  yamc::checked::recursive_timed_mutex,
  yamc::fair::recursive_timed_mutex,
  yamc::alternate::recursive_timed_mutex
>;

template <typename Mutex>
struct RecursiveTimedMutexTest : ::testing::Test {};

TYPED_TEST_CASE(RecursiveTimedMutexTest, RecursiveTimedMutexTypes);

// recursive_timed_mutex::try_lock_for()
TYPED_TEST(RecursiveTimedMutexTest, TryLockFor)
{
  TypeParam mtx;
  std::size_t c1 = 0, c2 = 0, c3 = 0;
  yamc::test::task_runner(
    TEST_THREADS,
    [&](std::size_t /*id*/) {
      for (std::size_t n = 0; n < TEST_ITERATION; ++n) {
        while (!mtx.try_lock_for(TEST_NOT_TIMEOUT)) {
          std::this_thread::yield();
        }
        std::lock_guard<TypeParam> lk1(mtx, std::adopt_lock);
        ++c1;
        {
          ASSERT_TRUE(mtx.try_lock_for(TEST_NOT_TIMEOUT));
          std::lock_guard<TypeParam> lk2(mtx, std::adopt_lock);
          ++c2;
        }
        ++c3;
      }
    });
  EXPECT_EQ(TEST_ITERATION * TEST_THREADS, c1);
  EXPECT_EQ(TEST_ITERATION * TEST_THREADS, c2);
  EXPECT_EQ(TEST_ITERATION * TEST_THREADS, c3);
}

// recursive_timed_mutex::try_lock_until()
TYPED_TEST(RecursiveTimedMutexTest, TryLockUntil)
{
  TypeParam mtx;
  std::size_t c1 = 0, c2 = 0, c3 = 0;
  yamc::test::task_runner(
    TEST_THREADS,
    [&](std::size_t /*id*/) {
      for (std::size_t n = 0; n < TEST_ITERATION; ++n) {
        while (!mtx.try_lock_until(std::chrono::system_clock::now() + TEST_NOT_TIMEOUT)) {
          std::this_thread::yield();
        }
        std::lock_guard<TypeParam> lk1(mtx, std::adopt_lock);
        ++c1;
        {
          ASSERT_TRUE(mtx.try_lock_until(std::chrono::system_clock::now() + TEST_NOT_TIMEOUT));
          std::lock_guard<TypeParam> lk2(mtx, std::adopt_lock);
          ++c2;
        }
        ++c3;
      }
    });
  EXPECT_EQ(TEST_ITERATION * TEST_THREADS, c1);
  EXPECT_EQ(TEST_ITERATION * TEST_THREADS, c2);
  EXPECT_EQ(TEST_ITERATION * TEST_THREADS, c3);
}


using MockSharedMutex = yamc::mock::shared_mutex;
using MockSharedTimedMutex = yamc::mock::shared_timed_mutex;

// shared_lock::mutex_type
TEST(SharedLockTest, MutexType)
{
  bool shall_be_true = std::is_same<MockSharedMutex, yamc::shared_lock<MockSharedMutex>::mutex_type>::value;
  EXPECT_TRUE(shall_be_true);
}

// shared_lock() noexcept
TEST(SharedLockTest, CtorDefault)
{
  yamc::shared_lock<MockSharedMutex> lk;
  EXPECT_EQ(nullptr, lk.mutex());
  EXPECT_FALSE(lk.owns_lock());
  EXPECT_TRUE(noexcept(yamc::shared_lock<MockSharedMutex>{}));
}

// explicit shared_lock(mutex_type&)
TEST(SharedLockTest, CtorMutex)
{
  MockSharedMutex mtx;
  yamc::shared_lock<MockSharedMutex> lk(mtx);
  EXPECT_EQ(&mtx, lk.mutex());
  EXPECT_TRUE(lk.owns_lock());
}

// shared_lock(mutex_type&, defer_lock_t) noexcept
TEST(SharedLockTest, CtorDeferLock)
{
  MockSharedMutex mtx;
  yamc::shared_lock<MockSharedMutex> lk(mtx, std::defer_lock);
  EXPECT_EQ(&mtx, lk.mutex());
  EXPECT_FALSE(lk.owns_lock());
  EXPECT_TRUE(noexcept(yamc::shared_lock<MockSharedMutex>(mtx, std::defer_lock)));
}

// shared_lock(mutex_type&, try_to_lock_t)
TEST(SharedLockTest, CtorTryToLock)
{
  MockSharedMutex mtx;
  yamc::shared_lock<MockSharedMutex> lk(mtx, std::try_to_lock);
  EXPECT_EQ(&mtx, lk.mutex());
  EXPECT_TRUE(lk.owns_lock());
}

// shared_lock(mutex_type&, try_to_lock_t) failure
TEST(SharedLockTest, CtorTryToLockFail)
{
  MockSharedMutex mtx;
  mtx.retval_on_trylock = false;
  yamc::shared_lock<MockSharedMutex> lk(mtx, std::try_to_lock);
  EXPECT_EQ(&mtx, lk.mutex());
  EXPECT_FALSE(lk.owns_lock());
}

// shared_lock(mutex_type&, adopt_lock_t)
TEST(SharedLockTest, CtorAdoptLock)
{
  MockSharedMutex mtx;
  mtx.lock_shared();
  yamc::shared_lock<MockSharedMutex> lk(mtx, std::adopt_lock);
  EXPECT_EQ(&mtx, lk.mutex());
  EXPECT_TRUE(lk.owns_lock());
}

// shared_lock(mutex_type&, const chrono::time_point&)
TEST(SharedLockTest, CtorTimePoint)
{
  MockSharedTimedMutex mtx;
  yamc::shared_lock<MockSharedTimedMutex> lk(mtx, std::chrono::system_clock::now());
  EXPECT_EQ(&mtx, lk.mutex());
  EXPECT_TRUE(lk.owns_lock());
}

// shared_lock(mutex_type&, const chrono::time_point&) failure
TEST(SharedLockTest, CtorTimePointFail)
{
  MockSharedTimedMutex mtx;
  mtx.retval_on_trylock = false;
  yamc::shared_lock<MockSharedTimedMutex> lk(mtx, std::chrono::system_clock::now());
  EXPECT_EQ(&mtx, lk.mutex());
  EXPECT_FALSE(lk.owns_lock());
}

// shared_lock(mutex_type&, const chrono::duration&)
TEST(SharedLockTest, CtorRelTime)
{
  MockSharedTimedMutex mtx;
  yamc::shared_lock<MockSharedTimedMutex> lk(mtx, std::chrono::milliseconds(1));
  EXPECT_EQ(&mtx, lk.mutex());
  EXPECT_TRUE(lk.owns_lock());
}

// shared_lock(mutex_type&, const chrono::duration&) failure
TEST(SharedLockTest, CtorRelTimeFail)
{
  MockSharedTimedMutex mtx;
  mtx.retval_on_trylock = false;
  yamc::shared_lock<MockSharedTimedMutex> lk(mtx, std::chrono::milliseconds(1));
  EXPECT_EQ(&mtx, lk.mutex());
  EXPECT_FALSE(lk.owns_lock());
}

// shared_lock(shared_lock&&) noexcept
TEST(SharedLockTest, MoveCtor)
{
  MockSharedMutex mtx;
  yamc::shared_lock<MockSharedMutex> lk1(mtx);
  yamc::shared_lock<MockSharedMutex> lk2(std::move(lk1));  // move-constructor
  EXPECT_EQ(nullptr, lk1.mutex());
  EXPECT_FALSE(lk1.owns_lock());
  EXPECT_EQ(&mtx, lk2.mutex());
  EXPECT_TRUE(lk2.owns_lock());
  EXPECT_TRUE(noexcept(yamc::shared_lock<MockSharedMutex>(std::move(lk2))));
}

// shared_lock& operator=(shared_lock&&) noexcept
TEST(SharedLockTest, MoveAssign)
{
  MockSharedMutex mtx;
  yamc::shared_lock<MockSharedMutex> lk1(mtx);
  yamc::shared_lock<MockSharedMutex> lk2;
  lk2 = std::move(lk1);  // move-assignment
  EXPECT_EQ(nullptr, lk1.mutex());
  EXPECT_FALSE(lk1.owns_lock());
  EXPECT_EQ(&mtx, lk2.mutex());
  EXPECT_TRUE(lk2.owns_lock());
  EXPECT_TRUE(noexcept(lk1 = std::move(lk2)));
}

// lock()
TEST(SharedLockTest, Lock)
{
  MockSharedMutex mtx;
  yamc::shared_lock<MockSharedMutex> lk(mtx, std::defer_lock);
  EXPECT_NO_THROW(lk.lock());
  EXPECT_TRUE(lk.owns_lock());
}

// lock() throw exception/operation_not_permitted
TEST(SharedLockTest, LockThrowEPERM)
{
  {
    yamc::shared_lock<MockSharedMutex> lk;
    EXPECT_THROW(lk.lock(), std::system_error);
  }
  {
    yamc::shared_lock<MockSharedMutex> lk;
    EXPECT_THORW_SYSTEM_ERROR(std::errc::operation_not_permitted, {
      lk.lock();
    });
  }
}

// lock() throw exception/resource_deadlock_would_occur
TEST(SharedLockTest, LockThrowEDEADLK)
{
  {
    yamc::shared_lock<MockSharedMutex> lk;
    EXPECT_THROW(lk.lock(), std::system_error);
  }
  {
    MockSharedMutex mtx;
    yamc::shared_lock<MockSharedMutex> lk(mtx);
    EXPECT_THORW_SYSTEM_ERROR(std::errc::resource_deadlock_would_occur, {
      lk.lock();
    });
  }
}

// try_lock()
TEST(SharedLockTest, TryLock)
{
  MockSharedMutex mtx;
  yamc::shared_lock<MockSharedMutex> lk(mtx, std::defer_lock);
  EXPECT_TRUE(lk.try_lock());
  EXPECT_TRUE(lk.owns_lock());
}

// try_lock() failure
TEST(SharedLockTest, TryLockFail)
{
  MockSharedMutex mtx;
  mtx.retval_on_trylock = false;
  yamc::shared_lock<MockSharedMutex> lk(mtx, std::defer_lock);
  EXPECT_FALSE(lk.try_lock());
  EXPECT_FALSE(lk.owns_lock());
}

// try_lock() throw exception/operation_not_permitted
TEST(SharedLockTest, TryLockThrowEPERM)
{
  {
    yamc::shared_lock<MockSharedMutex> lk;
    EXPECT_THROW(lk.try_lock(), std::system_error);
  }
  {
    yamc::shared_lock<MockSharedMutex> lk;
    EXPECT_THORW_SYSTEM_ERROR(std::errc::operation_not_permitted, {
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
    EXPECT_THROW(lk.try_lock(), std::system_error);
  }
  {
    MockSharedMutex mtx;
    yamc::shared_lock<MockSharedMutex> lk(mtx);
    EXPECT_THORW_SYSTEM_ERROR(std::errc::resource_deadlock_would_occur, {
      lk.try_lock();
    });
  }
}

// unlock()
TEST(SharedLockTest, Unlock)
{
  MockSharedMutex mtx;
  yamc::shared_lock<MockSharedMutex> lk(mtx);
  EXPECT_NO_THROW(lk.unlock());
  EXPECT_FALSE(lk.owns_lock());
}

// unlock() throw system_error/operation_not_permitted
TEST(SharedLockTest, UnlockThrowEPERM)
{
  {
    MockSharedMutex mtx;
    yamc::shared_lock<MockSharedMutex> lk(mtx, std::defer_lock);
    EXPECT_THROW(lk.unlock(), std::system_error);
  }
  {
    MockSharedMutex mtx;
    yamc::shared_lock<MockSharedMutex> lk(mtx, std::defer_lock);
    EXPECT_THORW_SYSTEM_ERROR(std::errc::operation_not_permitted, {
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
  EXPECT_EQ(&mtx2, lk1.mutex());
  EXPECT_FALSE(lk1.owns_lock());
  EXPECT_EQ(&mtx1, lk2.mutex());
  EXPECT_TRUE(lk2.owns_lock());
  EXPECT_TRUE(noexcept(lk1.swap(lk2)));
}

// void swap(shared_lock&, shared_lock&) noexcept
TEST(SharedLockTest, SwapNonMember)
{
  MockSharedMutex mtx1, mtx2;
  yamc::shared_lock<MockSharedMutex> lk1(mtx1);                   // {&mtx1, true}
  yamc::shared_lock<MockSharedMutex> lk2(mtx2, std::defer_lock);  // {&mtx2, false}
  std::swap(lk1, lk2);
  EXPECT_EQ(&mtx2, lk1.mutex());
  EXPECT_FALSE(lk1.owns_lock());
  EXPECT_EQ(&mtx1, lk2.mutex());
  EXPECT_TRUE(lk2.owns_lock());
  EXPECT_TRUE(noexcept(std::swap(lk1, lk2)));
}

// mutex_type* release() noexcept
TEST(SharedLockTest, Release)
{
  MockSharedMutex mtx;
  yamc::shared_lock<MockSharedMutex> lk(mtx);
  EXPECT_EQ(&mtx, lk.release());
  EXPECT_EQ(nullptr, lk.mutex());
  EXPECT_FALSE(lk.owns_lock());
  EXPECT_TRUE(noexcept(lk.release()));
}

// bool owns_lock() const noexcept
TEST(SharedLockTest, OwnsLock)
{
  MockSharedMutex mtx;
  const yamc::shared_lock<MockSharedMutex> lk(mtx);
  EXPECT_TRUE(lk.owns_lock());
  EXPECT_TRUE(noexcept(lk.owns_lock()));
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
  EXPECT_EQ(&mtx, lk.mutex());
  EXPECT_TRUE(noexcept(lk.mutex()));
}
