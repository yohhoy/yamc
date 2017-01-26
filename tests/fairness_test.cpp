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


#define TEST_TICKS std::chrono::milliseconds(200)

#define EXPECT_STEP(n_) \
  { EXPECT_EQ(n_, ++step); std::this_thread::sleep_for(TEST_TICKS); }


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


using FairMutexTypes = ::testing::Types<
  yamc::fair::mutex,
  yamc::fair::recursive_mutex
>;

template <typename Mutex>
struct FairMutexTest : ::testing::Test {};

TYPED_TEST_CASE(FairMutexTest, FairMutexTypes);

// FIFO scheduling
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
      TRACE("request... L1");
      mtx.lock();                 // 1) acquire 1st lock
      TRACE("acquire L1");
      EXPECT_STEP(1)
      ph.advance(2);
      ph.await();
      EXPECT_STEP(4)
      mtx.unlock();               // 4) unlock
      TRACE("release L1");
      break;
    case 1:
      ph.await();
      EXPECT_STEP(2)
      ph.advance(2);
      TRACE("request... L2");
      mtx.lock();                 // 2) request 2nd lock
      TRACE("acquire L2");
      EXPECT_STEP(5)              // 5) acquire 2nd lock (before 3rd lock)
      mtx.unlock();               // 6) unlock
      TRACE("release L2");
      break;
    case 2:
      ph.await();
      ph.await();
      EXPECT_STEP(3)
      ph.advance(1);
      TRACE("request... L3");
      mtx.lock();                 // 3) request 3rd lock
      TRACE("acquire L3");
      EXPECT_STEP(6)              // 7) acquire 3rd lock (after 2nd unlock)
      mtx.unlock();
      TRACE("release L3");
      break;
    }
  });
  ASSERT_LE(TEST_TICKS * step, sw.elapsed());
}
