/*
 * latch_test.cpp
 */
#include "gtest/gtest.h"
#include "yamc_latch.hpp"
#include "yamc_testutil.hpp"


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
  SETUP_STEPTEST;
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
  SETUP_STEPTEST;
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

// latch::max()
TEST(LatchTest, Max)
{
  EXPECT_GT((yamc::latch::max)(), 0);
}
