/*
 * checked_test.cpp
 */
#include <chrono>
#include <system_error>
#include "gtest/gtest.h"
#include "checked_mutex.hpp"
#include "yamc_testutil.hpp"


using CheckedMutexTypes = ::testing::Types<
  yamc::checked::mutex,
  yamc::checked::timed_mutex
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
    ASSERT_THROW({
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
    ASSERT_THROW(mtx.unlock(), std::system_error);
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
    ASSERT_THROW({
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
    ASSERT_THROW(mtx.unlock(), std::system_error);
    step.await();  // b2
  }
}


// recurse try_lock_for() on non-recursive mutex
TEST(CheckedTimedMutexTest, RecurseTryLockFor) {
  yamc::checked::timed_mutex mtx;
  ASSERT_NO_THROW(mtx.lock());
  ASSERT_THROW({
    mtx.try_lock_for(std::chrono::seconds(1));
  }, std::system_error);
  ASSERT_NO_THROW(mtx.unlock());
}

// recurse try_lock_until() on non-recursive mutex
TEST(CheckedTimedMutexTest, RecurseTryLockUntil) {
  yamc::checked::timed_mutex mtx;
  ASSERT_NO_THROW(mtx.lock());
  ASSERT_THROW({
    mtx.try_lock_until(std::chrono::system_clock::now());
  }, std::system_error);
  ASSERT_NO_THROW(mtx.unlock());
}
