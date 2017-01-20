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


#define TEST_THREADS   8
#define TEST_ITERATION 100000u


using NormalMutexTypes = ::testing::Types<
  yamc::spin::mutex,
  yamc::spin_weak::mutex,
  yamc::spin_ttas::mutex,
  yamc::checked::mutex,
  yamc::checked::recursive_mutex,
  yamc::fair::mutex,
  yamc::fair::recursive_mutex
>;

template <typename Mutex>
struct NormalMutexTest : ::testing::Test {};

TYPED_TEST_CASE(NormalMutexTest, NormalMutexTypes);


// normal mutex usecase
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

using RecursiveMutexTypes = ::testing::Types<
  yamc::checked::recursive_mutex,
  yamc::fair::recursive_mutex
>;

template <typename Mutex>
struct RecursiveMutexTest : ::testing::Test {};

TYPED_TEST_CASE(RecursiveMutexTest, RecursiveMutexTypes);


// recursive mutex usecase
TYPED_TEST(RecursiveMutexTest, BasicLock)
{
  TypeParam mtx;
  std::size_t c1 = 0, c2 = 0, c3 = 0;
  yamc::test::task_runner(
    TEST_THREADS,
    [&](std::size_t /*id*/) {
      for (std::size_t n = 0; n < TEST_ITERATION; ++n) {
        std::lock_guard<decltype(mtx)> lk1(mtx);
        ++c1;
        {
          std::lock_guard<decltype(mtx)> lk2(mtx);
          ++c2;
        }
        ++c3;
      }
    });
  ASSERT_EQ(TEST_ITERATION * TEST_THREADS, c1);
  ASSERT_EQ(TEST_ITERATION * TEST_THREADS, c2);
  ASSERT_EQ(TEST_ITERATION * TEST_THREADS, c3);
}

