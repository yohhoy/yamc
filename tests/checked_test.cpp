#include <system_error>
#include <thread>
#include "gtest/gtest.h"
#include "checked_mutex.hpp"
#include "yamc_test.hpp"


// abondon mutex
TEST(CheckedMutexTest, AbondonMutex) {
  ASSERT_THROW({
    yamc::checked::mutex mtx;
    mtx.lock();
    // no unlock()
  }, std::system_error);
}

// abondon mutex by other thread
TEST(CheckedMutexTest, AbondonMutexSide) {
  yamc::test::barrier b(2);
  auto pmtx = yamc::cxx::make_unique<yamc::checked::mutex>();
  // owner-thread
  std::thread thd([&]{
    ASSERT_NO_THROW(pmtx->lock());
    b.await();  // b1
    b.await();  // b2
  });
  // other-thread
  {
    b.await();  // b1
    ASSERT_THROW({
      delete pmtx.release();
    }, std::system_error);
    b.await();  // b2
  }
  thd.join();
}

// recurse lock() on non-recursive mutex
TEST(CheckedMutexTest, RecurseLock) {
  yamc::checked::mutex mtx;
  ASSERT_NO_THROW(mtx.lock());
  ASSERT_THROW(mtx.lock(), std::system_error);
  ASSERT_NO_THROW(mtx.unlock());
}

// recurse try_lock() on non-recursive mutex
TEST(CheckedMutexTest, RecurseTryLock) {
  yamc::checked::mutex mtx;
  ASSERT_NO_THROW(mtx.lock());
  ASSERT_THROW(mtx.try_lock(), std::system_error);
  ASSERT_NO_THROW(mtx.unlock());
}

// invalid unlock()
TEST(CheckedMutexTest, InvalidUnlock0) {
  yamc::checked::mutex mtx;
  ASSERT_THROW(mtx.unlock(), std::system_error);
}

// invalid unlock()
TEST(CheckedMutexTest, InvalidUnlock1) {
  yamc::checked::mutex mtx;
  ASSERT_NO_THROW(mtx.lock());
  ASSERT_NO_THROW(mtx.unlock());
  ASSERT_THROW(mtx.unlock(), std::system_error);
}

// non owner thread call unlock()
TEST(CheckedMutexTest, NonOwnerUnlock) {
  yamc::test::barrier step(2);
  yamc::checked::mutex mtx;
  // owner-thread
  std::thread thd([&]{
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
  thd.join();
}

// abondon recursive_mutex
TEST(CheckedRecursiveMutexTest, AbondonMutex) {
  ASSERT_THROW({
    yamc::checked::recursive_mutex mtx;
    mtx.lock();
    // no unlock()
  }, std::system_error);
}

// abondon mutex by other thread
TEST(CheckedRecursiveMutexTest, AbondonMutexSide) {
  yamc::test::barrier b(2);
  auto pmtx = yamc::cxx::make_unique<yamc::checked::recursive_mutex>();
  // owner-thread
  std::thread thd([&]{
    ASSERT_NO_THROW(pmtx->lock());
    b.await();  // b1
    b.await();  // b2
  });
  // other-thread
  {
    b.await();  // b1
    ASSERT_THROW({
      delete pmtx.release();
    }, std::system_error);
    b.await();  // b2
  }
  thd.join();
}

// invalid unlock()
TEST(CheckedRecursiveMutexTest, InvalidUnlock0) {
  yamc::checked::recursive_mutex mtx;
  ASSERT_THROW(mtx.unlock(), std::system_error);
}

// invalid unlock()
TEST(CheckedRecursiveMutexTest, InvalidUnlock1) {
  yamc::checked::recursive_mutex mtx;
  ASSERT_NO_THROW(mtx.lock());    // lockcnt = 1
  ASSERT_NO_THROW(mtx.unlock());  // lockcnt = 0
  ASSERT_THROW(mtx.unlock(), std::system_error);
}

// invalid unlock()
TEST(CheckedRecursiveMutexTest, InvalidUnlock2) {
  yamc::checked::recursive_mutex mtx;
  ASSERT_NO_THROW(mtx.lock());    // lockcnt = 1
  ASSERT_NO_THROW(mtx.lock());    // lockcnt = 2
  ASSERT_NO_THROW(mtx.unlock());  // lockcnt = 1
  ASSERT_NO_THROW(mtx.unlock());  // lockcnt = 0
  ASSERT_THROW(mtx.unlock(), std::system_error);
}

// non owner thread call unlock()
TEST(CheckedRecursiveMutexTest, NonOwnerUnlock) {
  yamc::test::barrier step(2);
  yamc::checked::recursive_mutex mtx;
  // owner-thread
  std::thread thd([&]{
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
  thd.join();
}
