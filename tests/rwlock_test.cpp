/*
 * rwlock_test.cpp
 */
#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>
#include "gtest/gtest.h"
#include "alternate_shared_mutex.hpp"
#include "yamc_shared_lock.hpp"
#include "yamc_testutil.hpp"


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

#define TEST_READER_THREADS 4

#define TEST_EXPECT_TIMEOUT std::chrono::milliseconds(300)
#define TEST_NOT_TIMEOUT std::chrono::minutes(3)

#define TEST_TICKS std::chrono::milliseconds(100)
#define WAIT_TICKS std::this_thread::sleep_for(TEST_TICKS)


using SharedMutexTypes = ::testing::Types<
  yamc::alternate::basic_shared_mutex<yamc::rwlock::ReaderPrefer>,
  yamc::alternate::basic_shared_mutex<yamc::rwlock::WriterPrefer>,
  yamc::alternate::basic_shared_timed_mutex<yamc::rwlock::ReaderPrefer>,
  yamc::alternate::basic_shared_timed_mutex<yamc::rwlock::WriterPrefer>
>;

template <typename Mutex>
struct SharedMutexTest : ::testing::Test {};

TYPED_TEST_CASE(SharedMutexTest, SharedMutexTypes);

// shared_mutex::lock()
TYPED_TEST(SharedMutexTest, Lock)
{
  yamc::test::phaser phaser(1 + TEST_READER_THREADS);
  TypeParam mtx;
  std::atomic<int> nread = {};
  yamc::test::stopwatch<> sw;
  yamc::test::task_runner(
    1 + TEST_READER_THREADS,
    [&](std::size_t id) {
      auto ph = phaser.get(id);
      if (id == 0) {
        // writer-thread
        ph.await();     // p1
        EXPECT_NO_THROW(mtx.lock());
        EXPECT_EQ(TEST_READER_THREADS, nread);
        WAIT_TICKS;
        EXPECT_NO_THROW(mtx.unlock());
      } else {
        // reader-threads
        yamc::shared_lock<TypeParam> read_lk(mtx);
        ph.advance(1);  // p1
        ++nread;
        WAIT_TICKS;
      }
    });
  ASSERT_LE(TEST_TICKS * 2, sw.elapsed());
}

// shared_mutex::try_lock()
TYPED_TEST(SharedMutexTest, TryLock)
{
  yamc::test::phaser phaser(1 + TEST_READER_THREADS);
  TypeParam mtx;
  std::atomic<int> nread = {};
  yamc::test::stopwatch<> sw;
  yamc::test::task_runner(
    1 + TEST_READER_THREADS,
    [&](std::size_t id) {
      auto ph = phaser.get(id);
      if (id == 0) {
        // writer-thread
        ph.await();     // p1
        while (!mtx.try_lock()) {
          std::this_thread::yield();
        }
        EXPECT_EQ(TEST_READER_THREADS, nread);
        WAIT_TICKS;
        EXPECT_NO_THROW(mtx.unlock());
      } else {
        // reader-threads
        yamc::shared_lock<TypeParam> read_lk(mtx);
        ph.advance(1);  // p1
        ++nread;
        WAIT_TICKS;
      }
    });
  ASSERT_LE(TEST_TICKS * 2, sw.elapsed());
}

// shared_mutex::try_lock() failure
TYPED_TEST(SharedMutexTest, TryLockFail)
{
  yamc::test::barrier step(2);
  TypeParam mtx;
  // reader-thread
  yamc::test::join_thread thd([&]{
    EXPECT_NO_THROW(mtx.lock_shared());
    step.await();  // b1
    step.await();  // b2
    EXPECT_NO_THROW(mtx.unlock_shared());
  });
  // writer-thread
  {
    step.await();  // b1
    EXPECT_FALSE(mtx.try_lock());
    step.await();  // b2
  }
}

// shared_mutex::lock_shared()
TYPED_TEST(SharedMutexTest, LockShared)
{
  yamc::test::phaser phaser(1 + TEST_READER_THREADS);
  TypeParam mtx;
  int data = 0;
  yamc::test::stopwatch<> sw;
  yamc::test::task_runner(
    1 + TEST_READER_THREADS,
    [&](std::size_t id) {
      auto ph = phaser.get(id);
      if (id == 0) {
        // writer-thread
        std::lock_guard<TypeParam> write_lk(mtx);
        ph.advance(1);  // p1
        data = 42;
        WAIT_TICKS;
      } else {
        // reader-threads
        ph.await();     // p1
        EXPECT_NO_THROW(mtx.lock_shared());
        EXPECT_EQ(42, data);
        WAIT_TICKS;
        EXPECT_NO_THROW(mtx.unlock_shared());
      }
    });
  ASSERT_LE(TEST_TICKS * 2, sw.elapsed());
}

// shared_mutex::try_lock_shared()
TYPED_TEST(SharedMutexTest, TryLockShared)
{
  yamc::test::phaser phaser(1 + TEST_READER_THREADS);
  TypeParam mtx;
  int data = 0;
  yamc::test::stopwatch<> sw;
  yamc::test::task_runner(
    1 + TEST_READER_THREADS,
    [&](std::size_t id) {
      auto ph = phaser.get(id);
      if (id == 0) {
        // writer-thread
        std::lock_guard<TypeParam> write_lk(mtx);
        ph.advance(1);  // p1
        data = 42;
        WAIT_TICKS;
      } else {
        // reader-threads
        ph.await();     // p1
        while (!mtx.try_lock_shared()) {
          std::this_thread::yield();
        }
        EXPECT_EQ(42, data);
        WAIT_TICKS;
        EXPECT_NO_THROW(mtx.unlock_shared());
      }
    });
  ASSERT_LE(TEST_TICKS * 2, sw.elapsed());
}

// shared_mutex::try_lock_shared() failure
TYPED_TEST(SharedMutexTest, TryLockSharedFail)
{
  yamc::test::barrier step(1 + TEST_READER_THREADS);
  TypeParam mtx;
  yamc::test::task_runner(
    1 + TEST_READER_THREADS,
    [&](std::size_t id) {
      if (id == 0) {
        // writer-threads
        EXPECT_NO_THROW(mtx.lock());
        step.await();  // b1
        step.await();  // b2
        EXPECT_NO_THROW(mtx.unlock());
      } else {
        // reader-threads
        step.await();  // b1
        EXPECT_FALSE(mtx.try_lock_shared());
        step.await();  // b2
      }
    });
}


using SharedTimedMutexTypes = ::testing::Types<
  yamc::alternate::basic_shared_timed_mutex<yamc::rwlock::ReaderPrefer>,
  yamc::alternate::basic_shared_timed_mutex<yamc::rwlock::WriterPrefer>
>;

template <typename Mutex>
struct SharedTimedMutexTest : ::testing::Test {};

TYPED_TEST_CASE(SharedTimedMutexTest, SharedTimedMutexTypes);

// shared_timed_mutex::try_lock_for()
TYPED_TEST(SharedTimedMutexTest, TryLockFor)
{
  yamc::test::phaser phaser(1 + TEST_READER_THREADS);
  TypeParam mtx;
  std::atomic<int> nread = {};
  yamc::test::stopwatch<> sw;
  yamc::test::task_runner(
    1 + TEST_READER_THREADS,
    [&](std::size_t id) {
      auto ph = phaser.get(id);
      if (id == 0) {
        // writer-thread
        ph.await();     // p1
        EXPECT_TRUE(mtx.try_lock_for(TEST_NOT_TIMEOUT));
        EXPECT_EQ(TEST_READER_THREADS, nread);
        WAIT_TICKS;
        EXPECT_NO_THROW(mtx.unlock());
      } else {
        // reader-threads
        yamc::shared_lock<TypeParam> read_lk(mtx);
        ph.advance(1);  // p1
        ++nread;
        WAIT_TICKS;
      }
    });
  ASSERT_LE(TEST_TICKS * 2, sw.elapsed());
}

// shared_timed_mutex::try_lock_until()
TYPED_TEST(SharedTimedMutexTest, TryLockUntil)
{
  yamc::test::phaser phaser(1 + TEST_READER_THREADS);
  TypeParam mtx;
  std::atomic<int> nread = {};
  yamc::test::stopwatch<> sw;
  yamc::test::task_runner(
    1 + TEST_READER_THREADS,
    [&](std::size_t id) {
      auto ph = phaser.get(id);
      if (id == 0) {
        // writer-thread
        ph.await();     // p1
        EXPECT_TRUE(mtx.try_lock_until(std::chrono::system_clock::now() + TEST_NOT_TIMEOUT));
        EXPECT_EQ(TEST_READER_THREADS, nread);
        WAIT_TICKS;
        EXPECT_NO_THROW(mtx.unlock());
      } else {
        // reader-threads
        yamc::shared_lock<TypeParam> read_lk(mtx);
        ph.advance(1);  // p1
        ++nread;
        WAIT_TICKS;
      }
    });
  ASSERT_LE(TEST_TICKS * 2, sw.elapsed());
}

// shared_timed_mutex::try_lock_for() timeout
TYPED_TEST(SharedTimedMutexTest, TryLockForTimeout)
{
  yamc::test::barrier step(2);
  TypeParam mtx;
  // reader-thread
  yamc::test::join_thread thd([&]{
    EXPECT_NO_THROW(mtx.lock_shared());
    step.await();  // b1
    step.await();  // b2
    EXPECT_NO_THROW(mtx.unlock_shared());
  });
  // writer-thread
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

// shared_timed_mutex::try_lock_until() timeout
TYPED_TEST(SharedTimedMutexTest, TryLockUntilTimeout)
{
  yamc::test::barrier step(2);
  TypeParam mtx;
  // reader-thread
  yamc::test::join_thread thd([&]{
    EXPECT_NO_THROW(mtx.lock_shared());
    step.await();  // b1
    step.await();  // b2
    EXPECT_NO_THROW(mtx.unlock_shared());
  });
  // writer-thread
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

// shared_timed_mutex::try_lock_shared_for()
TYPED_TEST(SharedTimedMutexTest, TryLockSharedFor)
{
  yamc::test::phaser phaser(1 + TEST_READER_THREADS);
  TypeParam mtx;
  yamc::test::stopwatch<> sw;
  yamc::test::task_runner(
    1 + TEST_READER_THREADS,
    [&](std::size_t id) {
      auto ph = phaser.get(id);
      if (id == 0) {
        // writer-threads
        std::lock_guard<TypeParam> write_lk(mtx);
        WAIT_TICKS;
        ph.advance(1);  // p1
      } else {
        // reader-threads
        ph.await();     // p1
        EXPECT_TRUE(mtx.try_lock_shared_for(TEST_NOT_TIMEOUT));
        WAIT_TICKS;
        mtx.unlock_shared();
      }
    });
  ASSERT_LE(TEST_TICKS * 2, sw.elapsed());
}

// shared_timed_mutex::try_lock_shared_until()
TYPED_TEST(SharedTimedMutexTest, TryLockSharedUntil)
{
  yamc::test::phaser phaser(1 + TEST_READER_THREADS);
  TypeParam mtx;
  yamc::test::stopwatch<> sw;
  yamc::test::task_runner(
    1 + TEST_READER_THREADS,
    [&](std::size_t id) {
      auto ph = phaser.get(id);
      if (id == 0) {
        // writer-threads
        std::lock_guard<TypeParam> write_lk(mtx);
        WAIT_TICKS;
        ph.advance(1);  // p1
      } else {
        // reader-threads
        ph.await();     // p2
        EXPECT_TRUE(mtx.try_lock_shared_until(std::chrono::system_clock::now() + TEST_NOT_TIMEOUT));
        WAIT_TICKS;
        mtx.unlock_shared();
      }
    });
  ASSERT_LE(TEST_TICKS * 2, sw.elapsed());
}

// shared_timed_mutex::try_lock_shared_for() timeout
TYPED_TEST(SharedTimedMutexTest, TryLockSharedForTimeout)
{
  yamc::test::barrier step(1 + TEST_READER_THREADS);
  TypeParam mtx;
  yamc::test::task_runner(
    1 + TEST_READER_THREADS,
    [&](std::size_t id) {
      if (id == 0) {
        // writer-thread
        EXPECT_NO_THROW(mtx.lock());
        step.await();  // b1
        step.await();  // b2
        EXPECT_NO_THROW(mtx.unlock());
      } else {
        // reader-threads
        step.await();  // b1
        yamc::test::stopwatch<> sw;
        bool result = mtx.try_lock_shared_for(TEST_EXPECT_TIMEOUT);
        auto elapsed = sw.elapsed();
        ASSERT_EQ(false, result);
        EXPECT_LE(TEST_EXPECT_TIMEOUT, elapsed);
        step.await();  // b2
      }
    });
}

// shared_timed_mutex::try_lock_shared_until() timeout
TYPED_TEST(SharedTimedMutexTest, TryLockSharedUntilTimeout)
{
  yamc::test::barrier step(1 + TEST_READER_THREADS);
  TypeParam mtx;
  yamc::test::task_runner(
    1 + TEST_READER_THREADS,
    [&](std::size_t id) {
      if (id == 0) {
        // writer-thread
        EXPECT_NO_THROW(mtx.lock());
        step.await();  // b1
        step.await();  // b2
        EXPECT_NO_THROW(mtx.unlock());
      } else {
        // reader-threads
        step.await();  // b1
        const auto tp = std::chrono::system_clock::now() + TEST_EXPECT_TIMEOUT;
        yamc::test::stopwatch<> sw;
        bool result = mtx.try_lock_shared_until(tp);
        auto elapsed = sw.elapsed();
        ASSERT_EQ(false, result);
        EXPECT_LE(TEST_EXPECT_TIMEOUT, elapsed);
        step.await();  // b2
      }
    });
}
