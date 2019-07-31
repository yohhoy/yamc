/*
 * semaphore_test.cpp
 */
#include <atomic>
#include "gtest/gtest.h"
#include "yamc_semaphore.hpp"
#include "yamc_testutil.hpp"


#define TEST_THREADS   8
#define TEST_ITERATION 10000u

#define TEST_NOT_TIMEOUT    std::chrono::minutes(3)
#define TEST_EXPECT_TIMEOUT std::chrono::milliseconds(300)

#if 0
namespace {
std::mutex g_guard;
#define TRACE(msg_) \
  (std::unique_lock<std::mutex>(g_guard),\
   std::cout << std::this_thread::get_id() << ':' << (msg_) << std::endl)
}
#else
#define TRACE(msg_)
#endif

#define TEST_TICKS std::chrono::milliseconds(200)
#define WAIT_TICKS std::this_thread::sleep_for(TEST_TICKS)

#define EXPECT_STEP(n_) \
  { TRACE("STEP"#n_); EXPECT_EQ(n_, ++step); std::this_thread::sleep_for(TEST_TICKS); }
#define EXPECT_STEP_RANGE(r0_, r1_) \
  { TRACE("STEP"#r0_"-"#r1_); int s = ++step; EXPECT_TRUE(r0_ <= s && s <= r1_); WAIT_TICKS; }


// semaphore construction with zero
TEST(SemaphoreTest, CtorZero)
{
  constexpr ptrdiff_t LEAST_MAX_VALUE = 1000;
  EXPECT_NO_THROW(yamc::counting_semaphore<LEAST_MAX_VALUE>{0});
}

// semaphore constructor with maximum value
TEST(SemaphoreTest, CtorMaxValue)
{
  constexpr ptrdiff_t LEAST_MAX_VALUE = 1000;
  EXPECT_NO_THROW(yamc::counting_semaphore<LEAST_MAX_VALUE>{LEAST_MAX_VALUE});
}

// semaphore::acquire()
TEST(SemaphoreTest, Acquire)
{
  yamc::counting_semaphore<> sem(1);
  EXPECT_NO_THROW(sem.acquire());
  SUCCEED();
}

// semaphore::try_acquire()
TEST(SemaphoreTest, TryAcquire)
{
  yamc::counting_semaphore<> sem(1);
  EXPECT_TRUE(sem.try_acquire());
}

// semaphore::try_acquire() failure
TEST(SemaphoreTest, TryAcquireFail)
{
  yamc::counting_semaphore<> sem(0);
  EXPECT_FALSE(sem.try_acquire());
}

// semaphore::try_acquire_for()
TEST(SemaphoreTest, TryAcquireFor)
{
  yamc::counting_semaphore<> sem(1);
  EXPECT_TRUE(sem.try_acquire_for(TEST_NOT_TIMEOUT));
}

// semaphore::try_acquire_until()
TEST(SemaphoreTest, TryAcquireUntil)
{
  yamc::counting_semaphore<> sem(1);
  EXPECT_TRUE(sem.try_acquire_until(std::chrono::system_clock::now() + TEST_NOT_TIMEOUT));
}

// semaphore::try_acquire_for() timeout
TEST(SemaphoreTest, TryAcquireForTimeout)
{
  yamc::counting_semaphore<> sem(0);
  yamc::test::stopwatch<std::chrono::steady_clock> sw;
  EXPECT_FALSE(sem.try_acquire_for(TEST_EXPECT_TIMEOUT));
  EXPECT_LE(TEST_EXPECT_TIMEOUT, sw.elapsed());
}

// semaphore::try_acquire_until() timeout
TEST(SemaphoreTest, TryAcquireUntilTimeout)
{
  yamc::counting_semaphore<> sem(0);
  yamc::test::stopwatch<> sw;
  EXPECT_FALSE(sem.try_acquire_until(std::chrono::system_clock::now() + TEST_EXPECT_TIMEOUT));
  EXPECT_LE(TEST_EXPECT_TIMEOUT, sw.elapsed());
}

// semaphore::release()
TEST(SemaphoreTest, Release)
{
  std::atomic<int> step = {};
  yamc::counting_semaphore<> sem(0);
  yamc::test::join_thread thd([&]{
    EXPECT_STEP(1);
    EXPECT_NO_THROW(sem.release());
  });
  // wait-thread
  {
    EXPECT_NO_THROW(sem.acquire());
    EXPECT_STEP(2);
  }
}

// semaphore::release(update)
TEST(SemaphoreTest, ReleaseUpdate)
{
  std::atomic<int> step = {};
  yamc::counting_semaphore<> sem(0);
  yamc::test::task_runner(
    4,
	[&](std::size_t id) {
	  if (id == 0) {
        // signal-thread
		EXPECT_STEP(1);
		EXPECT_NO_THROW(sem.release(3));
	  } else {
        // 3 wait-threads
        EXPECT_NO_THROW(sem.acquire());
        EXPECT_STEP_RANGE(2, 4);
	  }
	}
  );
}

// use semaphore as Mutex
TEST(SemaphoreTest, UseAsMutex)
{
  yamc::binary_semaphore sem(1);
  std::size_t counter = 0;
  yamc::test::task_runner(
    TEST_THREADS,
    [&](std::size_t /*id*/) {
      for (std::size_t n = 0; n < TEST_ITERATION; ++n) {
        EXPECT_NO_THROW(sem.acquire());
        counter = counter + 1;
        std::this_thread::yield();  // provoke lock contention
        EXPECT_NO_THROW(sem.release());
      }
    });
  EXPECT_EQ(TEST_ITERATION * TEST_THREADS, counter);
}


// counting_semaphore::max()
TEST(LeastMaxValueTest, CounitingSemaphoreDefault)
{
  EXPECT_GE(yamc::counting_semaphore<>::max(), YAMC_SEMAPHORE_LEAST_MAX_VALUE);
}

// counting_semaphore::max() with least_max_value
TEST(LeastMaxValueTest, CounitingSemaphore)
{
  constexpr ptrdiff_t LEAST_MAX_VALUE = 1000;
  EXPECT_GE(yamc::counting_semaphore<LEAST_MAX_VALUE>::max(), LEAST_MAX_VALUE);
  // counting_semaphre<N>::max() may return value greater than N.
}

// binary_semaphore::max
TEST(LeastMaxValueTest, BinarySemaphore)
{
  EXPECT_GE(yamc::binary_semaphore::max(), 1);
  // counting_semaphre<N>::max() may return value greater than N.
}
