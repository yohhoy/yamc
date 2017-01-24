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


#define WAIT_TICK \
  std::this_thread::sleep_for(std::chrono::milliseconds(200))


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
  yamc::test::join_thread thd0([&]{
    auto ph = phaser.get(0);
    WAIT_TICK;
    TRACE("request... L1");
    mtx.lock();                 // 1) acquire 1st lock
    TRACE("acquire L1");
    WAIT_TICK;
    ph.advance(2);
    WAIT_TICK;
    ph.await();
    WAIT_TICK;
    EXPECT_EQ(1, ++step);
    mtx.unlock();               // 4) unlock
    TRACE("release L1");
  });
  yamc::test::join_thread thd1([&]{
    auto ph = phaser.get(1);
    WAIT_TICK;
    ph.await();
    WAIT_TICK;
    ph.advance(2);
    WAIT_TICK;
    TRACE("request... L2");
    mtx.lock();                 // 2) request 2nd lock
    TRACE("acquire L2");
    EXPECT_EQ(2, ++step);       // 5) acquire 2nd lock (before 3rd lock)
    WAIT_TICK;
    mtx.unlock();               // 6) unlock
    TRACE("release L2");
  });
  {
    auto ph = phaser.get(2);
    WAIT_TICK;
    ph.await();
    WAIT_TICK;
    ph.await();
    WAIT_TICK;
    ph.advance(1);
    WAIT_TICK;
    TRACE("request... L3");
    mtx.lock();                 // 3) request 3rd lock
    TRACE("acquire L3");
    EXPECT_EQ(3, ++step);       // 7) acquire 3rd lock
    WAIT_TICK;
    mtx.unlock();
    TRACE("release L3");
  }
}
