/*
 * checked_test.cpp
 */
#include <chrono>
#include <system_error>
#include "gtest/gtest.h"
#include "checked_mutex.hpp"
#include "checked_shared_mutex.hpp"
#include "yamc_testutil.hpp"


using CheckedMutexTypes = ::testing::Types<
  yamc::checked::mutex,
  yamc::checked::timed_mutex,
  yamc::checked::shared_mutex
>;

template <typename Mutex>
struct CheckedMutexTest : ::testing::Test {};

TYPED_TEST_CASE(CheckedMutexTest, CheckedMutexTypes);

// abondon mutex
TYPED_TEST(CheckedMutexTest, AbondonMutex) {
  ASSERT_THROW({
    TypeParam mtx;
    mtx.lock();
    // no unlock()
  }, std::system_error);
}

// abondon mutex by other thread
TYPED_TEST(CheckedMutexTest, AbondonMutexSide) {
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
    EXPECT_THROW({
      delete pmtx.release();
    }, std::system_error);
    step.await();  // b2
  }
}

// recurse lock() on non-recursive mutex
TYPED_TEST(CheckedMutexTest, RecurseLock) {
  TypeParam mtx;
  ASSERT_NO_THROW(mtx.lock());
  ASSERT_THROW(mtx.lock(), std::system_error);
  ASSERT_NO_THROW(mtx.unlock());
}

// recurse try_lock() on non-recursive mutex
TYPED_TEST(CheckedMutexTest, RecurseTryLock) {
  TypeParam mtx;
  ASSERT_NO_THROW(mtx.lock());
  ASSERT_THROW(mtx.try_lock(), std::system_error);
  ASSERT_NO_THROW(mtx.unlock());
}

// invalid unlock()
TYPED_TEST(CheckedMutexTest, InvalidUnlock0) {
  TypeParam mtx;
  ASSERT_THROW(mtx.unlock(), std::system_error);
}

// invalid unlock()
TYPED_TEST(CheckedMutexTest, InvalidUnlock1) {
  TypeParam mtx;
  ASSERT_NO_THROW(mtx.lock());
  ASSERT_NO_THROW(mtx.unlock());
  ASSERT_THROW(mtx.unlock(), std::system_error);
}

// non owner thread call unlock()
TYPED_TEST(CheckedMutexTest, NonOwnerUnlock) {
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
    EXPECT_THROW(mtx.unlock(), std::system_error);
    step.await();  // b2
  }
}


using CheckedRecursiveMutexTypes = ::testing::Types<
  yamc::checked::recursive_mutex,
  yamc::checked::recursive_timed_mutex
>;

template <typename Mutex>
struct CheckedRecursiveMutexTest : ::testing::Test {};

TYPED_TEST_CASE(CheckedRecursiveMutexTest, CheckedRecursiveMutexTypes);

// abondon recursive_mutex
TYPED_TEST(CheckedRecursiveMutexTest, AbondonMutex) {
  ASSERT_THROW({
    TypeParam mtx;
    mtx.lock();
    // no unlock()
  }, std::system_error);
}

// abondon mutex by other thread
TYPED_TEST(CheckedRecursiveMutexTest, AbondonMutexSide) {
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
    EXPECT_THROW({
      delete pmtx.release();
    }, std::system_error);
    step.await();  // b2
  }
}

// invalid unlock()
TYPED_TEST(CheckedRecursiveMutexTest, InvalidUnlock0) {
  TypeParam mtx;
  ASSERT_THROW(mtx.unlock(), std::system_error);
}

// invalid unlock()
TYPED_TEST(CheckedRecursiveMutexTest, InvalidUnlock1) {
  TypeParam mtx;
  ASSERT_NO_THROW(mtx.lock());    // lockcnt = 1
  ASSERT_NO_THROW(mtx.unlock());  // lockcnt = 0
  ASSERT_THROW(mtx.unlock(), std::system_error);
}

// invalid unlock()
TYPED_TEST(CheckedRecursiveMutexTest, InvalidUnlock2) {
  TypeParam mtx;
  ASSERT_NO_THROW(mtx.lock());    // lockcnt = 1
  ASSERT_NO_THROW(mtx.lock());    // lockcnt = 2
  ASSERT_NO_THROW(mtx.unlock());  // lockcnt = 1
  ASSERT_NO_THROW(mtx.unlock());  // lockcnt = 0
  ASSERT_THROW(mtx.unlock(), std::system_error);
}

// non owner thread call unlock()
TYPED_TEST(CheckedRecursiveMutexTest, NonOwnerUnlock) {
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
    EXPECT_THROW(mtx.unlock(), std::system_error);
    step.await();  // b2
  }
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
  ASSERT_THROW({
    mtx.try_lock_for(std::chrono::seconds(1));
  }, std::system_error);
  ASSERT_NO_THROW(mtx.unlock());
}

// recurse try_lock_until() on non-recursive mutex
TYPED_TEST(CheckedTimedMutexTest, RecurseTryLockUntil) {
  TypeParam mtx;
  ASSERT_NO_THROW(mtx.lock());
  ASSERT_THROW({
    mtx.try_lock_until(std::chrono::system_clock::now());
  }, std::system_error);
  ASSERT_NO_THROW(mtx.unlock());
}


using CheckedSharedMutexTypes = ::testing::Types<
  yamc::checked::shared_mutex,
  yamc::checked::shared_timed_mutex
>;

template <typename Mutex>
struct CheckedSharedMutexTest : ::testing::Test {};

TYPED_TEST_CASE(CheckedSharedMutexTest, CheckedSharedMutexTypes);

// abondon mutex
TYPED_TEST(CheckedSharedMutexTest, AbondonMutex) {
  ASSERT_THROW({
    TypeParam mtx;
    mtx.lock_shared();
    // no unlock()
  }, std::system_error);
}

// abondon mutex by other thread
TYPED_TEST(CheckedSharedMutexTest, AbondonMutexSide) {
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
    EXPECT_THROW({
      delete pmtx.release();
    }, std::system_error);
    step.await();  // b2
  }
}

// recurse lock_shared()
TYPED_TEST(CheckedSharedMutexTest, RecurseLockShared) {
  TypeParam mtx;
  ASSERT_NO_THROW(mtx.lock_shared());
  ASSERT_THROW(mtx.lock_shared(), std::system_error);
  ASSERT_NO_THROW(mtx.unlock_shared());
}

// recurse try_lock_shared()
TYPED_TEST(CheckedSharedMutexTest, RecurseTryLockShared) {
  TypeParam mtx;
  ASSERT_NO_THROW(mtx.lock_shared());
  ASSERT_THROW(mtx.try_lock_shared(), std::system_error);
  ASSERT_NO_THROW(mtx.unlock_shared());
}

// lock() to lock_shared()
TYPED_TEST(CheckedSharedMutexTest, LockToLockShared) {
  TypeParam mtx;
  ASSERT_NO_THROW(mtx.lock());
  ASSERT_THROW(mtx.lock_shared(), std::system_error);
  ASSERT_NO_THROW(mtx.unlock());
}

// lock() to try_lock_shared()
TYPED_TEST(CheckedSharedMutexTest, LockToTryLockShared) {
  TypeParam mtx;
  ASSERT_NO_THROW(mtx.lock());
  ASSERT_THROW(mtx.try_lock_shared(), std::system_error);
  ASSERT_NO_THROW(mtx.unlock());
}

// lock_shared() to lock()
TYPED_TEST(CheckedSharedMutexTest, LockSharedToLock) {
  TypeParam mtx;
  ASSERT_NO_THROW(mtx.lock_shared());
  ASSERT_THROW(mtx.lock(), std::system_error);
  ASSERT_NO_THROW(mtx.unlock_shared());
}

// lock_shared() to try_lock()
TYPED_TEST(CheckedSharedMutexTest, LockSharedToTryLock) {
  TypeParam mtx;
  ASSERT_NO_THROW(mtx.lock_shared());
  ASSERT_THROW(mtx.try_lock(), std::system_error);
  ASSERT_NO_THROW(mtx.unlock_shared());
}

// unmatch unlock()
TYPED_TEST(CheckedSharedMutexTest, UnmatchUnlock) {
  TypeParam mtx;
  ASSERT_NO_THROW(mtx.lock_shared());
  ASSERT_THROW(mtx.unlock(), std::system_error);
  ASSERT_NO_THROW(mtx.unlock_shared());
}

// unmatch unlock_shared()
TYPED_TEST(CheckedSharedMutexTest, UnmatchUnlockShared) {
  TypeParam mtx;
  ASSERT_NO_THROW(mtx.lock());
  ASSERT_THROW(mtx.unlock_shared(), std::system_error);
  ASSERT_NO_THROW(mtx.unlock());
}

// invalid unlock_shared()
TYPED_TEST(CheckedSharedMutexTest, InvalidUnlockShared0) {
  TypeParam mtx;
  ASSERT_THROW(mtx.unlock_shared(), std::system_error);
}

// invalid unlock_shared()
TYPED_TEST(CheckedSharedMutexTest, InvalidUnlockShared1) {
  TypeParam mtx;
  ASSERT_NO_THROW(mtx.lock_shared());
  ASSERT_NO_THROW(mtx.unlock_shared());
  ASSERT_THROW(mtx.unlock_shared(), std::system_error);
}

// non owner thread call unlock_shared()
TYPED_TEST(CheckedSharedMutexTest, NonOwnerUnlockShared) {
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
    EXPECT_THROW(mtx.unlock_shared(), std::system_error);
    step.await();  // b2
  }
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
  ASSERT_THROW({
    mtx.try_lock_shared_for(std::chrono::seconds(1));
  }, std::system_error);
  ASSERT_NO_THROW(mtx.unlock_shared());
}

// recurse try_lock_shared_until()
TYPED_TEST(CheckedSharedTimedMutexTest, RecurseTryLockSharedUntil) {
  TypeParam mtx;
  ASSERT_NO_THROW(mtx.lock_shared());
  ASSERT_THROW({
    mtx.try_lock_shared_until(std::chrono::system_clock::now());
  }, std::system_error);
  ASSERT_NO_THROW(mtx.unlock_shared());
}

// lock() to try_lock_shared_for()
TYPED_TEST(CheckedSharedTimedMutexTest, LockToTryLockSharedFor) {
  TypeParam mtx;
  ASSERT_NO_THROW(mtx.lock());
  ASSERT_THROW({
    mtx.try_lock_shared_for(std::chrono::seconds(1));
  }, std::system_error);
  ASSERT_NO_THROW(mtx.unlock());
}

// lock() to try_lock_shared_until()
TYPED_TEST(CheckedSharedTimedMutexTest, LockToTryLockSharedUntil) {
  TypeParam mtx;
  ASSERT_NO_THROW(mtx.lock());
  ASSERT_THROW({
    mtx.try_lock_shared_until(std::chrono::system_clock::now());
  }, std::system_error);
  ASSERT_NO_THROW(mtx.unlock());
}

// lock_shared() to try_lock_for()
TYPED_TEST(CheckedSharedTimedMutexTest, LockSharedToTryLockFor) {
  TypeParam mtx;
  ASSERT_NO_THROW(mtx.lock_shared());
  ASSERT_THROW({
    mtx.try_lock_for(std::chrono::seconds(1));
  }, std::system_error);
  ASSERT_NO_THROW(mtx.unlock_shared());
}

// lock_shared() to try_lock_until()
TYPED_TEST(CheckedSharedTimedMutexTest, LockSharedToTryLockUntil) {
  TypeParam mtx;
  ASSERT_NO_THROW(mtx.lock_shared());
  ASSERT_THROW({
    mtx.try_lock_until(std::chrono::system_clock::now());
  }, std::system_error);
  ASSERT_NO_THROW(mtx.unlock_shared());
}
