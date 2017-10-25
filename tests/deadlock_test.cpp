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
  yamc::checked::recursive_timed_mutex,
  yamc::checked::shared_mutex,
  yamc::checked::shared_timed_mutex
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

// simultaneous deadlocks with multiple thread-pairs
TYPED_TEST(CheckedMutexTest, SimultaneousDeadlock)
{
  auto test_body = []{
    constexpr size_t NPAIRS = 4;
    yamc::test::barrier step(NPAIRS * 2);
    TypeParam mtx[NPAIRS * 2];
    yamc::test::task_runner(NPAIRS * 2, [&](std::size_t id) {
      TypeParam& mtx1 = mtx[id % NPAIRS];
      TypeParam& mtx2 = mtx[id % NPAIRS + NPAIRS];
      if (id < NPAIRS) {
        ASSERT_NO_THROW(mtx1.lock());
        step.await();
        EXPECT_NO_THROW(mtx2.lock());
        EXPECT_NO_THROW(mtx2.unlock());
        EXPECT_NO_THROW(mtx1.unlock());
      } else {
        ASSERT_NO_THROW(mtx2.lock());
        step.await();
        WAIT_TICKS;
        EXPECT_CHECK_FAILURE_INNER({
          mtx1.lock();
        });
        EXPECT_NO_THROW(mtx2.unlock());
      }
    });
  };
  EXPECT_CHECK_FAILURE_OUTER(test_body());
}


using CheckedTimedMutexTypes = ::testing::Types<
  yamc::checked::timed_mutex,
  yamc::checked::recursive_timed_mutex,
  yamc::checked::shared_timed_mutex
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


using CheckedSharedMutexTypes = ::testing::Types<
  yamc::checked::shared_mutex,
  yamc::checked::shared_timed_mutex
>;

template <typename Mutex>
struct CheckedSharedMutexTest : ::testing::Test {};

TYPED_TEST_CASE(CheckedSharedMutexTest, CheckedSharedMutexTypes);

// reader deadlock
TYPED_TEST(CheckedSharedMutexTest, ReaderDeadlock)
{
  auto test_body = []{
    yamc::test::barrier step(2);
    TypeParam mtx1;
    TypeParam mtx2;
    yamc::test::task_runner(2, [&](std::size_t id) {
      switch (id) {
      case 0:
        ASSERT_NO_THROW(mtx1.lock());
        step.await();  // p1
        EXPECT_NO_THROW(mtx2.lock_shared());
        EXPECT_NO_THROW(mtx2.unlock_shared());
        step.await();  // p2
        EXPECT_NO_THROW(mtx1.unlock());
        break;
      case 1:
        ASSERT_NO_THROW(mtx2.lock());
        step.await();  // p1
        WAIT_TICKS;
        EXPECT_CHECK_FAILURE_INNER({
          mtx1.lock_shared();
        });
        EXPECT_NO_THROW(mtx2.unlock());
        step.await();  // p2
        break;
      }
    });
  };
  EXPECT_CHECK_FAILURE_OUTER(test_body());
}

// reader deadlock with try_lock
TYPED_TEST(CheckedSharedMutexTest, TryLockReaderDeadlock)
{
  auto test_body = []{
    yamc::test::barrier step(2);
    TypeParam mtx1;
    TypeParam mtx2;
    yamc::test::task_runner(2, [&](std::size_t id) {
      switch (id) {
      case 0:
        ASSERT_TRUE(mtx1.try_lock());
        step.await();  // p1
        EXPECT_NO_THROW(mtx2.lock_shared());
        EXPECT_NO_THROW(mtx2.unlock_shared());
        step.await();  // p2
        EXPECT_NO_THROW(mtx1.unlock());
        break;
      case 1:
        ASSERT_NO_THROW(mtx2.try_lock());
        step.await();  // p1
        WAIT_TICKS;
        EXPECT_CHECK_FAILURE_INNER({
          mtx1.lock_shared();
        });
        EXPECT_NO_THROW(mtx2.unlock());
        step.await();  // p2
        break;
      }
    });
  };
  EXPECT_CHECK_FAILURE_OUTER(test_body());
}

// writer deadlock
TYPED_TEST(CheckedSharedMutexTest, WriterDeadlock)
{
  auto test_body = []{
    yamc::test::barrier step(3);
    TypeParam mtx1;
    TypeParam mtx2;
    yamc::test::task_runner(3, [&](std::size_t id) {
      switch (id) {
      case 0:
        ASSERT_NO_THROW(mtx1.lock_shared());
        step.await();  // p1
        EXPECT_NO_THROW(mtx2.lock());
        EXPECT_NO_THROW(mtx2.unlock());
        step.await();  // p2
        EXPECT_NO_THROW(mtx1.unlock_shared());
        break;
      case 1:
        ASSERT_NO_THROW(mtx1.lock_shared());
        step.await();  // p1
        step.await();  // p2
        EXPECT_NO_THROW(mtx1.unlock_shared());
        break;
      case 2:
        ASSERT_NO_THROW(mtx2.lock());
        step.await();  // p1
        WAIT_TICKS;
        EXPECT_CHECK_FAILURE_INNER({
          mtx1.lock();
        });
        EXPECT_NO_THROW(mtx2.unlock());
        step.await();  // p2
        break;
      }
    });
  };
  EXPECT_CHECK_FAILURE_OUTER(test_body());
}

// writer deadlock with try_lock(_shared)
TYPED_TEST(CheckedSharedMutexTest, TryLockWriterDeadlock)
{
  auto test_body = []{
    yamc::test::barrier step(3);
    TypeParam mtx1;
    TypeParam mtx2;
    yamc::test::task_runner(3, [&](std::size_t id) {
      switch (id) {
      case 0:
        ASSERT_TRUE(mtx1.try_lock_shared());
        step.await();  // p1
        EXPECT_NO_THROW(mtx2.lock());
        EXPECT_NO_THROW(mtx2.unlock());
        step.await();  // p2
        EXPECT_NO_THROW(mtx1.unlock_shared());
        break;
      case 1:
        ASSERT_TRUE(mtx1.try_lock_shared());
        step.await();  // p1
        step.await();  // p2
        EXPECT_NO_THROW(mtx1.unlock_shared());
        break;
      case 2:
        ASSERT_TRUE(mtx2.try_lock());
        step.await();  // p1
        WAIT_TICKS;
        EXPECT_CHECK_FAILURE_INNER({
          mtx1.lock();
        });
        EXPECT_NO_THROW(mtx2.unlock());
        step.await();  // p2
        break;
      }
    });
  };
  EXPECT_CHECK_FAILURE_OUTER(test_body());
}


using CheckedSharedTimedMutexTypes = ::testing::Types<
  yamc::checked::shared_timed_mutex
>;

template <typename Mutex>
struct CheckedSharedTimedMutexTest : ::testing::Test {};

TYPED_TEST_CASE(CheckedSharedTimedMutexTest, CheckedSharedTimedMutexTypes);

// reader deadlock with try_lock_for
TYPED_TEST(CheckedSharedTimedMutexTest, TryLockForReaderDeadlock)
{
  auto test_body = []{
    yamc::test::barrier step(2);
    TypeParam mtx1;
    TypeParam mtx2;
    yamc::test::task_runner(2, [&](std::size_t id) {
      switch (id) {
      case 0:
        ASSERT_TRUE(mtx1.try_lock_for(TEST_NOT_TIMEOUT));
        step.await();  // p1
        EXPECT_NO_THROW(mtx2.lock_shared());
        EXPECT_NO_THROW(mtx2.unlock_shared());
        step.await();  // p2
        EXPECT_NO_THROW(mtx1.unlock());
        break;
      case 1:
        ASSERT_NO_THROW(mtx2.try_lock_for(TEST_NOT_TIMEOUT));
        step.await();  // p1
        WAIT_TICKS;
        EXPECT_CHECK_FAILURE_INNER({
          mtx1.lock_shared();
        });
        EXPECT_NO_THROW(mtx2.unlock());
        step.await();  // p2
        break;
      }
    });
  };
  EXPECT_CHECK_FAILURE_OUTER(test_body());
}

// reader deadlock with try_lock_until
TYPED_TEST(CheckedSharedTimedMutexTest, TryLockUntilReaderDeadlock)
{
  auto test_body = []{
    yamc::test::barrier step(2);
    TypeParam mtx1;
    TypeParam mtx2;
    yamc::test::task_runner(2, [&](std::size_t id) {
      switch (id) {
      case 0:
        ASSERT_TRUE(mtx1.try_lock_until(std::chrono::system_clock::now() + TEST_NOT_TIMEOUT));
        step.await();  // p1
        EXPECT_NO_THROW(mtx2.lock_shared());
        EXPECT_NO_THROW(mtx2.unlock_shared());
        step.await();  // p2
        EXPECT_NO_THROW(mtx1.unlock());
        break;
      case 1:
        ASSERT_NO_THROW(mtx2.try_lock_until(std::chrono::system_clock::now() + TEST_NOT_TIMEOUT));
        step.await();  // p1
        WAIT_TICKS;
        EXPECT_CHECK_FAILURE_INNER({
          mtx1.lock_shared();
        });
        EXPECT_NO_THROW(mtx2.unlock());
        step.await();  // p2
        break;
      }
    });
  };
  EXPECT_CHECK_FAILURE_OUTER(test_body());
}

// writer deadlock with try_lock(_shared)_for
TYPED_TEST(CheckedSharedTimedMutexTest, TryLockForWriterDeadlock)
{
  auto test_body = []{
    yamc::test::barrier step(3);
    TypeParam mtx1;
    TypeParam mtx2;
    yamc::test::task_runner(3, [&](std::size_t id) {
      switch (id) {
      case 0:
        ASSERT_TRUE(mtx1.try_lock_shared_for(TEST_NOT_TIMEOUT));
        step.await();  // p1
        EXPECT_NO_THROW(mtx2.lock());
        EXPECT_NO_THROW(mtx2.unlock());
        step.await();  // p2
        EXPECT_NO_THROW(mtx1.unlock_shared());
        break;
      case 1:
        ASSERT_TRUE(mtx1.try_lock_shared_for(TEST_NOT_TIMEOUT));
        step.await();  // p1
        step.await();  // p2
        EXPECT_NO_THROW(mtx1.unlock_shared());
        break;
      case 2:
        ASSERT_TRUE(mtx2.try_lock_for(TEST_NOT_TIMEOUT));
        step.await();  // p1
        WAIT_TICKS;
        EXPECT_CHECK_FAILURE_INNER({
          mtx1.lock();
        });
        EXPECT_NO_THROW(mtx2.unlock());
        step.await();  // p2
        break;
      }
    });
  };
  EXPECT_CHECK_FAILURE_OUTER(test_body());
}

// writer deadlock with try_lock(_shared)_until
TYPED_TEST(CheckedSharedTimedMutexTest, TryLockUntilWriterDeadlock)
{
  auto test_body = []{
    yamc::test::barrier step(3);
    TypeParam mtx1;
    TypeParam mtx2;
    yamc::test::task_runner(3, [&](std::size_t id) {
      switch (id) {
      case 0:
        ASSERT_TRUE(mtx1.try_lock_shared_until(std::chrono::system_clock::now() + TEST_NOT_TIMEOUT));
        step.await();  // p1
        EXPECT_NO_THROW(mtx2.lock());
        EXPECT_NO_THROW(mtx2.unlock());
        step.await();  // p2
        EXPECT_NO_THROW(mtx1.unlock_shared());
        break;
      case 1:
        ASSERT_TRUE(mtx1.try_lock_shared_until(std::chrono::system_clock::now() + TEST_NOT_TIMEOUT));
        step.await();  // p1
        step.await();  // p2
        EXPECT_NO_THROW(mtx1.unlock_shared());
        break;
      case 2:
        ASSERT_TRUE(mtx2.try_lock_until(std::chrono::system_clock::now() + TEST_NOT_TIMEOUT));
        step.await();  // p1
        WAIT_TICKS;
        EXPECT_CHECK_FAILURE_INNER({
          mtx1.lock();
        });
        EXPECT_NO_THROW(mtx2.unlock());
        step.await();  // p2
        break;
      }
    });
  };
  EXPECT_CHECK_FAILURE_OUTER(test_body());
}
