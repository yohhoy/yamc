/*
 * semaphore_test.cpp
 */
#include "gtest/gtest.h"
#include "yamc_semaphore.hpp"
#include "yamc_testutil.hpp"
#if defined(__APPLE__)
#include "gcd_semaphore.hpp"
#define ENABLE_GCD_SEMAPHORE
#endif
#if defined(__linux__) && !defined(__APPLE__)
#include "posix_semaphore.hpp"
#define ENABLE_POSIX_SEMAPHORE
#endif
#if defined(_WIN32)
#include "win_semaphore.hpp"
#define ENABLE_WIN_SEMAPHORE
#endif


#define TEST_THREADS   8
#define TEST_ITERATION 10000u

#define TEST_NOT_TIMEOUT    std::chrono::minutes(3)
#define TEST_EXPECT_TIMEOUT std::chrono::milliseconds(300)


// selector for generic semaphore implementation (platform independent)
struct GenericSemaphore {
  template <std::ptrdiff_t least_max_value>
  using counting_semaphore = yamc::counting_semaphore<least_max_value>;
  using counting_semaphore_def = yamc::counting_semaphore<>;
  using binary_semaphore = yamc::binary_semaphore;
};

#if defined(ENABLE_GCD_SEMAPHORE)
// selector for GCD dispatch semaphore implementation
struct GcdSemaphore {
  template <std::ptrdiff_t least_max_value>
  using counting_semaphore = yamc::gcd::counting_semaphore<least_max_value>;
  using counting_semaphore_def = yamc::gcd::counting_semaphore<>;
  using binary_semaphore = yamc::gcd::binary_semaphore;
};
#endif

#if defined(ENABLE_POSIX_SEMAPHORE)
// selector for POSIX semaphore implementation
struct PosixSemaphore {
  template <std::ptrdiff_t least_max_value>
  using counting_semaphore = yamc::posix::counting_semaphore<least_max_value>;
  using counting_semaphore_def = yamc::posix::counting_semaphore<>;
  using binary_semaphore = yamc::posix::binary_semaphore;
};
#endif

#if defined(ENABLE_WIN_SEMAPHORE)
// selector for Windows semaphore implementation
struct WinSemaphore {
  template <std::ptrdiff_t least_max_value>
  using counting_semaphore = yamc::win::counting_semaphore<least_max_value>;
  using counting_semaphore_def = yamc::win::counting_semaphore<>;
  using binary_semaphore = yamc::win::binary_semaphore;
};
#endif

using SemaphoreSelector = ::testing::Types<
  GenericSemaphore
#if defined(ENABLE_GCD_SEMAPHORE)
  , GcdSemaphore
#endif
#if defined(ENABLE_POSIX_SEMAPHORE)
  , PosixSemaphore
#endif
#if defined(ENABLE_WIN_SEMAPHORE)
  , WinSemaphore
#endif
>;


template <typename Selector>
struct SemaphoreTest : ::testing::Test {};

TYPED_TEST_SUITE(SemaphoreTest, SemaphoreSelector);

// semaphore construction with zero
TYPED_TEST(SemaphoreTest, CtorZero)
{
  using counting_semaphore = typename TypeParam::template counting_semaphore<1>;
  EXPECT_NO_THROW(counting_semaphore{0});
}

// semaphore constructor with maximum value
TYPED_TEST(SemaphoreTest, CtorMaxValue)
{
  constexpr ptrdiff_t LEAST_MAX_VALUE = 1000;
  using counting_semaphore = typename TypeParam::template counting_semaphore<LEAST_MAX_VALUE>;
  EXPECT_NO_THROW(counting_semaphore{(counting_semaphore::max)()});
}

// semaphore::acquire()
TYPED_TEST(SemaphoreTest, Acquire)
{
  using counting_semaphore = typename TypeParam::counting_semaphore_def;
  counting_semaphore sem{1};
  EXPECT_NO_THROW(sem.acquire());
}

// semaphore::try_acquire()
TYPED_TEST(SemaphoreTest, TryAcquire)
{
  using counting_semaphore = typename TypeParam::counting_semaphore_def;
  counting_semaphore sem{1};
  EXPECT_TRUE(sem.try_acquire());
}

// semaphore::try_acquire() failure
TYPED_TEST(SemaphoreTest, TryAcquireFail)
{
  using counting_semaphore = typename TypeParam::counting_semaphore_def;
  counting_semaphore sem{0};
  EXPECT_FALSE(sem.try_acquire());
}

// semaphore::try_acquire_for()
TYPED_TEST(SemaphoreTest, TryAcquireFor)
{
  using counting_semaphore = typename TypeParam::counting_semaphore_def;
  counting_semaphore sem{1};
  EXPECT_TRUE(sem.try_acquire_for(TEST_NOT_TIMEOUT));
}

// semaphore::try_acquire_until()
TYPED_TEST(SemaphoreTest, TryAcquireUntil)
{
  using counting_semaphore = typename TypeParam::counting_semaphore_def;
  counting_semaphore sem{1};
  EXPECT_TRUE(sem.try_acquire_until(std::chrono::system_clock::now() + TEST_NOT_TIMEOUT));
}

// semaphore::try_acquire_for() timeout
TYPED_TEST(SemaphoreTest, TryAcquireForTimeout)
{
  using counting_semaphore = typename TypeParam::counting_semaphore_def;
  counting_semaphore sem{0};
  yamc::test::stopwatch<std::chrono::steady_clock> sw;
  EXPECT_FALSE(sem.try_acquire_for(TEST_EXPECT_TIMEOUT));
  EXPECT_LE(TEST_EXPECT_TIMEOUT, sw.elapsed());
}

// semaphore::try_acquire_until() timeout
TYPED_TEST(SemaphoreTest, TryAcquireUntilTimeout)
{
  using counting_semaphore = typename TypeParam::counting_semaphore_def;
  counting_semaphore sem{0};
  yamc::test::stopwatch<> sw;
  EXPECT_FALSE(sem.try_acquire_until(std::chrono::system_clock::now() + TEST_EXPECT_TIMEOUT));
  EXPECT_LE(TEST_EXPECT_TIMEOUT, sw.elapsed());
}

// semaphore::try_acquire_until() timeout with steady_clock
TYPED_TEST(SemaphoreTest, TryAcquireUntilTimeoutSteadyClock)
{
  using counting_semaphore = typename TypeParam::counting_semaphore_def;
  counting_semaphore sem{0};
  yamc::test::stopwatch<std::chrono::steady_clock> sw;
  EXPECT_FALSE(sem.try_acquire_until(std::chrono::steady_clock::now() + TEST_EXPECT_TIMEOUT));
  EXPECT_LE(TEST_EXPECT_TIMEOUT, sw.elapsed());
}

// semaphore::try_acquire_until() timeout with high_resolution_clock
TYPED_TEST(SemaphoreTest, TryAcquireUntilTimeoutHighResolutionClock)
{
  using counting_semaphore = typename TypeParam::counting_semaphore_def;
  counting_semaphore sem{0};
  yamc::test::stopwatch<std::chrono::high_resolution_clock> sw;
  EXPECT_FALSE(sem.try_acquire_until(std::chrono::high_resolution_clock::now() + TEST_EXPECT_TIMEOUT));
  EXPECT_LE(TEST_EXPECT_TIMEOUT, sw.elapsed());
}

// semaphore::release()
TYPED_TEST(SemaphoreTest, Release)
{
  SETUP_STEPTEST;
  using counting_semaphore = typename TypeParam::counting_semaphore_def;
  counting_semaphore sem{0};
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
TYPED_TEST(SemaphoreTest, ReleaseUpdate)
{
  SETUP_STEPTEST;
  using counting_semaphore = typename TypeParam::counting_semaphore_def;
  counting_semaphore sem{0};
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
TYPED_TEST(SemaphoreTest, UseAsMutex)
{
  using binary_semaphore = typename TypeParam::binary_semaphore;
  binary_semaphore sem{1};
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


template <typename Selector>
struct LeastMaxValueTest : ::testing::Test {};

TYPED_TEST_SUITE(LeastMaxValueTest, SemaphoreSelector);

// counting_semaphore::max() with least_max_value
TYPED_TEST(LeastMaxValueTest, CounitingSemaphore)
{
  constexpr ptrdiff_t LEAST_MAX_VALUE = 1000;
  using counting_semaphore = typename TypeParam::template counting_semaphore<LEAST_MAX_VALUE>;
  EXPECT_GE((counting_semaphore::max)(), LEAST_MAX_VALUE);
  // counting_semaphore<N>::max() may return value greater than N.
}

// binary_semaphore::max()
TYPED_TEST(LeastMaxValueTest, BinarySemaphore)
{
  using binary_semaphore = typename TypeParam::binary_semaphore;
  EXPECT_GE((binary_semaphore::max)(), 1);
  // counting_semaphore<N>::max() may return value greater than N.
}
