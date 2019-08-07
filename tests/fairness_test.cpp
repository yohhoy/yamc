/*
 * fairness_test.cpp
 */
#include "gtest/gtest.h"
#include "fair_mutex.hpp"
#include "fair_shared_mutex.hpp"
#include "yamc_testutil.hpp"


#define TEST_NOT_TIMEOUT std::chrono::minutes(3)


// debug for test case
#define TESTCASE_EXPECT_FAILED 0


using FairMutexTypes = ::testing::Types<
  yamc::fair::mutex,
  yamc::fair::timed_mutex,
  yamc::fair::recursive_mutex,
  yamc::fair::recursive_timed_mutex,
  yamc::fair::basic_shared_mutex<yamc::rwlock::TaskFairness>,
  yamc::fair::basic_shared_mutex<yamc::rwlock::PhaseFairness>,
  yamc::fair::basic_shared_timed_mutex<yamc::rwlock::TaskFairness>,
  yamc::fair::basic_shared_timed_mutex<yamc::rwlock::PhaseFairness>
>;

template <typename Mutex>
struct FairMutexTest : ::testing::Test {};

TYPED_TEST_SUITE(FairMutexTest, FairMutexTypes);

// FIFO scheduling
//
// T0: T=1=a=a=====w=4=U.l-----L=7=U
//         |  \   /    |       |
// T1: ....w.2.a.a.l---L=5=U...|....
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
  SETUP_STEPTEST;
  yamc::test::phaser phaser(3);
  TypeParam mtx;
  yamc::test::stopwatch<> sw;
  yamc::test::task_runner(3, [&](std::size_t id) {
    auto ph = phaser.get(id);
    switch (id) {
    case 0:
      EXPECT_TRUE(mtx.try_lock());
      EXPECT_STEP(1);
      ph.advance(2);  // p1-2
      ph.await();     // p3
      EXPECT_STEP(4);
      mtx.unlock();
      mtx.lock();
      EXPECT_STEP(7);
      mtx.unlock();
      break;
    case 1:
      ph.await();     // p1
      EXPECT_STEP(2);
      ph.advance(2);  // p2-3
      mtx.lock();
      EXPECT_STEP(5);
      mtx.unlock();
      break;
    case 2:
      ph.await();     // p1
      EXPECT_FALSE(mtx.try_lock());
      ph.await();     // p2
      EXPECT_STEP(3);
      ph.advance(1);  // p3
      mtx.lock();
      EXPECT_STEP(6);
      mtx.unlock();
      break;
    }
  });
  EXPECT_LE(TEST_TICKS * 7, sw.elapsed());
}


using FairTimedMutexTypes = ::testing::Types<
  yamc::fair::timed_mutex,
  yamc::fair::recursive_timed_mutex,
  yamc::fair::basic_shared_timed_mutex<yamc::rwlock::TaskFairness>,
  yamc::fair::basic_shared_timed_mutex<yamc::rwlock::PhaseFairness>
>;

template <typename Mutex>
struct FairTimedMutexTest : ::testing::Test {};

TYPED_TEST_SUITE(FairTimedMutexTest, FairTimedMutexTypes);

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
  SETUP_STEPTEST;
  yamc::test::phaser phaser(3);
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
      EXPECT_STEP(3);
      mtx.unlock();
      EXPECT_TRUE(mtx.try_lock_for(TEST_NOT_TIMEOUT));
      EXPECT_STEP(6);
      mtx.unlock();
      break;
    case 1:
      ph.await();     // p1
      ph.advance(1);  // p2
      EXPECT_FALSE(mtx.try_lock_for(TEST_TICKS * 2));
      ADVANCE_STEP("STEP1-2(timeout)", 2);
      ph.advance(1);  // p3
      EXPECT_TRUE(mtx.try_lock_for(TEST_NOT_TIMEOUT));
      EXPECT_STEP(5);
      mtx.unlock();
      break;
    case 2:
      ph.advance(1);  // p1
      ph.await();     // p2
      ph.advance(1);  // p3
      EXPECT_TRUE(mtx.try_lock_for(TEST_NOT_TIMEOUT));
      EXPECT_STEP(4);
      mtx.unlock();
      break;
    }
  });
  EXPECT_LE(TEST_TICKS * 6, sw.elapsed());
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
  SETUP_STEPTEST;
  yamc::test::phaser phaser(3);
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
      EXPECT_STEP(3);
      mtx.unlock();
      EXPECT_TRUE(mtx.try_lock_until(Clock::now() + TEST_NOT_TIMEOUT));
      EXPECT_STEP(6);
      mtx.unlock();
      break;
    case 1:
      ph.await();     // p1
      ph.advance(1);  // p2
      EXPECT_FALSE(mtx.try_lock_until(Clock::now() + TEST_TICKS * 2));
      ADVANCE_STEP("STEP1-2(timeout)", 2);
      ph.advance(1);  // p3
      EXPECT_TRUE(mtx.try_lock_until(Clock::now() + TEST_NOT_TIMEOUT));
      EXPECT_STEP(5);
      mtx.unlock();
      break;
    case 2:
      ph.advance(1);  // p1
      ph.await();     // p2
      ph.advance(1);  // p3
      EXPECT_TRUE(mtx.try_lock_until(Clock::now() + TEST_NOT_TIMEOUT));
      EXPECT_STEP(4);
      mtx.unlock();
      break;
    }
  });
  EXPECT_LE(TEST_TICKS * 6, sw.elapsed());
}


using FairSharedMutexTypes = ::testing::Types<
  yamc::fair::basic_shared_mutex<yamc::rwlock::PhaseFairness>,
  yamc::fair::basic_shared_mutex<yamc::rwlock::TaskFairness>,
  yamc::fair::basic_shared_timed_mutex<yamc::rwlock::PhaseFairness>,
  yamc::fair::basic_shared_timed_mutex<yamc::rwlock::TaskFairness>
>;

template <typename Mutex>
struct FairSharedMutexTest : ::testing::Test {};

TYPED_TEST_SUITE(FairSharedMutexTest, FairSharedMutexTypes);

// fair RW lock FIFO scheduling
//
// T0/R: S=a=1=a=2=a=3=V............
//         |   |   |   |
// T1/W: ..w.l-|---|---L=4=U..,.....
//         |   |   |       |
// T2/R: ..a...w.s-|-------S=5=V....
//         |  /    |           |
// T3/W: ..a.a.....w.l---------L=6=U
//
//   CriticalPath = 1-2-3-4-5-6
//
//   l/L=lock(request/acquired), U=unlock()
//   s/S=lock_shared(request/acquired), V=unlock_shared()
//   a=phase advance, w=phase await
//
TYPED_TEST(FairSharedMutexTest, FifoSched)
{
  SETUP_STEPTEST;
  yamc::test::phaser phaser(4);
  TypeParam mtx;
  yamc::test::stopwatch<> sw;
  yamc::test::task_runner(4, [&](std::size_t id) {
    auto ph = phaser.get(id);
    switch (id) {
    case 0:
      mtx.lock_shared();
      ph.advance(1);  // p1
      EXPECT_STEP(1);
      ph.advance(1);  // p2
      EXPECT_STEP(2);
      ph.advance(1);  // p3
      EXPECT_STEP(3);
      mtx.unlock_shared();
      break;
    case 1:
      ph.await();     // p1
      ph.advance(2);  // p2-3
      mtx.lock();
      EXPECT_STEP(4);
      mtx.unlock();
      break;
    case 2:
      ph.advance(1);  // p1
      ph.await();     // p2
      ph.advance(1);  // p3
      mtx.lock_shared();
      EXPECT_STEP(5);
      mtx.unlock_shared();
      break;
    case 3:
      ph.advance(2);  // p1-2
      ph.await();     // p3
      mtx.lock();
      EXPECT_STEP(6);
      mtx.unlock();
      break;
    }
  });
  EXPECT_LE(TEST_TICKS * 6, sw.elapsed());
}


using TaskFairSharedMutexTypes = ::testing::Types<
#if TESTCASE_EXPECT_FAILED
  yamc::fair::basic_shared_mutex<yamc::rwlock::PhaseFairness>,
  yamc::fair::basic_shared_timed_mutex<yamc::rwlock::PhaseFairness>,
#endif
  yamc::fair::basic_shared_mutex<yamc::rwlock::TaskFairness>,
  yamc::fair::basic_shared_timed_mutex<yamc::rwlock::TaskFairness>
>;

template <typename Mutex>
struct TaskFairSharedMutexTest : ::testing::Test {};

TYPED_TEST_SUITE(TaskFairSharedMutexTest, TaskFairSharedMutexTypes);

// task-fairness RW lock scheduling
//
// T0/W: L=a=1=a=2=a=3=a=U..................
//         |   |   |    \|----\
// T1/R: ..w.s-|---|-----S=====w=5=V........
//         |   |   |     |     |   |
// T2/R: ..w.s-|---|-----S=4=V.w.s-|---S=7=V
//         |   |   |               |   |
// T3/W: ..a...w.l-|---------------L=6=U....
//         |  /    |                   |
// T4/R: ..a.a.....w.s-----------------S=8=V
//
//   CriticalPath = 1-2-3-4-5-6-{7|8}
//
//   l/L=lock(request/acquired), U=unlock()
//   s/S=lock_shared(request/acquired), V=unlock_shared()
//   a=phase advance, w=phase await
//
TYPED_TEST(TaskFairSharedMutexTest, TaskFifoSched)
{
  SETUP_STEPTEST;
  yamc::test::phaser phaser(5);
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
      ph.advance(1);  // p4
      mtx.unlock();
      break;
    case 1:
      ph.await();     // p1
      ph.advance(2);  // p2-3
      mtx.lock_shared();
      ph.await();     // p4
      EXPECT_STEP(5);
      mtx.unlock_shared();
      break;
    case 2:
      ph.await();     // p1
      ph.advance(2);  // p2-3
      mtx.lock_shared();
      EXPECT_STEP(4);
      mtx.unlock_shared();
      ph.await();     // p4
      mtx.lock_shared();
      EXPECT_STEP_RANGE(7, 8);
      mtx.unlock_shared();
      break;
    case 3:
      ph.advance(1);  // p1
      ph.await();     // p2
      ph.advance(2);  // p3-4
      mtx.lock();
      EXPECT_STEP(6);
      mtx.unlock();
      break;
    case 4:
      ph.advance(2);  // p1-2
      ph.await();     // p3
      ph.advance(1);  // p4
      mtx.lock_shared();
      EXPECT_STEP_RANGE(7, 8);
      mtx.unlock_shared();
      break;
    }
  });
  EXPECT_LE(TEST_TICKS * 7, sw.elapsed());
}


using PhaseFairSharedMutexTypes = ::testing::Types<
#if TESTCASE_EXPECT_FAILED
  yamc::fair::basic_shared_mutex<yamc::rwlock::TaskFairness>,
  yamc::fair::basic_shared_timed_mutex<yamc::rwlock::TaskFairness>,
#endif
  yamc::fair::basic_shared_mutex<yamc::rwlock::PhaseFairness>,
  yamc::fair::basic_shared_timed_mutex<yamc::rwlock::PhaseFairness>
>;

template <typename Mutex>
struct PhaseFairSharedMutexTest : ::testing::Test {};

TYPED_TEST_SUITE(PhaseFairSharedMutexTest, PhaseFairSharedMutexTypes);

// phase-fair RW lock scheduling
//
// T0/W: L=a=1=a=2=a=3=a=U..................
//         |   |   |    \|----\
// T1/R: ..w.s-|---|-----S=====w=6=V........
//         |   |   |     |     |   |
// T2/R: ..w.s-|---|-----S=4=V.w.s-|---S=8=V
//         |   |   |     |         |   |
// T3/W: ..a...w.l-|-----|---------L=7=U....
//         |  /    |     |
// T4/R: ..a.a.....w.s---S=5=V..............
//
//   CriticalPath = 1-2-3-4-6-7-8
//
//   l/L=lock(request/acquired), U=unlock()
//   s/S=lock_shared(request/acquired), V=unlock_shared()
//   a=phase advance, w=phase await
//
TYPED_TEST(PhaseFairSharedMutexTest, PhaseFifoSched)
{
  SETUP_STEPTEST;
  yamc::test::phaser phaser(5);
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
      ph.advance(1);  // p4
      mtx.unlock();
      break;
    case 1:
      ph.await();     // p1
      ph.advance(2);  // p2-3
      mtx.lock_shared();
      ph.await();     // p4
      EXPECT_STEP(6);
      mtx.unlock_shared();
      break;
    case 2:
      ph.await();     // p1
      ph.advance(2);  // p2-3
      mtx.lock_shared();
      EXPECT_STEP_RANGE(4, 5);
      mtx.unlock_shared();
      ph.await();     // p4
      mtx.lock_shared();
      EXPECT_STEP(8);
      mtx.unlock_shared();
      break;
    case 3:
      ph.advance(1);  // p1
      ph.await();     // p2
      ph.advance(2);  // p3-4
      mtx.lock();
      EXPECT_STEP(7);
      mtx.unlock();
      break;
    case 4:
      ph.advance(2);  // p1-2
      ph.await();     // p3
      ph.advance(1);  // p4
      mtx.lock_shared();
      EXPECT_STEP_RANGE(4, 5);
      mtx.unlock_shared();
      break;
    }
  });
  EXPECT_LE(TEST_TICKS * 7, sw.elapsed());
}


using FairSharedTimedMutexTypes = ::testing::Types<
  yamc::fair::basic_shared_timed_mutex<yamc::rwlock::PhaseFairness>,
  yamc::fair::basic_shared_timed_mutex<yamc::rwlock::TaskFairness>
>;

template <typename Mutex>
struct FairSharedTimedMutexTest : ::testing::Test {};

TYPED_TEST_SUITE(FairSharedTimedMutexTest, FairSharedTimedMutexTypes);

// task-fair RW lock scheduling try_lock_for() timeout
//
// T0/W: T=a=1=a=2=a=3=a=U..........
//         |   |   |    \|----\
// T1/R: ..w.s-|---|-----S=====w=7=V
//         |   |   |     |     |
// T2/R: ..w.s-|---|-----S=6=V.a....
//         |   |   |           |
// T3/W: ..a...w.t-|-----4-5-*.a....
//         |  /    |         | |
// T4/R: ..a.a.....w.s-------S=w=8=V
//
//   CriticalPath = 1-2-{3-6|4-5}-{7|8}
//
//   t/T=try_lock_for(request/acquired), U=unlock(), *=timeout
//   s/S=lock_shared(request/acquired), V=unlock_shared()
//   a=phase advance, w=phase await
//
TYPED_TEST(FairSharedTimedMutexTest, FifoTryLockFor)
{
  SETUP_STEPTEST;
  yamc::test::phaser phaser(5);
  TypeParam mtx;
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
      ph.advance(1);  // p4
      mtx.unlock();
      break;
    case 1:
      ph.await();     // p1
      ph.advance(2);  // p2-3
      mtx.lock_shared();
      ph.await();     // p4
      EXPECT_STEP_RANGE(7, 8);
      mtx.unlock_shared();
      break;
    case 2:
      ph.await();     // p1
      ph.advance(2);  // p2-3
      mtx.lock_shared();
      EXPECT_STEP_RANGE(4, 6);
      mtx.unlock_shared();
      ph.advance(1);  // p4
      break;
    case 3:
      ph.advance(1);  // p1
      ph.await();     // p2
      ph.advance(1);  // p3
      EXPECT_FALSE(mtx.try_lock_for(TEST_TICKS * 2));
      ADVANCE_STEP("STEP4-5(timeout)", 2);
      ph.advance(1);  // p4
      break;
    case 4:
      ph.advance(2);  // p1-2
      ph.await();     // p3
      mtx.lock_shared();
      ph.await();     // p4
      EXPECT_STEP_RANGE(7, 8);
      mtx.unlock_shared();
      break;
    }
  });
  EXPECT_LE(TEST_TICKS * 5, sw.elapsed());
}

// fair RW lock scheduling try_lock_until() timeout
//
// T0/W: T=a=1=a=2=a=3=a=U..........
//         |   |   |    \|----\
// T1/R: ..w.s-|---|-----S=====w=7=V
//         |   |   |     |     |
// T2/R: ..w.s-|---|-----S=6=V.a....
//         |   |   |           |
// T3/W: ..a...w.t-|-----4-5-*.a....
//         |  /    |         | |
// T4/R: ..a.a.....w.s-------S=w=8=V
//
//   CriticalPath = 1-2-{3-6|4-5}-{7|8}
//
//   t/T=try_lock_until(request/acquired), U=unlock(), *=timeout
//   s/S=lock_shared(request/acquired), V=unlock_shared()
//   a=phase advance, w=phase await
//
TYPED_TEST(FairSharedTimedMutexTest, FifoTryLockUntil)
{
  SETUP_STEPTEST;
  yamc::test::phaser phaser(5);
  TypeParam mtx;
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
      ph.advance(1);  // p4
      mtx.unlock();
      break;
    case 1:
      ph.await();     // p1
      ph.advance(2);  // p2-3
      mtx.lock_shared();
      ph.await();     // p4
      EXPECT_STEP_RANGE(7, 8);
      mtx.unlock_shared();
      break;
    case 2:
      ph.await();     // p1
      ph.advance(2);  // p2-3
      mtx.lock_shared();
      EXPECT_STEP_RANGE(4, 6);
      mtx.unlock_shared();
      ph.advance(1);  // p4
      break;
    case 3:
      ph.advance(1);  // p1
      ph.await();     // p2
      ph.advance(1);  // p3
      EXPECT_FALSE(mtx.try_lock_until(Clock::now() + TEST_TICKS * 2));
      ADVANCE_STEP("STEP4-5(timeout)", 2);
      ph.advance(1);  // p4
      break;
    case 4:
      ph.advance(2);  // p1-2
      ph.await();     // p3
      mtx.lock_shared();
      ph.await();     // p4
      EXPECT_STEP_RANGE(7, 8);
      mtx.unlock_shared();
      break;
    }
  });
  EXPECT_LE(TEST_TICKS * 5, sw.elapsed());
}

// fair RW lock scheduling try_lock_shared_for() timeout
//
// T0/W: L=a=1=a=====a===w=5=U........
//         |  /      |  /    |
// T1/R: ..w.a.s-2-*.a.a.....|........
//         |  \      | |     |
// T2/W: ..a...w.3...a.a.l---L=6=U....
//         |   |     |  \        |
// T3/R: ..a...a.....w.4.a.s-----S=7=V
//
//   CriticalPath = 1-3-4-5-6-7
//
//   l/L=lock(request/acquired), U=unlock()
//   s/S=try_lock_shared_for(request/acquired)
//   V=unlock_shared(), *=timeout
//   a=phase advance, w=phase await
//
TYPED_TEST(FairSharedTimedMutexTest, FifoTryLockSharedFor)
{
  SETUP_STEPTEST;
  yamc::test::phaser phaser(4);
  TypeParam mtx;
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
      ADVANCE_STEP("STEP2(timeout)", 1);
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

// fair RW lock scheduling try_lock_shared_until() timeout
//
// T0/W: L=a=1=a=====a===w=5=U........
//         |  /      |  /    |
// T1/R: ..w.a.s-2-*.a.a.....|........
//         |  \      | |     |
// T2/W: ..a...w.3...a.a.l---L=6=U....
//         |   |     |  \       |
// T3/R: ..a...a.....w.4.a.s-----S=7=V
//
//   CriticalPath = 1-3-4-5-6-7
//
//   l/L=lock(request/acquired), U=unlock()
//   s/S=try_lock_shared_until(request/acquired)
//   V=unlock_shared(), *=timeout
//   a=phase advance, w=phase await
//
TYPED_TEST(FairSharedTimedMutexTest, FifoTryLockSharedUntil)
{
  SETUP_STEPTEST;
  yamc::test::phaser phaser(4);
  TypeParam mtx;
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
      ADVANCE_STEP("STEP2(timeout)", 1);
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
