/*
 * lock_test.cpp
 */
#include <mutex>
#include <type_traits>
#include "gtest/gtest.h"
#include "yamc_scoped_lock.hpp"
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
  yamc::test::phaser phaser(2);
  using Mutex1 = std::mutex;
  using Mutex2 = std::recursive_mutex;
  Mutex1 mtx1;
  Mutex2 mtx2;
  int step = 0;
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
