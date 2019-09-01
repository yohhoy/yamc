/*
 * basic_test.cpp
 */
#include <type_traits>
#include "gtest/gtest.h"
#include "checked_mutex.hpp"
#include "checked_shared_mutex.hpp"
#include "fair_mutex.hpp"
#include "fair_shared_mutex.hpp"
#include "alternate_mutex.hpp"
#include "alternate_shared_mutex.hpp"
#if defined(__linux__) || defined(__APPLE__)
#include "posix_native_mutex.hpp"
#define ENABLE_POSIX_NATIVE_MUTEX
#endif
#if defined(_WIN32)
#include "win_native_mutex.hpp"
#define ENABLE_WIN_NATIVE_MUTEX
#endif
#if defined(__APPLE__)
#include "apple_native_mutex.hpp"
#define ENABLE_APPLE_NATIVE_MUTEX
#endif
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
#if defined(ENABLE_POSIX_NATIVE_MUTEX)
  , yamc::posix::mutex
  , yamc::posix::shared_mutex
#if YAMC_POSIX_TIMEOUT_SUPPORTED
  , yamc::posix::timed_mutex
#endif
#endif
#if defined(ENABLE_WIN_NATIVE_MUTEX)
  , yamc::win::mutex
  , yamc::win::timed_mutex
  , yamc::win::shared_mutex
#endif
#if defined(ENABLE_APPLE_NATIVE_MUTEX)
  , yamc::apple::unfair_lock
#endif
>;

template <typename Mutex>
struct NormalMutexTest : ::testing::Test {};

TYPED_TEST_SUITE(NormalMutexTest, NormalMutexTypes);

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
#if defined(ENABLE_POSIX_NATIVE_MUTEX)
  , yamc::posix::recursive_mutex
#if YAMC_POSIX_TIMEOUT_SUPPORTED
  , yamc::posix::recursive_timed_mutex
#endif
#endif
#if defined(ENABLE_WIN_NATIVE_MUTEX)
  , yamc::win::recursive_mutex
  , yamc::win::recursive_timed_mutex
#endif
>;

template <typename Mutex>
struct RecursiveMutexTest : ::testing::Test {};

TYPED_TEST_SUITE(RecursiveMutexTest, RecursiveMutexTypes);

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
#if defined(ENABLE_POSIX_NATIVE_MUTEX)
#if YAMC_POSIX_TIMEOUT_SUPPORTED
  , yamc::posix::timed_mutex
  , yamc::posix::recursive_timed_mutex
#endif
#if YAMC_POSIX_TIMEOUT_SUPPORTED
  , yamc::posix::shared_timed_mutex
#endif
#endif
#if defined(ENABLE_WIN_NATIVE_MUTEX)
  , yamc::win::timed_mutex
  , yamc::win::recursive_timed_mutex
#endif
>;

template <typename Mutex>
struct TimedMutexTest : ::testing::Test {};

TYPED_TEST_SUITE(TimedMutexTest, TimedMutexTypes);

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
    yamc::test::stopwatch<std::chrono::steady_clock> sw;
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
#if defined(ENABLE_POSIX_NATIVE_MUTEX) && YAMC_POSIX_TIMEOUT_SUPPORTED
  , yamc::posix::recursive_timed_mutex
#endif
#if defined(ENABLE_WIN_NATIVE_MUTEX)
  , yamc::win::recursive_timed_mutex
#endif
>;

template <typename Mutex>
struct RecursiveTimedMutexTest : ::testing::Test {};

TYPED_TEST_SUITE(RecursiveTimedMutexTest, RecursiveTimedMutexTypes);

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


#if defined(ENABLE_POSIX_NATIVE_MUTEX)
// posix::native_mutex::native_handle_type
TEST(NativeMutexTest, NativeHandleType)
{
  ::testing::StaticAssertTypeEq<yamc::posix::native_mutex::native_handle_type, ::pthread_mutex_t*>();
}

// posix::native_mutex::native_handle()
TEST(NativeMutexTest, NativeHandle)
{
  yamc::posix::native_mutex mtx;
  yamc::posix::native_mutex::native_handle_type handle = mtx.native_handle();
  (void)handle;  // suppress "unused variable" warning
}

// posix::native_recursive_mutex::native_handle_type
TEST(NativeRecursiveMutexTest, NativeHandleType)
{
  ::testing::StaticAssertTypeEq<yamc::posix::native_recursive_mutex::native_handle_type, ::pthread_mutex_t*>();
}

// posix::native_recursive_mutex::native_handle()
TEST(NativeRecursiveMutexTest, NativeHandle)
{
  yamc::posix::native_recursive_mutex mtx;
  yamc::posix::native_recursive_mutex::native_handle_type handle = mtx.native_handle();
  (void)handle;  // suppress "unused variable" warning
}

// posix::rwlock::native_handle_type
TEST(PosixRWLockTest, NativeHandleType)
{
  ::testing::StaticAssertTypeEq<yamc::posix::rwlock::native_handle_type, ::pthread_rwlock_t*>();
}

// posix::rwlock::native_handle()
TEST(PosixRWLockTest, NativeHandle)
{
  yamc::posix::rwlock mtx;
  yamc::posix::rwlock::native_handle_type handle = mtx.native_handle();
  (void)handle;  // suppress "unused variable" warning
}

#if YAMC_POSIX_SPINLOCK_SUPPORTED
// posix::spinlock::native_handle_type
TEST(PosixSpinlockTest, NativeHandleType)
{
  ::testing::StaticAssertTypeEq<yamc::posix::spinlock::native_handle_type, ::pthread_spinlock_t*>();
}

// posix::spinlock::native_handle()
TEST(PosixSpinlockTest, NativeHandle)
{
  yamc::posix::spinlock mtx;
  yamc::posix::spinlock::native_handle_type handle = mtx.native_handle();
  (void)handle;  // suppress "unused variable" warning
}
#endif // YAMC_POSIX_SPINLOCK_SUPPORTED
#endif // defined(ENABLE_POSIX_NATIVE_MUTEX)


#if defined(ENABLE_WIN_NATIVE_MUTEX)
// win::native_mutex::native_handle_type
TEST(NativeMutexTest, NativeHandleType)
{
  ::testing::StaticAssertTypeEq<yamc::win::native_mutex::native_handle_type, ::HANDLE>();
}

// win::native_mutex::native_handle()
TEST(NativeMutexTest, NativeHandle)
{
  yamc::win::native_mutex mtx;
  yamc::win::native_mutex::native_handle_type handle = mtx.native_handle();
  (void)handle;  // suppress "unused variable" warning
}

// win::critical_section::native_handle_type
TEST(CriticalSectionTest, NativeHandleType)
{
  ::testing::StaticAssertTypeEq<yamc::win::critical_section::native_handle_type, ::CRITICAL_SECTION*>();
}

// win::critical_section::native_handle()
TEST(CriticalSectionTest, NativeHandle)
{
  yamc::win::critical_section mtx;
  yamc::win::critical_section::native_handle_type handle = mtx.native_handle();
  (void)handle;  // suppress "unused variable" warning
}

// win::slim_rwlock::native_handle_type
TEST(SlimRWLockTest, NativeHandleType)
{
  ::testing::StaticAssertTypeEq<yamc::win::slim_rwlock::native_handle_type, ::SRWLOCK*>();
}

// win::slim_rwlock::native_handle()
TEST(SlimRWLockTest, NativeHandle)
{
  yamc::win::slim_rwlock mtx;
  yamc::win::slim_rwlock::native_handle_type handle = mtx.native_handle();
  (void)handle;  // suppress "unused variable" warning
}
#endif // defined(ENABLE_WIN_NATIVE_MUTEX)


#if defined(ENABLE_APPLE_NATIVE_MUTEX)
// apple::unfair_lock::native_handle_type
TEST(UnfairLockTest, NativeHandleType)
{
  ::testing::StaticAssertTypeEq<yamc::apple::unfair_lock::native_handle_type, ::os_unfair_lock_t>();
}

// apple::unfair_lock:native_handle()
TEST(UnfairLockTest, NativeHandle)
{
  yamc::apple::unfair_lock mtx;
  yamc::apple::unfair_lock::native_handle_type handle = mtx.native_handle();
  (void)handle;  // suppress "unused variable" warning
}
#endif // defined(ENABLE_APPLE_NATIVE_MUTEX)
