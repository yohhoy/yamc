/*
 * checked_test.cpp
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


using CheckedMutexTypes = ::testing::Types<
  yamc::checked::mutex,
  yamc::checked::timed_mutex,
  yamc::checked::shared_mutex
>;

template <typename Mutex>
struct CheckedMutexTest : ::testing::Test {};

TYPED_TEST_CASE(CheckedMutexTest, CheckedMutexTypes);

// abandon mutex
TYPED_TEST(CheckedMutexTest, AbandonMutex) {
  EXPECT_CHECK_FAILURE({
    TypeParam mtx;
    mtx.lock();
    // no unlock()
  });
}

// abandon mutex by other thread
TYPED_TEST(CheckedMutexTest, AbandonMutexSide) {
  auto test_body = []{
    yamc::test::barrier step(2);
    auto pmtx = yamc::cxx::make_unique<TypeParam>();
    // owner-thread
    yamc::test::join_thread thd([&]{
      ASSERT_NO_THROW(pmtx->lock());
      step.await();  // b1
      step.await();  // b2
    });
    // other-thread
    {
      step.await();  // b1
      EXPECT_CHECK_FAILURE_INNER({
        delete pmtx.release();
      });
      step.await();  // b2
    }
  };
  EXPECT_CHECK_FAILURE_OUTER(test_body());
}

// recurse lock() on non-recursive mutex
TYPED_TEST(CheckedMutexTest, RecurseLock) {
  TypeParam mtx;
  ASSERT_NO_THROW(mtx.lock());
  EXPECT_CHECK_FAILURE(mtx.lock());
  ASSERT_NO_THROW(mtx.unlock());
}

// recurse try_lock() on non-recursive mutex
TYPED_TEST(CheckedMutexTest, RecurseTryLock) {
  TypeParam mtx;
  ASSERT_NO_THROW(mtx.lock());
  EXPECT_CHECK_FAILURE(mtx.try_lock());
  ASSERT_NO_THROW(mtx.unlock());
}

// invalid unlock()
TYPED_TEST(CheckedMutexTest, InvalidUnlock0) {
  TypeParam mtx;
  EXPECT_CHECK_FAILURE(mtx.unlock());
}

// invalid unlock()
TYPED_TEST(CheckedMutexTest, InvalidUnlock1) {
  TypeParam mtx;
  ASSERT_NO_THROW(mtx.lock());
  ASSERT_NO_THROW(mtx.unlock());
  EXPECT_CHECK_FAILURE(mtx.unlock());
}

// non owner thread call unlock()
TYPED_TEST(CheckedMutexTest, NonOwnerUnlock) {
  auto test_body = []{
    yamc::test::barrier step(2);
    TypeParam mtx;
    // owner-thread
    yamc::test::join_thread thd([&]{
      ASSERT_NO_THROW(mtx.lock());
      step.await();  // b1
      step.await();  // b2
      ASSERT_NO_THROW(mtx.unlock());
    });
    // other-thread
    {
      step.await();  // b1
      EXPECT_CHECK_FAILURE_INNER(mtx.unlock());
      step.await();  // b2
    }
  };
  EXPECT_CHECK_FAILURE_OUTER(test_body());
}


using CheckedRecursiveMutexTypes = ::testing::Types<
  yamc::checked::recursive_mutex,
  yamc::checked::recursive_timed_mutex
>;

template <typename Mutex>
struct CheckedRecursiveMutexTest : ::testing::Test {};

TYPED_TEST_CASE(CheckedRecursiveMutexTest, CheckedRecursiveMutexTypes);

// abandon recursive_mutex
TYPED_TEST(CheckedRecursiveMutexTest, AbandonMutex) {
  EXPECT_CHECK_FAILURE({
    TypeParam mtx;
    mtx.lock();
    // no unlock()
  });
}

// abandon mutex by other thread
TYPED_TEST(CheckedRecursiveMutexTest, AbandonMutexSide) {
  auto test_body = []{
    yamc::test::barrier step(2);
    auto pmtx = yamc::cxx::make_unique<TypeParam>();
    // owner-thread
    yamc::test::join_thread thd([&]{
      ASSERT_NO_THROW(pmtx->lock());
      step.await();  // b1
      step.await();  // b2
    });
    // other-thread
    {
      step.await();  // b1
      EXPECT_CHECK_FAILURE_INNER({
        delete pmtx.release();
      });
      step.await();  // b2
    }
  };
  EXPECT_CHECK_FAILURE_OUTER(test_body());
}

// invalid unlock()
TYPED_TEST(CheckedRecursiveMutexTest, InvalidUnlock0) {
  TypeParam mtx;
  EXPECT_CHECK_FAILURE(mtx.unlock());
}

// invalid unlock()
TYPED_TEST(CheckedRecursiveMutexTest, InvalidUnlock1) {
  TypeParam mtx;
  ASSERT_NO_THROW(mtx.lock());    // lockcnt = 1
  ASSERT_NO_THROW(mtx.unlock());  // lockcnt = 0
  EXPECT_CHECK_FAILURE(mtx.unlock());
}

// invalid unlock()
TYPED_TEST(CheckedRecursiveMutexTest, InvalidUnlock2) {
  TypeParam mtx;
  ASSERT_NO_THROW(mtx.lock());    // lockcnt = 1
  ASSERT_NO_THROW(mtx.lock());    // lockcnt = 2
  ASSERT_NO_THROW(mtx.unlock());  // lockcnt = 1
  ASSERT_NO_THROW(mtx.unlock());  // lockcnt = 0
  EXPECT_CHECK_FAILURE(mtx.unlock());
}

// non owner thread call unlock()
TYPED_TEST(CheckedRecursiveMutexTest, NonOwnerUnlock) {
  auto test_body = []{
    yamc::test::barrier step(2);
    TypeParam mtx;
    // owner-thread
    yamc::test::join_thread thd([&]{
      ASSERT_NO_THROW(mtx.lock());
      step.await();  // b1
      step.await();  // b2
      ASSERT_NO_THROW(mtx.unlock());
    });
    // other-thread
    {
      step.await();  // b1
      EXPECT_CHECK_FAILURE_INNER(mtx.unlock());
      step.await();  // b2
    }
  };
  EXPECT_CHECK_FAILURE_OUTER(test_body());
}


using CheckedTimedMutexTypes = ::testing::Types<
  yamc::checked::timed_mutex,
  yamc::checked::shared_timed_mutex
>;

template <typename Mutex>
struct CheckedTimedMutexTest : ::testing::Test {};

TYPED_TEST_CASE(CheckedTimedMutexTest, CheckedTimedMutexTypes);

// recurse try_lock_for() on non-recursive mutex
TYPED_TEST(CheckedTimedMutexTest, RecurseTryLockFor) {
  TypeParam mtx;
  ASSERT_NO_THROW(mtx.lock());
  EXPECT_CHECK_FAILURE(mtx.try_lock_for(std::chrono::seconds(1)));
  ASSERT_NO_THROW(mtx.unlock());
}

// recurse try_lock_until() on non-recursive mutex
TYPED_TEST(CheckedTimedMutexTest, RecurseTryLockUntil) {
  TypeParam mtx;
  ASSERT_NO_THROW(mtx.lock());
  EXPECT_CHECK_FAILURE(mtx.try_lock_until(std::chrono::system_clock::now()));
  ASSERT_NO_THROW(mtx.unlock());
}


using CheckedSharedMutexTypes = ::testing::Types<
  yamc::checked::shared_mutex,
  yamc::checked::shared_timed_mutex
>;

template <typename Mutex>
struct CheckedSharedMutexTest : ::testing::Test {};

TYPED_TEST_CASE(CheckedSharedMutexTest, CheckedSharedMutexTypes);

// abandon mutex
TYPED_TEST(CheckedSharedMutexTest, AbandonMutex) {
  EXPECT_CHECK_FAILURE({
    TypeParam mtx;
    mtx.lock_shared();
    // no unlock()
  });
}

// abandon mutex by other thread
TYPED_TEST(CheckedSharedMutexTest, AbandonMutexSide) {
  auto test_body = []{
    yamc::test::barrier step(2);
    auto pmtx = yamc::cxx::make_unique<TypeParam>();
    // owner-thread
    yamc::test::join_thread thd([&]{
      ASSERT_NO_THROW(pmtx->lock_shared());
      step.await();  // b1
      step.await();  // b2
    });
    // other-thread
    {
      step.await();  // b1
      EXPECT_CHECK_FAILURE_INNER({
        delete pmtx.release();
      });
      step.await();  // b2
    }
  };
  EXPECT_CHECK_FAILURE_OUTER(test_body());
}

// recurse lock_shared()
TYPED_TEST(CheckedSharedMutexTest, RecurseLockShared) {
  TypeParam mtx;
  ASSERT_NO_THROW(mtx.lock_shared());
  EXPECT_CHECK_FAILURE(mtx.lock_shared());
  ASSERT_NO_THROW(mtx.unlock_shared());
}

// recurse try_lock_shared()
TYPED_TEST(CheckedSharedMutexTest, RecurseTryLockShared) {
  TypeParam mtx;
  ASSERT_NO_THROW(mtx.lock_shared());
  EXPECT_CHECK_FAILURE(mtx.try_lock_shared());
  ASSERT_NO_THROW(mtx.unlock_shared());
}

// lock() to lock_shared()
TYPED_TEST(CheckedSharedMutexTest, LockToLockShared) {
  TypeParam mtx;
  ASSERT_NO_THROW(mtx.lock());
  EXPECT_CHECK_FAILURE(mtx.lock_shared());
  ASSERT_NO_THROW(mtx.unlock());
}

// lock() to try_lock_shared()
TYPED_TEST(CheckedSharedMutexTest, LockToTryLockShared) {
  TypeParam mtx;
  ASSERT_NO_THROW(mtx.lock());
  EXPECT_CHECK_FAILURE(mtx.try_lock_shared());
  ASSERT_NO_THROW(mtx.unlock());
}

// lock_shared() to lock()
TYPED_TEST(CheckedSharedMutexTest, LockSharedToLock) {
  TypeParam mtx;
  ASSERT_NO_THROW(mtx.lock_shared());
  EXPECT_CHECK_FAILURE(mtx.lock());
  ASSERT_NO_THROW(mtx.unlock_shared());
}

// lock_shared() to try_lock()
TYPED_TEST(CheckedSharedMutexTest, LockSharedToTryLock) {
  TypeParam mtx;
  ASSERT_NO_THROW(mtx.lock_shared());
  EXPECT_CHECK_FAILURE(mtx.try_lock());
  ASSERT_NO_THROW(mtx.unlock_shared());
}

// unmatch unlock()
TYPED_TEST(CheckedSharedMutexTest, UnmatchUnlock) {
  TypeParam mtx;
  ASSERT_NO_THROW(mtx.lock_shared());
  EXPECT_CHECK_FAILURE(mtx.unlock());
  ASSERT_NO_THROW(mtx.unlock_shared());
}

// unmatch unlock_shared()
TYPED_TEST(CheckedSharedMutexTest, UnmatchUnlockShared) {
  TypeParam mtx;
  ASSERT_NO_THROW(mtx.lock());
  EXPECT_CHECK_FAILURE(mtx.unlock_shared());
  ASSERT_NO_THROW(mtx.unlock());
}

// invalid unlock_shared()
TYPED_TEST(CheckedSharedMutexTest, InvalidUnlockShared0) {
  TypeParam mtx;
  EXPECT_CHECK_FAILURE(mtx.unlock_shared());
}

// invalid unlock_shared()
TYPED_TEST(CheckedSharedMutexTest, InvalidUnlockShared1) {
  TypeParam mtx;
  ASSERT_NO_THROW(mtx.lock_shared());
  ASSERT_NO_THROW(mtx.unlock_shared());
  EXPECT_CHECK_FAILURE(mtx.unlock_shared());
}

// non owner thread call unlock_shared()
TYPED_TEST(CheckedSharedMutexTest, NonOwnerUnlockShared) {
  auto test_body = []{
    yamc::test::barrier step(2);
    TypeParam mtx;
    // owner-thread
    yamc::test::join_thread thd([&]{
      ASSERT_NO_THROW(mtx.lock_shared());
      step.await();  // b1
      step.await();  // b2
      ASSERT_NO_THROW(mtx.unlock_shared());
    });
    // other-thread
    {
      step.await();  // b1
      EXPECT_CHECK_FAILURE_INNER(mtx.unlock_shared());
      step.await();  // b2
    }
  };
  EXPECT_CHECK_FAILURE_OUTER(test_body());
}


using CheckedSharedTimedMutexTypes = ::testing::Types<
  yamc::checked::shared_timed_mutex
>;

template <typename Mutex>
struct CheckedSharedTimedMutexTest : ::testing::Test {};

TYPED_TEST_CASE(CheckedSharedTimedMutexTest, CheckedSharedTimedMutexTypes);

// recurse try_lock_shared_for()
TYPED_TEST(CheckedSharedTimedMutexTest, RecurseTryLockSharedFor) {
  TypeParam mtx;
  ASSERT_NO_THROW(mtx.lock_shared());
  EXPECT_CHECK_FAILURE(mtx.try_lock_shared_for(std::chrono::seconds(1)));
  ASSERT_NO_THROW(mtx.unlock_shared());
}

// recurse try_lock_shared_until()
TYPED_TEST(CheckedSharedTimedMutexTest, RecurseTryLockSharedUntil) {
  TypeParam mtx;
  ASSERT_NO_THROW(mtx.lock_shared());
  EXPECT_CHECK_FAILURE(mtx.try_lock_shared_until(std::chrono::system_clock::now()));
  ASSERT_NO_THROW(mtx.unlock_shared());
}

// lock() to try_lock_shared_for()
TYPED_TEST(CheckedSharedTimedMutexTest, LockToTryLockSharedFor) {
  TypeParam mtx;
  ASSERT_NO_THROW(mtx.lock());
  EXPECT_CHECK_FAILURE(mtx.try_lock_shared_for(std::chrono::seconds(1)));
  ASSERT_NO_THROW(mtx.unlock());
}

// lock() to try_lock_shared_until()
TYPED_TEST(CheckedSharedTimedMutexTest, LockToTryLockSharedUntil) {
  TypeParam mtx;
  ASSERT_NO_THROW(mtx.lock());
  EXPECT_CHECK_FAILURE(mtx.try_lock_shared_until(std::chrono::system_clock::now()));
  ASSERT_NO_THROW(mtx.unlock());
}

// lock_shared() to try_lock_for()
TYPED_TEST(CheckedSharedTimedMutexTest, LockSharedToTryLockFor) {
  TypeParam mtx;
  ASSERT_NO_THROW(mtx.lock_shared());
  EXPECT_CHECK_FAILURE(mtx.try_lock_for(std::chrono::seconds(1)));
  ASSERT_NO_THROW(mtx.unlock_shared());
}

// lock_shared() to try_lock_until()
TYPED_TEST(CheckedSharedTimedMutexTest, LockSharedToTryLockUntil) {
  TypeParam mtx;
  ASSERT_NO_THROW(mtx.lock_shared());
  EXPECT_CHECK_FAILURE(mtx.try_lock_until(std::chrono::system_clock::now()));
  ASSERT_NO_THROW(mtx.unlock_shared());
}
