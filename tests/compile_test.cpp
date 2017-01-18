#include <mutex>  // lock_guard, unique_lock
#include "naive_spin_mutex.hpp"
#include "ttas_spin_mutex.hpp"
#include "checked_mutex.hpp"


template <typename Mutex>
void test_requirements()
{
  Mutex mtx;
  {
    std::lock_guard<Mutex> lk(mtx);
  }
  {
    std::unique_lock<Mutex> lk(mtx);
  }
  {
    std::unique_lock<Mutex> lk(mtx, std::try_to_lock);
  }
}

int main()
{
  test_requirements<std::mutex>();
  test_requirements<yamc::spin::mutex>();
  test_requirements<yamc::spin_weak::mutex>();
  test_requirements<yamc::spin_ttas::mutex>();
  test_requirements<yamc::checked::mutex>();
  test_requirements<yamc::checked::recursive_mutex>();
  return 0;
}
