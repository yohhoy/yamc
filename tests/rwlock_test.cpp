/*
 * rwlock_test.cpp
 */
#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>
#include "gtest/gtest.h"
#include "checked_shared_mutex.hpp"
#include "fair_shared_mutex.hpp"
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

#define EXPECT_STEP(n_) \
  { TRACE("STEP"#n_); EXPECT_EQ(n_, ++step); WAIT_TICKS; }
#define EXPECT_STEP_RANGE(r0_, r1_) \
  { TRACE("STEP"#r0_"-"#r1_); int s = ++step; EXPECT_TRUE(r0_ <= s && s <= r1_); WAIT_TICKS; }


using SharedMutexTypes = ::testing::Types<
  yamc::checked::shared_mutex,
  yamc::checked::shared_timed_mutex,
  yamc::fair::shared_mutex,
  yamc::fair::shared_timed_mutex,
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
        ASSERT_NO_THROW(mtx.lock_shared());
        ph.advance(1);  // p1
        ++nread;
        WAIT_TICKS;
        ASSERT_NO_THROW(mtx.unlock_shared());
      }
    });
  EXPECT_LE(TEST_TICKS * 2, sw.elapsed());
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
        ASSERT_NO_THROW(mtx.lock_shared());
        ph.advance(1);  // p1
        ++nread;
        WAIT_TICKS;
        ASSERT_NO_THROW(mtx.unlock_shared());
      }
    });
  EXPECT_LE(TEST_TICKS * 2, sw.elapsed());
}

// shared_mutex::try_lock() failure
TYPED_TEST(SharedMutexTest, TryLockFail)
{
  yamc::test::barrier step(2);
  TypeParam mtx;
  // reader-thread
  yamc::test::join_thread thd([&]{
    ASSERT_NO_THROW(mtx.lock_shared());
    step.await();  // b1
    step.await();  // b2
    ASSERT_NO_THROW(mtx.unlock_shared());
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
        ASSERT_NO_THROW(mtx.lock());
        ph.advance(1);  // p1
        data = 42;
        WAIT_TICKS;
        ASSERT_NO_THROW(mtx.unlock());
      } else {
        // reader-threads
        ph.await();     // p1
        EXPECT_NO_THROW(mtx.lock_shared());
        EXPECT_EQ(42, data);
        WAIT_TICKS;
        EXPECT_NO_THROW(mtx.unlock_shared());
      }
    });
  EXPECT_LE(TEST_TICKS * 2, sw.elapsed());
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
        ASSERT_NO_THROW(mtx.lock());
        ph.advance(1);  // p1
        data = 42;
        WAIT_TICKS;
        ASSERT_NO_THROW(mtx.unlock());
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
  EXPECT_LE(TEST_TICKS * 2, sw.elapsed());
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
        ASSERT_NO_THROW(mtx.lock());
        step.await();  // b1
        step.await();  // b2
        ASSERT_NO_THROW(mtx.unlock());
      } else {
        // reader-threads
        step.await();  // b1
        EXPECT_FALSE(mtx.try_lock_shared());
        step.await();  // b2
      }
    });
}


using SharedTimedMutexTypes = ::testing::Types<
  yamc::checked::shared_timed_mutex,
  yamc::fair::shared_timed_mutex,
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
        ASSERT_NO_THROW(mtx.lock_shared());
        ph.advance(1);  // p1
        ++nread;
        WAIT_TICKS;
        ASSERT_NO_THROW(mtx.unlock_shared());
      }
    });
  EXPECT_LE(TEST_TICKS * 2, sw.elapsed());
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
        ASSERT_NO_THROW(mtx.lock_shared());
        ph.advance(1);  // p1
        ++nread;
        WAIT_TICKS;
        ASSERT_NO_THROW(mtx.unlock_shared());
      }
    });
  EXPECT_LE(TEST_TICKS * 2, sw.elapsed());
}

// shared_timed_mutex::try_lock_for() timeout
TYPED_TEST(SharedTimedMutexTest, TryLockForTimeout)
{
  yamc::test::barrier step(2);
  TypeParam mtx;
  // reader-thread
  yamc::test::join_thread thd([&]{
    ASSERT_NO_THROW(mtx.lock_shared());
    step.await();  // b1
    step.await();  // b2
    ASSERT_NO_THROW(mtx.unlock_shared());
  });
  // writer-thread
  {
    step.await();  // b1
    yamc::test::stopwatch<> sw;
    EXPECT_FALSE(mtx.try_lock_for(TEST_EXPECT_TIMEOUT));
    EXPECT_LE(TEST_EXPECT_TIMEOUT, sw.elapsed());
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
    ASSERT_NO_THROW(mtx.lock_shared());
    step.await();  // b1
    step.await();  // b2
    ASSERT_NO_THROW(mtx.unlock_shared());
  });
  // writer-thread
  {
    step.await();  // b1
    yamc::test::stopwatch<> sw;
    EXPECT_FALSE(mtx.try_lock_until(std::chrono::system_clock::now() + TEST_EXPECT_TIMEOUT));
    EXPECT_LE(TEST_EXPECT_TIMEOUT, sw.elapsed());
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
        ASSERT_NO_THROW(mtx.lock());
        ph.advance(1);  // p1
        WAIT_TICKS;
        ASSERT_NO_THROW(mtx.unlock());
      } else {
        // reader-threads
        ph.await();     // p1
        EXPECT_TRUE(mtx.try_lock_shared_for(TEST_NOT_TIMEOUT));
        WAIT_TICKS;
        EXPECT_NO_THROW(mtx.unlock_shared());
      }
    });
  EXPECT_LE(TEST_TICKS * 2, sw.elapsed());
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
        ASSERT_NO_THROW(mtx.lock());
        ph.advance(1);  // p1
        WAIT_TICKS;
        ASSERT_NO_THROW(mtx.unlock());
      } else {
        // reader-threads
        ph.await();     // p1
        EXPECT_TRUE(mtx.try_lock_shared_until(std::chrono::system_clock::now() + TEST_NOT_TIMEOUT));
        WAIT_TICKS;
        EXPECT_NO_THROW(mtx.unlock_shared());
      }
    });
  EXPECT_LE(TEST_TICKS * 2, sw.elapsed());
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
        ASSERT_NO_THROW(mtx.lock());
        step.await();  // b1
        step.await();  // b2
        ASSERT_NO_THROW(mtx.unlock());
      } else {
        // reader-threads
        step.await();  // b1
        yamc::test::stopwatch<> sw;
        EXPECT_FALSE(mtx.try_lock_shared_for(TEST_EXPECT_TIMEOUT));
        EXPECT_LE(TEST_EXPECT_TIMEOUT, sw.elapsed());
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
        ASSERT_NO_THROW(mtx.lock());
        step.await();  // b1
        step.await();  // b2
        ASSERT_NO_THROW(mtx.unlock());
      } else {
        // reader-threads
        step.await();  // b1
        yamc::test::stopwatch<> sw;
        EXPECT_FALSE(mtx.try_lock_shared_until(std::chrono::system_clock::now() + TEST_EXPECT_TIMEOUT));
        EXPECT_LE(TEST_EXPECT_TIMEOUT, sw.elapsed());
        step.await();  // b2
      }
    });
}


using RWLockReaderPreferTypes = ::testing::Types<
  yamc::alternate::basic_shared_mutex<yamc::rwlock::ReaderPrefer>,
  yamc::alternate::basic_shared_timed_mutex<yamc::rwlock::ReaderPrefer>
>;

template <typename Mutex>
struct RWLockReaderPreferTest : ::testing::Test {};

TYPED_TEST_CASE(RWLockReaderPreferTest, RWLockReaderPreferTypes);

// Reader prefer lock
//
// T0: L=a=1=U...w...a.l-----L=7=U
//       |   |   |    \      |
// T1: ..w.s-S=2=a=3=V.w.S=6=V....
//       |       |     |
// T2: ..a.......w.S=4=a=5=V......
//
//   CriticalPath = 1-2-{3|4}-6-7
//
//   l/L=lock(request/acquire), U=unlock()
//   s/S=lock_shared(request/acquire), V=unlock_shared()
//   a=phase advance, w=phase await
//
TYPED_TEST(RWLockReaderPreferTest, LockOrder)
{
  yamc::test::phaser phaser(3);
  std::atomic<int> step = {};
  TypeParam mtx;
  yamc::test::stopwatch<> sw;
  yamc::test::task_runner(3, [&](std::size_t id) {
    auto ph = phaser.get(id);
    switch (id) {
    case 0:
      mtx.lock();
      EXPECT_STEP(1)
      ph.advance(1);  // p1
      mtx.unlock();
      ph.await();     // p2
      ph.advance(1);  // p3
      mtx.lock();
      EXPECT_STEP(7)
      mtx.unlock();
      break;
    case 1:
      ph.await();     // p1
      mtx.lock_shared();
      EXPECT_STEP(2)
      ph.advance(1);  // p2
      EXPECT_STEP_RANGE(3, 4)
      mtx.unlock_shared();
      ph.await();     // p3
      mtx.lock_shared();
      EXPECT_STEP_RANGE(5, 6)
      mtx.unlock_shared();
      break;
    case 2:
      ph.advance(1);  // p1
      ph.await();     // p2
      mtx.lock_shared();
      EXPECT_STEP_RANGE(3, 4)
      ph.advance(1);  // p3
      EXPECT_STEP_RANGE(5, 6)
      mtx.unlock_shared();
      break;
    }
  });
  EXPECT_LE(TEST_TICKS * 5, sw.elapsed());
}


using RwLockWriterPreferTypes = ::testing::Types<
  yamc::alternate::basic_shared_mutex<yamc::rwlock::WriterPrefer>,
  yamc::alternate::basic_shared_timed_mutex<yamc::rwlock::WriterPrefer>
>;

template <typename Mutex>
struct RwLockWriterPreferTest : ::testing::Test {};

TYPED_TEST_CASE(RwLockWriterPreferTest, RwLockWriterPreferTypes);

// Writer prefer lock
//
// T0: L=a=1=U...w.a.l---L=5=U....
//       |   |   |  \    |   |
// T1: ..w.s-S=2=a=3=a=4=V...|....
//       |       |   |       |
// T2: ..a.......a...w.s-----S=6=V
//
//   CriticalPath = 1-2-3-4-5-6
//
//   l/L=lock(request/acquire), U=unlock()
//   s/S=lock_shared(request/acquire), V=unlock_shared()
//   a=phase advance, w=phase await
//
TYPED_TEST(RwLockWriterPreferTest, LockOrder)
{
  yamc::test::phaser phaser(3);
  std::atomic<int> step = {};
  TypeParam mtx;
  yamc::test::stopwatch<> sw;
  yamc::test::task_runner(3, [&](std::size_t id) {
    auto ph = phaser.get(id);
    switch (id) {
    case 0:
      mtx.lock();
      EXPECT_STEP(1)
      ph.advance(1);  // p1
      mtx.unlock();
      ph.await();     // p2
      ph.advance(1);  // p3
      mtx.lock();
      EXPECT_STEP(5)
      mtx.unlock();
      break;
    case 1:
      ph.await();     // p1
      mtx.lock_shared();
      EXPECT_STEP(2)
      ph.advance(1);  // p2
      EXPECT_STEP(3)
      ph.advance(1);  // p3
      EXPECT_STEP(4)
      mtx.unlock_shared();
      break;
    case 2:
      ph.advance(2);  // p1-2
      ph.await();     // p3
      mtx.lock_shared();
      EXPECT_STEP(6)
      mtx.unlock_shared();
      break;
    }
  });
  EXPECT_LE(TEST_TICKS * 6, sw.elapsed());
}
