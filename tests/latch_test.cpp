/*
 * latch_test.cpp
 */
#include <atomic>
#include "gtest/gtest.h"
#include "yamc_latch.hpp"
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

#define TEST_TICKS std::chrono::milliseconds(200)
#define WAIT_TICKS std::this_thread::sleep_for(TEST_TICKS)

#define EXPECT_STEP(n_) \
  { TRACE("STEP"#n_); EXPECT_EQ(n_, ++step); std::this_thread::sleep_for(TEST_TICKS); }
#define EXPECT_STEP_RANGE(r0_, r1_) \
  { TRACE("STEP"#r0_"-"#r1_); int s = ++step; EXPECT_TRUE(r0_ <= s && s <= r1_); WAIT_TICKS; }


// latch constructor
TEST(LatchTest, Ctor)
{
  EXPECT_NO_THROW(yamc::latch{1});
}

// latch::count_down()
TEST(LatchTest, CountDown)
{
  yamc::latch latch{1};
  EXPECT_NO_THROW(latch.count_down());
}

// latch::count_down(0)
TEST(LatchTest, CountDownZero)
{
  yamc::latch latch{1};
  EXPECT_NO_THROW(latch.count_down(0));
}

// latch::try_wait()
TEST(LatchTest, TryWait)
{
  yamc::latch latch{0};
  EXPECT_TRUE(latch.try_wait());
}

// latch::try_wait() failure
TEST(LatchTest, TryWaitFail)
{
  yamc::latch latch{1};
  EXPECT_FALSE(latch.try_wait());
}

// latch::wait()
TEST(LatchTest, Wait)
{
  std::atomic<int> step = {};
  yamc::latch latch{1};
  // signal-thread
  yamc::test::join_thread thd([&]{
    EXPECT_STEP(1);
    EXPECT_NO_THROW(latch.count_down(0));  // no signal
    EXPECT_STEP(2);
    EXPECT_NO_THROW(latch.count_down());
  });
  // wait-thread
  {
    EXPECT_NO_THROW(latch.wait());
    EXPECT_STEP(3);
  }
}

// latch::arrive_and_wait
TEST(LatchTest, ArriveAndWait)
{
  std::atomic<int> step = {};
  yamc::latch latch{3};  // counter=3
  // signal-thread
  yamc::test::join_thread thd([&]{
    EXPECT_STEP(1);
    EXPECT_NO_THROW(latch.arrive_and_wait(2));  // update=2
    EXPECT_STEP_RANGE(2, 3);
  });
  // wait-thread
  {
    EXPECT_NO_THROW(latch.arrive_and_wait());   // update=1
    EXPECT_STEP_RANGE(2, 3);
  }
}
