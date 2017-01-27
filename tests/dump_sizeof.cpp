#include <cstdio>
#include <condition_variable>
#include <mutex>
#include <thread>
#include "naive_spin_mutex.hpp"
#include "ttas_spin_mutex.hpp"
#include "checked_mutex.hpp"
#include "fair_mutex.hpp"
#include "alternate_mutex.hpp"


#define DUMP(T) std::printf("%s %zu\n", #T, sizeof(T))


int main()
{
  DUMP(int);
  DUMP(std::size_t);
  DUMP(std::thread::id);
  DUMP(std::condition_variable);

  DUMP(std::mutex);
  DUMP(std::timed_mutex);
  DUMP(std::recursive_mutex);
  DUMP(std::recursive_timed_mutex);

  DUMP(yamc::spin::mutex);
  DUMP(yamc::spin_weak::mutex);
  DUMP(yamc::spin_ttas::mutex);
  DUMP(yamc::checked::mutex);
  DUMP(yamc::checked::timed_mutex);
  DUMP(yamc::checked::recursive_mutex);
  DUMP(yamc::checked::recursive_timed_mutex);
  DUMP(yamc::fair::mutex);
  DUMP(yamc::fair::recursive_mutex);
  DUMP(yamc::fair::timed_mutex);
  DUMP(yamc::alternate::recursive_mutex);
  DUMP(yamc::alternate::timed_mutex);
  DUMP(yamc::alternate::recursive_timed_mutex);
  return 0;
}
