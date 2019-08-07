/*
 * lock_test.cpp
 */
#include <type_traits>
#include "gtest/gtest.h"
#include "yamc_shared_lock.hpp"
#include "yamc_scoped_lock.hpp"
#include "yamc_testutil.hpp"


#define EXPECT_THROW_SYSTEM_ERROR(errorcode_, block_) \
  try { \
    block_ \
    FAIL(); \
  } catch (const std::system_error& e) { \
    EXPECT_EQ(std::make_error_code(errorcode_), e.code()); \
  }


using MockSharedMutex = yamc::mock::shared_mutex;
using MockSharedTimedMutex = yamc::mock::shared_timed_mutex;

// shared_lock::mutex_type
TEST(SharedLockTest, MutexType)
{
  bool shall_be_true = std::is_same<MockSharedMutex, yamc::shared_lock<MockSharedMutex>::mutex_type>::value;
  EXPECT_TRUE(shall_be_true);
}

// shared_lock() noexcept
TEST(SharedLockTest, CtorDefault)
{
  yamc::shared_lock<MockSharedMutex> lk;
  EXPECT_EQ(nullptr, lk.mutex());
  EXPECT_FALSE(lk.owns_lock());
  EXPECT_TRUE(noexcept(yamc::shared_lock<MockSharedMutex>{}));
}

// explicit shared_lock(mutex_type&)
TEST(SharedLockTest, CtorMutex)
{
  MockSharedMutex mtx;
  yamc::shared_lock<MockSharedMutex> lk(mtx);
  EXPECT_EQ(&mtx, lk.mutex());
  EXPECT_TRUE(lk.owns_lock());
}

// shared_lock(mutex_type&, defer_lock_t) noexcept
TEST(SharedLockTest, CtorDeferLock)
{
  MockSharedMutex mtx;
  yamc::shared_lock<MockSharedMutex> lk(mtx, std::defer_lock);
  EXPECT_EQ(&mtx, lk.mutex());
  EXPECT_FALSE(lk.owns_lock());
  EXPECT_TRUE(noexcept(yamc::shared_lock<MockSharedMutex>(mtx, std::defer_lock)));
}

// shared_lock(mutex_type&, try_to_lock_t)
TEST(SharedLockTest, CtorTryToLock)
{
  MockSharedMutex mtx;
  yamc::shared_lock<MockSharedMutex> lk(mtx, std::try_to_lock);
  EXPECT_EQ(&mtx, lk.mutex());
  EXPECT_TRUE(lk.owns_lock());
}

// shared_lock(mutex_type&, try_to_lock_t) failure
TEST(SharedLockTest, CtorTryToLockFail)
{
  MockSharedMutex mtx;
  mtx.retval_on_trylock = false;
  yamc::shared_lock<MockSharedMutex> lk(mtx, std::try_to_lock);
  EXPECT_EQ(&mtx, lk.mutex());
  EXPECT_FALSE(lk.owns_lock());
}

// shared_lock(mutex_type&, adopt_lock_t)
TEST(SharedLockTest, CtorAdoptLock)
{
  MockSharedMutex mtx;
  mtx.lock_shared();
  yamc::shared_lock<MockSharedMutex> lk(mtx, std::adopt_lock);
  EXPECT_EQ(&mtx, lk.mutex());
  EXPECT_TRUE(lk.owns_lock());
}

// shared_lock(mutex_type&, const chrono::time_point&)
TEST(SharedLockTest, CtorTimePoint)
{
  MockSharedTimedMutex mtx;
  yamc::shared_lock<MockSharedTimedMutex> lk(mtx, std::chrono::system_clock::now());
  EXPECT_EQ(&mtx, lk.mutex());
  EXPECT_TRUE(lk.owns_lock());
}

// shared_lock(mutex_type&, const chrono::time_point&) failure
TEST(SharedLockTest, CtorTimePointFail)
{
  MockSharedTimedMutex mtx;
  mtx.retval_on_trylock = false;
  yamc::shared_lock<MockSharedTimedMutex> lk(mtx, std::chrono::system_clock::now());
  EXPECT_EQ(&mtx, lk.mutex());
  EXPECT_FALSE(lk.owns_lock());
}

// shared_lock(mutex_type&, const chrono::duration&)
TEST(SharedLockTest, CtorRelTime)
{
  MockSharedTimedMutex mtx;
  yamc::shared_lock<MockSharedTimedMutex> lk(mtx, std::chrono::milliseconds(1));
  EXPECT_EQ(&mtx, lk.mutex());
  EXPECT_TRUE(lk.owns_lock());
}

// shared_lock(mutex_type&, const chrono::duration&) failure
TEST(SharedLockTest, CtorRelTimeFail)
{
  MockSharedTimedMutex mtx;
  mtx.retval_on_trylock = false;
  yamc::shared_lock<MockSharedTimedMutex> lk(mtx, std::chrono::milliseconds(1));
  EXPECT_EQ(&mtx, lk.mutex());
  EXPECT_FALSE(lk.owns_lock());
}

// shared_lock(shared_lock&&) noexcept
TEST(SharedLockTest, MoveCtor)
{
  MockSharedMutex mtx;
  yamc::shared_lock<MockSharedMutex> lk1(mtx);
  yamc::shared_lock<MockSharedMutex> lk2(std::move(lk1));  // move-constructor
  EXPECT_EQ(nullptr, lk1.mutex());
  EXPECT_FALSE(lk1.owns_lock());
  EXPECT_EQ(&mtx, lk2.mutex());
  EXPECT_TRUE(lk2.owns_lock());
  EXPECT_TRUE(noexcept(yamc::shared_lock<MockSharedMutex>(std::move(lk2))));
}

// shared_lock& operator=(shared_lock&&) noexcept
TEST(SharedLockTest, MoveAssign)
{
  MockSharedMutex mtx;
  yamc::shared_lock<MockSharedMutex> lk1(mtx);
  yamc::shared_lock<MockSharedMutex> lk2;
  lk2 = std::move(lk1);  // move-assignment
  EXPECT_EQ(nullptr, lk1.mutex());
  EXPECT_FALSE(lk1.owns_lock());
  EXPECT_EQ(&mtx, lk2.mutex());
  EXPECT_TRUE(lk2.owns_lock());
  EXPECT_TRUE(noexcept(lk1 = std::move(lk2)));
}

// lock()
TEST(SharedLockTest, Lock)
{
  MockSharedMutex mtx;
  yamc::shared_lock<MockSharedMutex> lk(mtx, std::defer_lock);
  EXPECT_NO_THROW(lk.lock());
  EXPECT_TRUE(lk.owns_lock());
}

// lock() throw exception/operation_not_permitted
TEST(SharedLockTest, LockThrowEPERM)
{
  {
    yamc::shared_lock<MockSharedMutex> lk;
    EXPECT_THROW(lk.lock(), std::system_error);
  }
  {
    yamc::shared_lock<MockSharedMutex> lk;
    EXPECT_THROW_SYSTEM_ERROR(std::errc::operation_not_permitted, {
      lk.lock();
    });
  }
}

// lock() throw exception/resource_deadlock_would_occur
TEST(SharedLockTest, LockThrowEDEADLK)
{
  {
    yamc::shared_lock<MockSharedMutex> lk;
    EXPECT_THROW(lk.lock(), std::system_error);
  }
  {
    MockSharedMutex mtx;
    yamc::shared_lock<MockSharedMutex> lk(mtx);
    EXPECT_THROW_SYSTEM_ERROR(std::errc::resource_deadlock_would_occur, {
      lk.lock();
    });
  }
}

// try_lock()
TEST(SharedLockTest, TryLock)
{
  MockSharedMutex mtx;
  yamc::shared_lock<MockSharedMutex> lk(mtx, std::defer_lock);
  EXPECT_TRUE(lk.try_lock());
  EXPECT_TRUE(lk.owns_lock());
}

// try_lock() failure
TEST(SharedLockTest, TryLockFail)
{
  MockSharedMutex mtx;
  mtx.retval_on_trylock = false;
  yamc::shared_lock<MockSharedMutex> lk(mtx, std::defer_lock);
  EXPECT_FALSE(lk.try_lock());
  EXPECT_FALSE(lk.owns_lock());
}

// try_lock() throw exception/operation_not_permitted
TEST(SharedLockTest, TryLockThrowEPERM)
{
  {
    yamc::shared_lock<MockSharedMutex> lk;
    EXPECT_THROW(lk.try_lock(), std::system_error);
  }
  {
    yamc::shared_lock<MockSharedMutex> lk;
    EXPECT_THROW_SYSTEM_ERROR(std::errc::operation_not_permitted, {
      lk.try_lock();
    });
  }
}

// try_lock() throw exception/resource_deadlock_would_occur
TEST(SharedLockTest, TryLockThrowEDEADLK)
{
  {
    MockSharedMutex mtx;
    yamc::shared_lock<MockSharedMutex> lk(mtx);
    EXPECT_THROW(lk.try_lock(), std::system_error);
  }
  {
    MockSharedMutex mtx;
    yamc::shared_lock<MockSharedMutex> lk(mtx);
    EXPECT_THROW_SYSTEM_ERROR(std::errc::resource_deadlock_would_occur, {
      lk.try_lock();
    });
  }
}

// unlock()
TEST(SharedLockTest, Unlock)
{
  MockSharedMutex mtx;
  yamc::shared_lock<MockSharedMutex> lk(mtx);
  EXPECT_NO_THROW(lk.unlock());
  EXPECT_FALSE(lk.owns_lock());
}

// unlock() throw system_error/operation_not_permitted
TEST(SharedLockTest, UnlockThrowEPERM)
{
  {
    MockSharedMutex mtx;
    yamc::shared_lock<MockSharedMutex> lk(mtx, std::defer_lock);
    EXPECT_THROW(lk.unlock(), std::system_error);
  }
  {
    MockSharedMutex mtx;
    yamc::shared_lock<MockSharedMutex> lk(mtx, std::defer_lock);
    EXPECT_THROW_SYSTEM_ERROR(std::errc::operation_not_permitted, {
      lk.unlock();
    });
  }
}

// void swap(shared_lock&) noexcept
TEST(SharedLockTest, Swap)
{
  MockSharedMutex mtx1, mtx2;
  yamc::shared_lock<MockSharedMutex> lk1(mtx1);                   // {&mtx1, true}
  yamc::shared_lock<MockSharedMutex> lk2(mtx2, std::defer_lock);  // {&mtx2, false}
  lk1.swap(lk2);
  EXPECT_EQ(&mtx2, lk1.mutex());
  EXPECT_FALSE(lk1.owns_lock());
  EXPECT_EQ(&mtx1, lk2.mutex());
  EXPECT_TRUE(lk2.owns_lock());
  EXPECT_TRUE(noexcept(lk1.swap(lk2)));
}

// void swap(shared_lock&, shared_lock&) noexcept
TEST(SharedLockTest, SwapNonMember)
{
  MockSharedMutex mtx1, mtx2;
  yamc::shared_lock<MockSharedMutex> lk1(mtx1);                   // {&mtx1, true}
  yamc::shared_lock<MockSharedMutex> lk2(mtx2, std::defer_lock);  // {&mtx2, false}
  std::swap(lk1, lk2);
  EXPECT_EQ(&mtx2, lk1.mutex());
  EXPECT_FALSE(lk1.owns_lock());
  EXPECT_EQ(&mtx1, lk2.mutex());
  EXPECT_TRUE(lk2.owns_lock());
  EXPECT_TRUE(noexcept(std::swap(lk1, lk2)));
}

// mutex_type* release() noexcept
TEST(SharedLockTest, Release)
{
  MockSharedMutex mtx;
  yamc::shared_lock<MockSharedMutex> lk(mtx);
  EXPECT_EQ(&mtx, lk.release());
  EXPECT_EQ(nullptr, lk.mutex());
  EXPECT_FALSE(lk.owns_lock());
  EXPECT_TRUE(noexcept(lk.release()));
}

// bool owns_lock() const noexcept
TEST(SharedLockTest, OwnsLock)
{
  MockSharedMutex mtx;
  const yamc::shared_lock<MockSharedMutex> lk(mtx);
  EXPECT_TRUE(lk.owns_lock());
  EXPECT_TRUE(noexcept(lk.owns_lock()));
}

// explicit operator bool () const noexcept
TEST(SharedLockTest, OperatorBool)
{
  {
    MockSharedMutex mtx;
    const yamc::shared_lock<MockSharedMutex> lk(mtx);
    if (lk) {  // shall be true
      SUCCEED();
    } else {
      FAIL();
    }
  }
  {
    MockSharedMutex mtx;
    const yamc::shared_lock<MockSharedMutex> lk(mtx, std::defer_lock);
    if (lk) {  // shall be false
      FAIL();
    } else {
      SUCCEED();
    }
  }
}

// mutex_type* mutex() const noexcept
TEST(SharedLockTest, Mutex)
{
  MockSharedMutex mtx;
  const yamc::shared_lock<MockSharedMutex> lk(mtx);
  EXPECT_EQ(&mtx, lk.mutex());
  EXPECT_TRUE(noexcept(lk.mutex()));
}


using MockMutex = yamc::mock::mutex;

template <typename, typename = yamc::cxx::void_t<>>
struct has_mutex_type : std::false_type { };

template <typename T>
struct has_mutex_type<T, yamc::cxx::void_t<typename T::mutex_type>> : std::true_type { };


// scoped_lock::mutex_type
TEST(ScopedLockTest, MutexType)
{
  // scoped_lock<M1>
  bool shall_be_true = std::is_same<MockMutex, yamc::scoped_lock<MockMutex>::mutex_type>::value;
  EXPECT_TRUE(shall_be_true);
}

// scoped_lock::mutex_type not exist
TEST(ScopedLockTest, MutexTypeNotExist)
{
  // scoped_lock<>
  EXPECT_FALSE(has_mutex_type<yamc::scoped_lock<>>::value);
  // scoped_lock<M1, M2>
  bool shall_be_false = has_mutex_type<yamc::scoped_lock<MockMutex, MockMutex>>::value;
  EXPECT_FALSE(shall_be_false);
}

// explicit scoped_lock()
TEST(ScopedLockTest, CtorLock0)
{
  yamc::scoped_lock<> lk;
  (void)lk;  // suppress "unused variable" warning
  SUCCEED();
}

// explicit scoped_lock(Mutex1)
TEST(ScopedLockTest, CtorLock1)
{
  MockMutex mtx1;
  {
    yamc::scoped_lock<MockMutex> lk(mtx1);
    EXPECT_TRUE(mtx1.locked);
  }
  EXPECT_FALSE(mtx1.locked);
}

// explicit scoped_lock(Mutex1, Mutex2)
TEST(ScopedLockTest, CtorLock2)
{
  MockMutex mtx1, mtx2;
  {
    yamc::scoped_lock<MockMutex, MockMutex> lk(mtx1, mtx2);
    EXPECT_TRUE(mtx1.locked);
    EXPECT_TRUE(mtx2.locked);
  }
  EXPECT_FALSE(mtx1.locked);
  EXPECT_FALSE(mtx2.locked);
}

// explicit scoped_lock(adopt_lock_t)
TEST(ScopedLockTest, CtorAdoptLock0)
{
  yamc::scoped_lock<> lk(std::adopt_lock);
  (void)lk;  // suppress "unused variable" warning
  SUCCEED();
}

// explicit scoped_lock(adopt_lock_t, Mutex1)
TEST(ScopedLockTest, CtorAdoptLock1)
{
  MockMutex mtx1;
  mtx1.locked = true;
  {
    yamc::scoped_lock<MockMutex> lk(std::adopt_lock, mtx1);
  }
  EXPECT_FALSE(mtx1.locked);
}

// explicit scoped_lock(adopt_lock_t, Mutex1, Mutex2)
TEST(ScopedLockTest, CtorAdoptLock2)
{
  MockMutex mtx1, mtx2;
  mtx1.locked = mtx2.locked = true;
  {
    yamc::scoped_lock<MockMutex, MockMutex> lk(std::adopt_lock, mtx1, mtx2);
  }
  EXPECT_FALSE(mtx1.locked);
  EXPECT_FALSE(mtx2.locked);
}

// avoid deadlock
TEST(ScopedLockTest, AvoidDeadlock)
{
  SETUP_STEPTEST;
  yamc::test::phaser phaser(2);
  using Mutex1 = std::mutex;
  using Mutex2 = std::recursive_mutex;
  Mutex1 mtx1;
  Mutex2 mtx2;
  yamc::test::task_runner(2, [&](std::size_t id) {
    auto ph = phaser.get(id);
    switch (id) {
    case 0:
      // lock order: 1->2
      ASSERT_NO_THROW(mtx1.lock());
      ASSERT_NO_THROW(mtx2.lock());
      ph.await();     // p1
      {
        yamc::scoped_lock<Mutex1, Mutex2> lk(std::adopt_lock, mtx1, mtx2);
        EXPECT_STEP(1);
        ph.await();   // p2
      }
      ph.await();     // p3
      // lock order: 2->1
      ASSERT_NO_THROW(mtx2.lock());
      ASSERT_NO_THROW(mtx1.lock());
      ph.await();     // p4
      {
        yamc::scoped_lock<Mutex1, Mutex2> lk(std::adopt_lock, mtx1, mtx2);
        EXPECT_STEP(3);
        ph.await();   // p5
      }
      break;
    case 1:
      ph.await();     // p1
      ph.advance(1);  // p2
      {
        yamc::scoped_lock<Mutex1, Mutex2> lk(mtx1, mtx2);
        EXPECT_STEP(2);
      }
      ph.await();     // p3
      ph.await();     // p4
      ph.advance(1);  // p5
      {
        yamc::scoped_lock<Mutex1, Mutex2> lk(mtx1, mtx2);
        EXPECT_STEP(4);
      }
      break;
    }
  });
}
