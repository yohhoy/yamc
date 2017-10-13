/*
 * deadlock_test.cpp
 *
 * test configuration:
 *   - YAMC_CHECKED_CALL_ABORT=0  throw std::system_error [default]
 *   - YAMC_CHECKED_CALL_ABORT=1  call std::abort()
 */
#include <chrono>
#include <system_error>
#include "gtest/gtest.h"
#include "checked_mutex.hpp"
#include "checked_shared_mutex.hpp"
#include "yamc_testutil.hpp"


#if YAMC_CHECKED_CALL_ABORT
// call std::abort() on check failure
#if TEST_PLATFORM_WINDOWS
#define EXPECT_CHECK_FAILURE(statement_) EXPECT_EXIT(statement_, ::testing::ExitedWithCode(3), "")
#else
#define EXPECT_CHECK_FAILURE(statement_) EXPECT_EXIT(statement_, ::testing::KilledBySignal(SIGABRT), "")
#endif
#define EXPECT_CHECK_FAILURE_OUTER(block_) EXPECT_CHECK_FAILURE(block_)
#define EXPECT_CHECK_FAILURE_INNER(block_) block_

#else
// throw an exception on check failure
#define EXPECT_CHECK_FAILURE(statement_) EXPECT_THROW(statement_, std::system_error)
#define EXPECT_CHECK_FAILURE_OUTER(block_) block_
#define EXPECT_CHECK_FAILURE_INNER(block_) EXPECT_CHECK_FAILURE(block_)

#endif


#define TEST_NOT_TIMEOUT std::chrono::minutes(3)

#define TEST_TICKS std::chrono::milliseconds(200)
#define WAIT_TICKS std::this_thread::sleep_for(TEST_TICKS)


using CheckedMutexTypes = ::testing::Types<
  yamc::checked::mutex,
  yamc::checked::timed_mutex,
  yamc::checked::recursive_mutex,
  yamc::checked::recursive_timed_mutex
>;

template <typename Mutex>
struct CheckedMutexTest : ::testing::Test {};

TYPED_TEST_CASE(CheckedMutexTest, CheckedMutexTypes);

// deadlock with 2-threads/2-mutex
TYPED_TEST(CheckedMutexTest, BasicDeadlock)
{
  auto test_body = []{
    yamc::test::barrier step(2);
    TypeParam mtx1;
    TypeParam mtx2;
    yamc::test::task_runner(2, [&](std::size_t id) {
      switch (id) {
      case 0:
        ASSERT_NO_THROW(mtx1.lock());
        step.await();
        EXPECT_NO_THROW(mtx2.lock());
        EXPECT_NO_THROW(mtx2.unlock());
        EXPECT_NO_THROW(mtx1.unlock());
        break;
      case 1:
        ASSERT_NO_THROW(mtx2.lock());
        step.await();
        WAIT_TICKS;
        EXPECT_CHECK_FAILURE_INNER({
          mtx1.lock();
        });
        EXPECT_NO_THROW(mtx2.unlock());
        break;
      }
    });
  };
  EXPECT_CHECK_FAILURE_OUTER(test_body());
}

// cyclic deadlock with 3-threads/3-mutex
TYPED_TEST(CheckedMutexTest, CyclicDeadlock)
{
  auto test_body = []{
    yamc::test::barrier step(3);
    TypeParam mtx1;
    TypeParam mtx2;
    TypeParam mtx3;
    yamc::test::task_runner(3, [&](std::size_t id) {
      switch (id) {
      case 0:
        ASSERT_NO_THROW(mtx1.lock());
        step.await();
        EXPECT_NO_THROW(mtx2.lock());
        EXPECT_NO_THROW(mtx2.unlock());
        EXPECT_NO_THROW(mtx1.unlock());
        break;
      case 1:
        ASSERT_NO_THROW(mtx2.lock());
        step.await();
        EXPECT_NO_THROW(mtx3.lock());
        EXPECT_NO_THROW(mtx3.unlock());
        EXPECT_NO_THROW(mtx2.unlock());
        break;
      case 2:
        ASSERT_NO_THROW(mtx3.lock());
        step.await();
        WAIT_TICKS;
        EXPECT_CHECK_FAILURE_INNER({
          mtx1.lock();
        });
        EXPECT_NO_THROW(mtx3.unlock());
        break;
      }
    });
  };
  EXPECT_CHECK_FAILURE_OUTER(test_body());
}

// deadlock with try_lock
TYPED_TEST(CheckedMutexTest, TryLockDeadlock)
{
  auto test_body = []{
    yamc::test::barrier step(2);
    TypeParam mtx1;
    TypeParam mtx2;
    yamc::test::task_runner(2, [&](std::size_t id) {
      switch (id) {
      case 0:
        ASSERT_TRUE(mtx1.try_lock());
        step.await();
        EXPECT_NO_THROW(mtx2.lock());
        EXPECT_NO_THROW(mtx2.unlock());
        EXPECT_NO_THROW(mtx1.unlock());
        break;
      case 1:
        ASSERT_TRUE(mtx2.try_lock());
        step.await();
        WAIT_TICKS;
        EXPECT_CHECK_FAILURE_INNER({
          mtx1.lock();
        });
        EXPECT_NO_THROW(mtx2.unlock());
        break;
      }
    });
  };
  EXPECT_CHECK_FAILURE_OUTER(test_body());
}


using CheckedTimedMutexTypes = ::testing::Types<
  yamc::checked::timed_mutex,
  yamc::checked::recursive_timed_mutex
>;

template <typename Mutex>
struct CheckedTimedMutexTest : ::testing::Test {};

TYPED_TEST_CASE(CheckedTimedMutexTest, CheckedTimedMutexTypes);

// deadlock with try_lock_for
TYPED_TEST(CheckedTimedMutexTest, TryLockForDeadlock)
{
  auto test_body = []{
    yamc::test::barrier step(2);
    TypeParam mtx1;
    TypeParam mtx2;
    yamc::test::task_runner(2, [&](std::size_t id) {
      switch (id) {
      case 0:
        ASSERT_TRUE(mtx1.try_lock_for(TEST_NOT_TIMEOUT));
        step.await();
        EXPECT_NO_THROW(mtx2.lock());
        EXPECT_NO_THROW(mtx2.unlock());
        EXPECT_NO_THROW(mtx1.unlock());
        break;
      case 1:
        ASSERT_TRUE(mtx2.try_lock_for(TEST_NOT_TIMEOUT));
        step.await();
        WAIT_TICKS;
        EXPECT_CHECK_FAILURE_INNER({
          mtx1.lock();
        });
        EXPECT_NO_THROW(mtx2.unlock());
        break;
      }
    });
  };
  EXPECT_CHECK_FAILURE_OUTER(test_body());
}

// deadlock with try_lock_until
TYPED_TEST(CheckedTimedMutexTest, TryLockUntilDeadlock)
{
  auto test_body = []{
    yamc::test::barrier step(2);
    TypeParam mtx1;
    TypeParam mtx2;
    yamc::test::task_runner(2, [&](std::size_t id) {
      switch (id) {
      case 0:
        ASSERT_TRUE(mtx1.try_lock_until(std::chrono::system_clock::now() + TEST_NOT_TIMEOUT));
        step.await();
        EXPECT_NO_THROW(mtx2.lock());
        EXPECT_NO_THROW(mtx2.unlock());
        EXPECT_NO_THROW(mtx1.unlock());
        break;
      case 1:
        ASSERT_TRUE(mtx2.try_lock_until(std::chrono::system_clock::now() + TEST_NOT_TIMEOUT));
        step.await();
        WAIT_TICKS;
        EXPECT_CHECK_FAILURE_INNER({
          mtx1.lock();
        });
        EXPECT_NO_THROW(mtx2.unlock());
        break;
      }
    });
  };
  EXPECT_CHECK_FAILURE_OUTER(test_body());
}
