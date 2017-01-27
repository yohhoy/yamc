/*
 * fairness_test.cpp
 */
#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>
#include "gtest/gtest.h"
#include "fair_mutex.hpp"
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


#define TEST_EXPECT_TIMEOUT std::chrono::milliseconds(10)
#define TEST_NOT_TIMEOUT std::chrono::minutes(3)
#define TEST_TICKS std::chrono::milliseconds(200)

#define EXPECT_STEP(n_) \
  { TRACE("STEP"#n_); EXPECT_EQ(n_, ++step); std::this_thread::sleep_for(TEST_TICKS); }


using FairMutexTypes = ::testing::Types<
  yamc::fair::mutex,
  yamc::fair::timed_mutex,
  yamc::fair::recursive_mutex
>;

template <typename Mutex>
struct FairMutexTest : ::testing::Test {};

TYPED_TEST_CASE(FairMutexTest, FairMutexTypes);

// FIFO scheduling
//
// T0: T=1=a=a=====w=4=U.......L=7=U
//         |  \   /    |       |
// T1: ....w.2.a.a.l---L=5=U........
//         |   |  \        |   |
// T2: ....w.t.w.3.a.l-----L=6=U....
//
//   l/L=lock(request/acquired), U=unlock()
//   T=try_lock(true), t=try_lock(false)
//   a=phase advance, w=phase await
//
TYPED_TEST(FairMutexTest, FifoSched)
{
  yamc::test::phaser phaser(3);
  int step = 0;
  TypeParam mtx;
  yamc::test::stopwatch<> sw;
  yamc::test::task_runner(3, [&](std::size_t id) {
    auto ph = phaser.get(id);
    switch (id) {
    case 0:
      EXPECT_TRUE(mtx.try_lock());
      EXPECT_STEP(1)
      ph.advance(2);  // p1-2
      ph.await();     // p3
      EXPECT_STEP(4)
      mtx.unlock();
      mtx.lock();
      EXPECT_STEP(7)
      mtx.unlock();
      break;
    case 1:
      ph.await();     // p1
      EXPECT_STEP(2)
      ph.advance(2);  // p2-3
      mtx.lock();
      EXPECT_STEP(5)
      mtx.unlock();
      break;
    case 2:
      ph.await();     // p1
      EXPECT_FALSE(mtx.try_lock());
      ph.await();     // p2
      EXPECT_STEP(3)
      ph.advance(1);  // p3
      mtx.lock();
      EXPECT_STEP(6)
      mtx.unlock();
      break;
    }
  });
  ASSERT_LE(TEST_TICKS * step, sw.elapsed());
}


using FairTimedMutexTypes = ::testing::Types<
  yamc::fair::timed_mutex
>;

template <typename Mutex>
struct FairTimedMutexTest : ::testing::Test {};

TYPED_TEST_CASE(FairTimedMutexTest, FairTimedMutexTypes);

// FIFO scheduling with try_lock_for()
//
// T0: T=1=a=====a=====w=4=U........
//         |      \   /    |
// T1: ....w.......w.a.t---T=5=U....
//         |       |  \        |
// T2: ....w.t-2-*.w.3.a.t-----T=6=U
//
//   t/T=try_lock_for(request/acquired)
//   U=unlock(), *=timeout
//   a=phase advance, w=phase await
//
TYPED_TEST(FairTimedMutexTest, FifoTryLockFor)
{
  yamc::test::phaser phaser(3);
  int step = 0;
  TypeParam mtx;
  yamc::test::stopwatch<> sw;
  yamc::test::task_runner(3, [&](std::size_t id) {
    auto ph = phaser.get(id);
    switch (id) {
    case 0:
      EXPECT_TRUE(mtx.try_lock_for(TEST_NOT_TIMEOUT));
      EXPECT_STEP(1)
      ph.advance(2);  // p1-2
      ph.await();     // p3
      EXPECT_STEP(4)
      mtx.unlock();
      break;
    case 1:
      ph.await();     // p1
      ph.await();     // p2
      ph.advance(1);  // p3
      EXPECT_TRUE(mtx.try_lock_for(TEST_NOT_TIMEOUT));
      EXPECT_STEP(5)
      mtx.unlock();
      break;
    case 2:
      ph.await();     // p1
      EXPECT_EQ(2, ++step);
      EXPECT_FALSE(mtx.try_lock_for(TEST_TICKS));
      ph.await();     // p2
      EXPECT_STEP(3)
      ph.advance(1);  // p3
      EXPECT_TRUE(mtx.try_lock_for(TEST_NOT_TIMEOUT));
      EXPECT_STEP(6)
      mtx.unlock();
      break;
    }
  });
  ASSERT_LE(TEST_TICKS * step, sw.elapsed());
}

// FIFO scheduling with try_lock_until()
//
// T0: T=1=a=====a=====w=4=U........
//         |      \   /    |
// T1: ....w.......w.a.t---L=5=U....
//         |       |  \        |
// T2: ....w.t-2-*.w.3.a.t-----L=6=U
//
//   t/T=try_lock_until(request/acquired)
//   U=unlock(), *=timeout
//   a=phase advance, w=phase await
//
TYPED_TEST(FairTimedMutexTest, FifoTryLockUntil)
{
  yamc::test::phaser phaser(3);
  int step = 0;
  TypeParam mtx;
  using Clock = std::chrono::steady_clock;
  yamc::test::stopwatch<> sw;
  yamc::test::task_runner(3, [&](std::size_t id) {
    auto ph = phaser.get(id);
    switch (id) {
    case 0:
      EXPECT_TRUE(mtx.try_lock_until(Clock::now() + TEST_NOT_TIMEOUT));
      EXPECT_STEP(1)
      ph.advance(2);  // p1-2
      ph.await();     // p3
      EXPECT_STEP(4)
      mtx.unlock();
      break;
    case 1:
      ph.await();     // p1
      ph.await();     // p2
      ph.advance(1);  // p3
      EXPECT_TRUE(mtx.try_lock_until(Clock::now() + TEST_NOT_TIMEOUT));
      EXPECT_STEP(5)
      mtx.unlock();
      break;
    case 2:
      ph.await();     // p1
      EXPECT_EQ(2, ++step);
      EXPECT_FALSE(mtx.try_lock_until(Clock::now() + TEST_TICKS));
      ph.await();     // p2
      EXPECT_STEP(3)
      ph.advance(1);  // p3
      EXPECT_TRUE(mtx.try_lock_until(Clock::now() + TEST_NOT_TIMEOUT));
      EXPECT_STEP(6)
      mtx.unlock();
      break;
    }
  });
  ASSERT_LE(TEST_TICKS * step, sw.elapsed());
}
