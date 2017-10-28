/*
 * spinlock_test.cpp
 */
#include <atomic>
#include <mutex>
#include <type_traits>
#include "gtest/gtest.h"
#include "naive_spin_mutex.hpp"
#include "ttas_spin_mutex.hpp"
#include "yamc_testutil.hpp"


#define TEST_THREADS   20
#define TEST_ITERATION 100000u


using SpinMutexTypes = ::testing::Types<
  yamc::spin::basic_mutex<yamc::backoff::exponential<>>,
  yamc::spin_weak::basic_mutex<yamc::backoff::exponential<>>,
  yamc::spin_ttas::basic_mutex<yamc::backoff::exponential<>>,
  yamc::spin::basic_mutex<yamc::backoff::yield>,
  yamc::spin_weak::basic_mutex<yamc::backoff::yield>,
  yamc::spin_ttas::basic_mutex<yamc::backoff::yield>,
  yamc::spin::basic_mutex<yamc::backoff::busy>,
  yamc::spin_weak::basic_mutex<yamc::backoff::busy>,
  yamc::spin_ttas::basic_mutex<yamc::backoff::busy>
>;

template <typename Mutex>
struct SpinMutexTest : ::testing::Test {};

TYPED_TEST_CASE(SpinMutexTest, SpinMutexTypes);

// mutex::lock()
TYPED_TEST(SpinMutexTest, BasicLock)
{
  TypeParam mtx;
  std::size_t counter = 0;
  yamc::test::task_runner(
    TEST_THREADS,
    [&](std::size_t /*id*/) {
      for (std::size_t n = 0; n < TEST_ITERATION; ++n) {
        std::lock_guard<TypeParam> lk(mtx);
        counter = counter + 1;
      }
    });
  EXPECT_EQ(TEST_ITERATION * TEST_THREADS, counter);
}

// mutex::try_lock()
TYPED_TEST(SpinMutexTest, TryLock)
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
      }
    });
  EXPECT_EQ(TEST_ITERATION * TEST_THREADS, counter);
}

// mutex::try_lock() failure
TYPED_TEST(SpinMutexTest, TryLockFail)
{
  yamc::test::barrier step(2);
  TypeParam mtx;
  yamc::test::join_thread thd([&]{
    EXPECT_NO_THROW(mtx.lock());
    step.await();  // b1
    step.await();  // b2
    EXPECT_NO_THROW(mtx.unlock());
  });
  {
    step.await();  // b1
    EXPECT_FALSE(mtx.try_lock());
    step.await();  // b2
  }
}


// lockfree property of atomic<int>
TEST(AtomicTest, LockfreeInt)
{
  // std::atomic<int> type is always lock-free
  EXPECT_EQ(2, ATOMIC_INT_LOCK_FREE);
}

// YAMC_BACKOFF_* macros
TEST(BackoffTest, Macro)
{
  bool yamc_backoff_spin_default = std::is_same<YAMC_BACKOFF_SPIN_DEFAULT, yamc::backoff::exponential<>>::value;
  EXPECT_TRUE(yamc_backoff_spin_default);
  EXPECT_EQ(4000, YAMC_BACKOFF_EXPONENTIAL_INITCOUNT);
}

// backoff::exponential<100>
TEST(BackoffTest, Exponential100)
{
  using BackoffPolicy = yamc::backoff::exponential<100>;
  BackoffPolicy::state state;
  EXPECT_EQ(100u, state.initcount);
  EXPECT_EQ(100u, state.counter);
  for (int i = 0; i < 100; ++i) {
    BackoffPolicy::wait(state);  // wait 100
  }
  EXPECT_EQ(0u, state.counter);
  for (int i = 0; i < 2000; ++i) {
    BackoffPolicy::wait(state);
  }
  EXPECT_EQ(1u, state.initcount);
  EXPECT_EQ(0u, state.counter);
  BackoffPolicy::wait(state);
  EXPECT_EQ(1u, state.initcount);
  EXPECT_EQ(0u, state.counter);
}

// backoff::exponential<1>
TEST(BackoffTest, Exponential1)
{
  using BackoffPolicy = yamc::backoff::exponential<1>;
  BackoffPolicy::state state;
  EXPECT_EQ(1u, state.initcount);
  EXPECT_EQ(1u, state.counter);
  BackoffPolicy::wait(state);
  EXPECT_EQ(1u, state.initcount);
  EXPECT_EQ(0u, state.counter);
}

// backoff::yield
TEST(BackoffTest, Yield)
{
  using BackoffPolicy = yamc::backoff::yield;
  // NOTE: backoff::yield class has no observable behavior nor state
  BackoffPolicy::state state;
  BackoffPolicy::wait(state);
}

// backoff::busy
TEST(BackoffTest, Busy)
{
  using BackoffPolicy = yamc::backoff::busy;
  // NOTE: backoff::busy class has no observable behavior nor state
  BackoffPolicy::state state;
  BackoffPolicy::wait(state);
}
