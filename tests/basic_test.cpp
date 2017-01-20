/*
 * basic_test.cpp
 */
#include <mutex>
#include "gtest/gtest.h"
#include "naive_spin_mutex.hpp"
#include "ttas_spin_mutex.hpp"
#include "checked_mutex.hpp"
#include "fair_mutex.hpp"
#include "yamc_test.hpp"
#include "alternate_mutex.hpp"


#define TEST_THREADS   8
#define TEST_ITERATION 10000u


using NormalMutexTypes = ::testing::Types<
  yamc::spin::mutex,
  yamc::spin_weak::mutex,
  yamc::spin_ttas::mutex,
  yamc::checked::mutex,
  yamc::fair::mutex
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
  yamc::fair::recursive_mutex,
  yamc::alternate::recursive_mutex
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
