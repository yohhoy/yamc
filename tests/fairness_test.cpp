/*
 * fairness_test.cpp
 */
#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>
#include "gtest/gtest.h"
#include "fair_mutex.hpp"
#include "fair_shared_mutex.hpp"
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
#define WAIT_TICKS std::this_thread::sleep_for(TEST_TICKS)

#define EXPECT_STEP(n_) \
  { TRACE("STEP"#n_); EXPECT_EQ(n_, ++step); std::this_thread::sleep_for(TEST_TICKS); }
#define EXPECT_STEP_RANGE(r0_, r1_) \
  { TRACE("STEP"#r0_"-"#r1_); int s = ++step; EXPECT_TRUE(r0_ <= s && s <= r1_); WAIT_TICKS; }


using FairMutexTypes = ::testing::Types<
  yamc::fair::mutex,
  yamc::fair::timed_mutex,
  yamc::fair::recursive_mutex,
  yamc::fair::recursive_timed_mutex,
  yamc::fair::shared_mutex,
  yamc::fair::shared_timed_mutex
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
//   CriticalPath = 1-2-3-4-5-6-7
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
  EXPECT_LE(TEST_TICKS * step, sw.elapsed());
}


using FairTimedMutexTypes = ::testing::Types<
  yamc::fair::timed_mutex,
  yamc::fair::recursive_timed_mutex,
  yamc::fair::shared_timed_mutex
>;

template <typename Mutex>
struct FairTimedMutexTest : ::testing::Test {};

TYPED_TEST_CASE(FairTimedMutexTest, FairTimedMutexTypes);

// FIFO scheduling with try_lock_for() timeout
//
// T0: T=a=0=a=======w=3=U.t-----T=6=U
//       |  /        |   |       |
// T1: ..w.a.t-1-2-*-a-t-|---T=5=U....
//       |  \   /---/    |   |
// T2: ..a...w.a.t-------T=4=U........
//
//   CriticalPath = 1-2-3-4-5-6
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
      ph.advance(1);  // p1
      WAIT_TICKS;
      ph.advance(1);  // p2
      ph.await();     // p3
      EXPECT_STEP(3)
      mtx.unlock();
      EXPECT_TRUE(mtx.try_lock_for(TEST_NOT_TIMEOUT));
      EXPECT_STEP(6)
      mtx.unlock();
      break;
    case 1:
      ph.await();     // p1
      ph.advance(1);  // p2
      EXPECT_FALSE(mtx.try_lock_for(TEST_TICKS * 2));
      step += 2;
      TRACE("STEP:1-2(timeout)");
      ph.advance(1);  // p3
      EXPECT_TRUE(mtx.try_lock_for(TEST_NOT_TIMEOUT));
      EXPECT_STEP(5)
      mtx.unlock();
      break;
    case 2:
      ph.advance(1);  // p1
      ph.await();     // p2
      ph.advance(1);  // p3
      EXPECT_TRUE(mtx.try_lock_for(TEST_NOT_TIMEOUT));
      EXPECT_STEP(4)
      mtx.unlock();
      break;
    }
  });
  EXPECT_LE(TEST_TICKS * step, sw.elapsed());
}

// FIFO scheduling with try_lock_until() timeout
//
// T0: T=a=0=a=======w=3=U.t-----T=6=U
//       |  /        |   |       |
// T1: ..w.a.t-1-2-*-a-t-|---T=5=U....
//       |  \   /---/    |   |
// T2: ..a...w.a.t-------T=4=U........
//
//   CriticalPath = 1-2-3-4-5-6
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
      ph.advance(1);  // p1
      WAIT_TICKS;
      ph.advance(1);  // p2
      ph.await();     // p3
      EXPECT_STEP(3)
      mtx.unlock();
      EXPECT_TRUE(mtx.try_lock_until(Clock::now() + TEST_NOT_TIMEOUT));
      EXPECT_STEP(6)
      mtx.unlock();
      break;
    case 1:
      ph.await();     // p1
      ph.advance(1);  // p2
      EXPECT_FALSE(mtx.try_lock_until(Clock::now() + TEST_TICKS * 2));
      step += 2;
      TRACE("STEP:1-2(timeout)");
      ph.advance(1);  // p3
      EXPECT_TRUE(mtx.try_lock_until(Clock::now() + TEST_NOT_TIMEOUT));
      EXPECT_STEP(5)
      mtx.unlock();
      break;
    case 2:
      ph.advance(1);  // p1
      ph.await();     // p2
      ph.advance(1);  // p3
      EXPECT_TRUE(mtx.try_lock_until(Clock::now() + TEST_NOT_TIMEOUT));
      EXPECT_STEP(4)
      mtx.unlock();
      break;
    }
  });
  EXPECT_LE(TEST_TICKS * step, sw.elapsed());
}


using FairSharedMutexTypes = ::testing::Types<
  yamc::fair::shared_mutex,
  yamc::fair::shared_timed_mutex
>;

template <typename Mutex>
struct FairSharedMutexTest : ::testing::Test {};

TYPED_TEST_CASE(FairSharedMutexTest, FairSharedMutexTypes);

// phase-fair readers-writer lock scheduling
//
// T0/W: L=a=1=a=2=a=3=a=a=U................
//         |   |   |   |  \|--\
// T1/R: ..w.s-|---|---|---S=4=w=6=V........
//         |   |   |   |   |    \  |
// T2/R: ..a...w.s-|---|---S=5=V.a.|.s-S=9=V
//         |  /    |   |           |   |
// T3/W: ..a.a.....w.l-|-----------L=7=U....
//         | |    /    |               |
// T4/R: ..a.a...a.....w.s-------------S=8=V
//
//   CriticalPath = 1-2-3-4-6-7-{8|9}
//
//   l/L=lock(request/acquired), U=unlock()
//   s/S=lock_shared(request/acquired), V=unlock_shared()
//   a=phase advance, w=phase await
//
TYPED_TEST(FairSharedMutexTest, PhaseFifoSched)
{
  yamc::test::phaser phaser(5);
  std::atomic<int> step = {0};
  TypeParam mtx;
  yamc::test::stopwatch<> sw;
  yamc::test::task_runner(5, [&](std::size_t id) {
    auto ph = phaser.get(id);
    switch (id) {
    case 0:
      mtx.lock();
      ph.advance(1);  // p1
      EXPECT_STEP(1);
      ph.advance(1);  // p2
      EXPECT_STEP(2);
      ph.advance(1);  // p3
      EXPECT_STEP(3);
      ph.advance(2);  // p4-5
      mtx.unlock();
      break;
    case 1:
      ph.await();     // p1
      ph.advance(3);  // p2-4
      mtx.lock_shared();
      EXPECT_STEP_RANGE(4, 5);
      ph.await();     // p5
      EXPECT_STEP(6);
      mtx.unlock_shared();
      break;
    case 2:
      ph.advance(1);  // p1
      ph.await();     // p2
      ph.advance(2);  // p3-4
      mtx.lock_shared();
      EXPECT_STEP_RANGE(4, 5);
      mtx.unlock_shared();
      ph.advance(1);  // p5
      mtx.lock_shared();
      EXPECT_STEP_RANGE(8, 9);
      mtx.unlock_shared();
      break;
    case 3:
      ph.advance(2);  // p1-2
      ph.await();     // p3
      ph.advance(2);  // p4-5
      mtx.lock();
      EXPECT_STEP(7);
      mtx.unlock();
      break;
    case 4:
      ph.advance(3);  // p1-3
      ph.await();     // p4
      ph.advance(1);  // p5
      mtx.lock_shared();
      EXPECT_STEP_RANGE(8, 9);
      mtx.unlock_shared();
      break;
    }
  });
  EXPECT_LE(TEST_TICKS * 7, sw.elapsed());
}


// phase-fair RW lock scheduling try_lock_for() timeout
//
// T0/W: T=a=1=a=2=a=3=a=a=U..........
//         |   |   |   |  \|----\
// T1/R: ..w.s-|---|---|---S=====w=7=V
//         |   |   |   |   |     |
// T2/R: ..a...w.s-|---|---S=6=V.a....
//         |  /    |   |         |
// T3/W: ..a.a.....w.t-|-4---5-*.a....
//         | |    /    |       | |
// T4/R: ..a.a...a.....w.s-----S=w=8=V
//
//   CriticalPath = 1-2-{3-6|4-5}-{7|8}
//
//   t/T=try_lock_for(request/acquired), U=unlock(), *=timeout
//   s/S=lock_shared(request/acquired), V=unlock_shared()
//   a=phase advance, w=phase await
//
TEST(FairSharedTimedMutexTest, PhaseFifoTryLockFor)
{
  yamc::test::phaser phaser(5);
  std::atomic<int> step = {0};
  yamc::fair::shared_timed_mutex mtx;
  yamc::test::stopwatch<> sw;
  yamc::test::task_runner(5, [&](std::size_t id) {
    auto ph = phaser.get(id);
    switch (id) {
    case 0:
      EXPECT_TRUE(mtx.try_lock_for(TEST_NOT_TIMEOUT));
      ph.advance(1);  // p1
      EXPECT_STEP(1);
      ph.advance(1);  // p2
      EXPECT_STEP(2);
      ph.advance(1);  // p3
      EXPECT_STEP(3);
      ph.advance(2);  // p4-5
      mtx.unlock();
      break;
    case 1:
      ph.await();     // p1
      ph.advance(3);  // p2-4
      mtx.lock_shared();
      ph.await();     // p5
      EXPECT_STEP_RANGE(7, 8);
      mtx.unlock_shared();
      break;
    case 2:
      ph.advance(1);  // p1
      ph.await();     // p2
      ph.advance(2);  // p3-4
      mtx.lock_shared();
      EXPECT_STEP_RANGE(4, 6);
      mtx.unlock_shared();
      ph.advance(1);  // p5
      break;
    case 3:
      ph.advance(2);  // p1-2
      ph.await();     // p3
      ph.advance(1);  // p4
      EXPECT_FALSE(mtx.try_lock_for(TEST_TICKS * 2));
      step += 2;
      TRACE("STEP4-5(timeout)");
      ph.advance(1);  // p5
      break;
    case 4:
      ph.advance(3);  // p1-3
      ph.await();     // p4
      mtx.lock_shared();
      ph.await();     // p5
      EXPECT_STEP_RANGE(7, 8);
      mtx.unlock_shared();
      break;
    }
  });
  EXPECT_LE(TEST_TICKS * 5, sw.elapsed());
}

// phase-fair RW lock scheduling try_lock_until() timeout
//
// T0/W: T=a=1=a=2=a=3=a=a=U..........
//         |   |   |   |  \|----\
// T1/R: ..w.s-|---|---|---S=====w=7=V
//         |   |   |   |   |     |
// T2/R: ..a...w.s-|---|---S=6=V.a....
//         |  /    |   |         |
// T3/W: ..a.a.....w.t-|-4---5-*.a....
//         | |    /    |       | |
// T4/R: ..a.a...a.....w.s-----S=w=8=V
//
//   CriticalPath = 1-2-{3-6|4-5}-{7|8}
//
//   t/T=try_lock_until(request/acquired), U=unlock(), *=timeout
//   s/S=lock_shared(request/acquired), V=unlock_shared()
//   a=phase advance, w=phase await
//
TEST(FairSharedTimedMutexTest, PhaseFifoTryLockUntil)
{
  yamc::test::phaser phaser(5);
  std::atomic<int> step = {0};
  yamc::fair::shared_timed_mutex mtx;
  using Clock = std::chrono::steady_clock;
  yamc::test::stopwatch<> sw;
  yamc::test::task_runner(5, [&](std::size_t id) {
    auto ph = phaser.get(id);
    switch (id) {
    case 0:
      EXPECT_TRUE(mtx.try_lock_until(Clock::now() + TEST_NOT_TIMEOUT));
      ph.advance(1);  // p1
      EXPECT_STEP(1);
      ph.advance(1);  // p2
      EXPECT_STEP(2);
      ph.advance(1);  // p3
      EXPECT_STEP(3);
      ph.advance(2);  // p4-5
      mtx.unlock();
      break;
    case 1:
      ph.await();     // p1
      ph.advance(3);  // p2-4
      mtx.lock_shared();
      ph.await();     // p5
      EXPECT_STEP_RANGE(7, 8);
      mtx.unlock_shared();
      break;
    case 2:
      ph.advance(1);  // p1
      ph.await();     // p2
      ph.advance(2);  // p3-4
      mtx.lock_shared();
      EXPECT_STEP_RANGE(4, 6);
      mtx.unlock_shared();
      ph.advance(1);  // p5
      break;
    case 3:
      ph.advance(2);  // p1-2
      ph.await();     // p3
      ph.advance(1);  // p4
      EXPECT_FALSE(mtx.try_lock_until(Clock::now() + TEST_TICKS * 2));
      step += 2;
      TRACE("STEP4-5(timeout)");
      ph.advance(1);  // p5
      break;
    case 4:
      ph.advance(3);  // p1-3
      ph.await();     // p4
      mtx.lock_shared();
      ph.await();     // p5
      EXPECT_STEP_RANGE(7, 8);
      mtx.unlock_shared();
      break;
    }
  });
  EXPECT_LE(TEST_TICKS * 5, sw.elapsed());
}

// phase-fair RW lock scheduling try_lock_shared_for() timeout
//
// T0/W: L=a=1=a=====a=w=5=U........
//         |  /      | |   |
// T1/R: ..w.a.s-2-*.a.a...|........
//         |  \     / /    |
// T2/W: ..a...w.3.a.a.l---L=6=U....
//         |   |   |  \        |
// T3/R: ..a...a...w.4.a.s-----S=7=V
//
//   CriticalPath = 1-3-4-5-6-7
//
//   l/L=lock(request/acquired), U=unlock()
//   s/S=try_lock_shared_for(request/acquired)
//   V=unlock_shared(), *=timeout
//   a=phase advance, w=phase await
//
TEST(FairSharedTimedMutexTest, PhaseFifoTryLockSharedFor)
{
  yamc::test::phaser phaser(4);
  std::atomic<int> step = {0};
  yamc::fair::shared_timed_mutex mtx;
  yamc::test::stopwatch<> sw;
  yamc::test::task_runner(4, [&](std::size_t id) {
    auto ph = phaser.get(id);
    switch (id) {
    case 0:
      mtx.lock();
      ph.advance(1);  // p1
      EXPECT_STEP(1);
      ph.advance(2);  // p2-3
      ph.await() ;    // p4
      EXPECT_STEP(5);
      mtx.unlock();
      break;
    case 1:
      ph.await();     // p1
      ph.advance(1);  // p2
      EXPECT_FALSE(mtx.try_lock_shared_for(TEST_TICKS));
      step += 1;
      TRACE("STEP:2(timeout)");
      ph.advance(2);  // p3-4
      break;
    case 2:
      ph.advance(1);  // p1
      ph.await();     // p2
      EXPECT_STEP_RANGE(2, 3);
      ph.advance(2);  // p3-4
      mtx.lock();
      EXPECT_STEP(6);
      mtx.unlock();
      break;
    case 3:
      ph.advance(2);  // p1-2
      ph.await();     // p3
      EXPECT_STEP(4);
      ph.advance(1);  // p4
      EXPECT_TRUE(mtx.try_lock_shared_for(TEST_NOT_TIMEOUT));
      EXPECT_STEP(7);
      mtx.unlock_shared();
      break;
    }
  });
  EXPECT_LE(TEST_TICKS * 6, sw.elapsed());
}

// phase-fair RW lock scheduling try_lock_shared_until() timeout
//
// T0/W: L=a=1=a=====a=w=5=U........
//         |  /      | |   |
// T1/R: ..w.a.s-2-*.a.a...|........
//         |  \     / /    |
// T2/W: ..a...w.3.a.a.l---L=6=U....
//         |   |   |  \        |
// T3/R: ..a...a...w.4.a.s-----S=7=V
//
//   CriticalPath = 1-3-4-5-6-7
//
//   l/L=lock(request/acquired), U=unlock()
//   s/S=try_lock_shared_until(request/acquired)
//   V=unlock_shared(), *=timeout
//   a=phase advance, w=phase await
//
TEST(FairSharedTimedMutexTest, PhaseFifoTryLockSharedUntil)
{
  yamc::test::phaser phaser(4);
  std::atomic<int> step = {0};
  yamc::fair::shared_timed_mutex mtx;
  using Clock = std::chrono::steady_clock;
  yamc::test::stopwatch<> sw;
  yamc::test::task_runner(4, [&](std::size_t id) {
    auto ph = phaser.get(id);
    switch (id) {
    case 0:
      mtx.lock();
      ph.advance(1);  // p1
      EXPECT_STEP(1);
      ph.advance(2);  // p2-3
      ph.await() ;    // p4
      EXPECT_STEP(5);
      mtx.unlock();
      break;
    case 1:
      ph.await();     // p1
      ph.advance(1);  // p2
      EXPECT_FALSE(mtx.try_lock_shared_until(Clock::now() + TEST_TICKS));
      step += 1;
      TRACE("STEP:2(timeout)");
      ph.advance(2);  // p3-4
      break;
    case 2:
      ph.advance(1);  // p1
      ph.await();     // p2
      EXPECT_STEP_RANGE(2, 3);
      ph.advance(2);  // p3-4
      mtx.lock();
      EXPECT_STEP(6);
      mtx.unlock();
      break;
    case 3:
      ph.advance(2);  // p1-2
      ph.await();     // p3
      EXPECT_STEP(4);
      ph.advance(1);  // p4
      EXPECT_TRUE(mtx.try_lock_shared_until(Clock::now() + TEST_NOT_TIMEOUT));
      EXPECT_STEP(7);
      mtx.unlock_shared();
      break;
    }
  });
  EXPECT_LE(TEST_TICKS * 6, sw.elapsed());
}
