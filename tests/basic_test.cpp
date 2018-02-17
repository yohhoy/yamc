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
#include "yamc_testutil.hpp"


#define TEST_THREADS   8
#define TEST_ITERATION 10000u

#define TEST_NOT_TIMEOUT    std::chrono::minutes(3)
#define TEST_EXPECT_TIMEOUT std::chrono::milliseconds(300)


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
