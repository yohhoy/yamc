/*
 * barrier_test.cpp
 */
#include <type_traits>
#include "gtest/gtest.h"
#include "yamc_barrier.hpp"
#include "yamc_testutil.hpp"


struct null_completion {
  void operator()() {}
};

struct counting_completion {
  int *counter;
  void operator()() {
    ++*counter;
  }
};


// type requirements (compile-time assertion)
TEST(BarrierTest, TypeRequirements)
{
  using arrival_token = yamc::barrier<>::arrival_token;
  static_assert(std::is_move_constructible<arrival_token>::value, "Cpp17MoveConstructible");
  static_assert(std::is_move_assignable<arrival_token>::value, "Cpp17MoveAssignable");
  static_assert(std::is_destructible<arrival_token>::value, "Cpp17Destructible");
}

// barrier constructor
TEST(BarrierTest, Ctor)
{
  EXPECT_NO_THROW(yamc::barrier<>{1});
}

// barrier construction with completion
TEST(BarrierTest, CtorCompletion)
{
  EXPECT_NO_THROW(yamc::barrier<null_completion>{1});
}

// barrier construction with completion argument
TEST(BarrierTest, CtorCompletionArg)
{
  null_completion completion;
  EXPECT_NO_THROW((yamc::barrier<null_completion>{1, completion}));
}

// barrier constructor throws exception
TEST(BarrierTest, CtorThrow)
{
  struct throwing_completion {
    throwing_completion() = default;
    throwing_completion(const throwing_completion&) = default;
    throwing_completion(throwing_completion&&) {
      // move constructor throw exception
      throw 42;  // int
    }
    void operator()() {}
  } completion;
  EXPECT_THROW((yamc::barrier<throwing_completion>{1, completion}), int);
}

// barrier::arrive()
TEST(BarrierTest, Arrive)
{
  yamc::barrier<> barrier{3};  // expected count=3
  EXPECT_NO_THROW((void)barrier.arrive());   // c=3->2
  EXPECT_NO_THROW((void)barrier.arrive(2));  // c=2->0, next phase
  EXPECT_NO_THROW((void)barrier.arrive(3));  // c=3->0, next phase
  // casting void to suppress "ignoring return value" warning
}

// barrier::arrive() with completion
TEST(BarrierTest, ArriveCompletion)
{
  int counter = 0;
  counting_completion complation{&counter};
  yamc::barrier<counting_completion> barrier{2, complation};  // expected count=2
  EXPECT_NO_THROW((void)barrier.arrive());   // c=2->1
  EXPECT_EQ(counter, 0);
  EXPECT_NO_THROW((void)barrier.arrive());   // c=1->0, call completion
  EXPECT_EQ(counter, 1);
  EXPECT_NO_THROW((void)barrier.arrive(2));  // c=2->0, call completion
  EXPECT_EQ(counter, 2);
}

// barrier::wait()
TEST(BarrierTest, Wait)
{
  yamc::barrier<> barrier{1};
  EXPECT_NO_THROW(barrier.wait(barrier.arrive()));
  auto token = barrier.arrive();
  EXPECT_NO_THROW(barrier.wait(std::move(token)));
}

// barrier::arrive_and_wait()
TEST(BarrierTest, ArriveAndWait)
{
  yamc::barrier<> barrier{1};
  EXPECT_NO_THROW(barrier.arrive_and_wait());
  EXPECT_NO_THROW(barrier.arrive_and_wait());
}

// barrier::arrive_and_wait() with completion
TEST(BarrierTest, ArriveAndWaitCompletion)
{
  int counter = 0;
  counting_completion complation{&counter};
  yamc::barrier<counting_completion> barrier{1, complation};
  EXPECT_NO_THROW(barrier.arrive_and_wait());
  EXPECT_EQ(counter, 1);
  EXPECT_NO_THROW(barrier.arrive_and_wait());
  EXPECT_EQ(counter, 2);
}

// barrier::arrive_and_drop()
TEST(BarrierTest, ArriveAndDrop)
{
  yamc::barrier<> barrier{1};
  EXPECT_NO_THROW(barrier.arrive_and_drop());
}

// barrier::arrive_and_drop() with completion
TEST(BarrierTest, ArriveAndDropCompletion)
{
  int counter = 0;
  counting_completion complation{&counter};
  yamc::barrier<counting_completion> barrier{1, complation};
  EXPECT_NO_THROW(barrier.arrive_and_drop());
  EXPECT_EQ(counter, 1);
}

// basic phasing
//
// T0: 1.X...X...X.4
//       |   |   |
// T1: ..X.2.X...X.5
//       |   |   |
// T2: ..X...X.3.X.6
//
//   CriticalPath = 1-2-3-{4|5|6}
//
//   X=arrive_and_wait()
//
TEST(BarrierTest, BasicPhasing)
{
  SETUP_STEPTEST;
  yamc::barrier<> barrier{3};
  yamc::test::task_runner(
    3,
    [&](std::size_t id) {
      switch (id) {
      case 0:
        EXPECT_STEP(1);
        EXPECT_NO_THROW(barrier.arrive_and_wait());
        EXPECT_NO_THROW(barrier.arrive_and_wait());
        EXPECT_NO_THROW(barrier.arrive_and_wait());
        EXPECT_STEP_RANGE(4, 6);
        break;
      case 1:
        EXPECT_NO_THROW(barrier.arrive_and_wait());
        EXPECT_STEP(2);
        EXPECT_NO_THROW(barrier.arrive_and_wait());
        EXPECT_NO_THROW(barrier.arrive_and_wait());
        EXPECT_STEP_RANGE(4, 6);
        break;
      case 2:
        EXPECT_NO_THROW(barrier.arrive_and_wait());
        EXPECT_NO_THROW(barrier.arrive_and_wait());
        EXPECT_STEP(3);
        EXPECT_NO_THROW(barrier.arrive_and_wait());
        EXPECT_STEP_RANGE(4, 6);
        break;
      }
    }
  );
}

// arrive+wait phasing
//
// T0: 1.A.A-W-3
//       |   |
// T1: A-W.2.A.4
//
//   CriticalPath = 1-2-{3|4}
//
//   A=arrive(), W=wait()
//
TEST(BarrierTest, ArriveWaitPhasing)
{
  SETUP_STEPTEST;
  yamc::barrier<> barrier{2};
  yamc::test::task_runner(
    2,
    [&](std::size_t id) {
      switch (id) {
      case 0:
      {
        EXPECT_STEP(1);
        EXPECT_NO_THROW((void)barrier.arrive());     // phase=0->1
        auto token = barrier.arrive();               // phase=1->2
        EXPECT_NO_THROW(barrier.wait(std::move(token)));
        EXPECT_STEP_RANGE(3, 4);
        break;
      }
      case 1:
      {
        auto token = barrier.arrive();               // phase=0->1
        EXPECT_NO_THROW(barrier.wait(std::move(token)));
        EXPECT_STEP(2);
        EXPECT_NO_THROW((void)barrier.arrive());     // phase=1->2
        EXPECT_STEP_RANGE(3, 4);
        break;
      }
      }
    }
  );
}

// past token
//
// T0: 1.A---X-W.3
//       |   |
// T1: ..X.2.X....
//
//   CriticalPath = 1-2-3
//
//   A=arrive(), W=wait()
//   X=arrive_and_wait()
//
TEST(BarrierTest, PastToken)
{
  SETUP_STEPTEST;
  yamc::barrier<> barrier{2};
  yamc::test::task_runner(
    2,
    [&](std::size_t id) {
      switch (id) {
      case 0:
      {
        EXPECT_STEP(1);
        auto token = barrier.arrive();
        EXPECT_NO_THROW(barrier.arrive_and_wait());
        EXPECT_NO_THROW(barrier.wait(std::move(token)));  // past token
        EXPECT_STEP(3);
        break;
      }
      case 1:
        EXPECT_NO_THROW(barrier.arrive_and_wait());
        EXPECT_STEP(2);
        EXPECT_NO_THROW(barrier.arrive_and_wait());
        break;
      }
    }
  );
}

// phasing with drop
//
// T0: 1.D.2........
//       |
// T1: ..X.3.X...D.5
//       |   |   |
// T2: ..X...X.4.X.6
//
//   CriticalPath = 1-{2|3}-4-{5|6}
//
//   X=arrive_and_wait(), D=arrive_and_drop()
//
TEST(BarrierTest, DropPhasing)
{
  SETUP_STEPTEST;
  yamc::barrier<> barrier{3};
  yamc::test::task_runner(
    3,
    [&](std::size_t id) {
      switch (id) {
      case 0:
        EXPECT_STEP(1);
        EXPECT_NO_THROW(barrier.arrive_and_drop());
        EXPECT_STEP_RANGE(2, 3);
        break;
      case 1:
        EXPECT_NO_THROW(barrier.arrive_and_wait());
        EXPECT_STEP_RANGE(2, 3);
        EXPECT_NO_THROW(barrier.arrive_and_wait());
        EXPECT_NO_THROW(barrier.arrive_and_drop());
        EXPECT_STEP_RANGE(5, 6);
        break;
      case 2:
        EXPECT_NO_THROW(barrier.arrive_and_wait());
        EXPECT_NO_THROW(barrier.arrive_and_wait());
        EXPECT_STEP(4);
        EXPECT_NO_THROW(barrier.arrive_and_wait());
        EXPECT_STEP_RANGE(5, 6);
        break;
      }
    }
  );
}

// barrier::max()
TEST(BarrierTest, Max)
{
  EXPECT_GT((yamc::barrier<>::max)(), 0);
}
