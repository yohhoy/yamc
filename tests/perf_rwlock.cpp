/*
 * perf_rwlock.cpp
 */
#include <atomic>
#include <chrono>
#include <iostream>
#include <iterator>
#include <numeric>
#include <thread>
#include <vector>
#include "alternate_shared_mutex.hpp"
#include "fair_shared_mutex.hpp"
#include "yamc_testutil.hpp"


// measurement duration
#define PERF_DURATION std::chrono::seconds(5)

#define PERF_WEIGHT_TASK 100
#define PERF_WEIGHT_WAIT 200

// dummy task (waste CPU instructions)
#define PERF_DUMMY_TASK(weight_) { volatile unsigned n = (weight_); while (--n); }


struct config {
  std::size_t nwriter;
  std::size_t nreader;
};


template <typename SharedMutex>
void perform_rwlock_contention(const config& cfg)
{
  yamc::test::barrier gate(cfg.nwriter + cfg.nreader + 1);
  std::vector<std::thread> thds;
  std::vector<std::size_t> counters(cfg.nwriter + cfg.nreader);

  std::atomic<int> running = {1};
  SharedMutex mtx;

  // setup writer threads
  for (std::size_t i = 0; i < cfg.nwriter; i++) {
    std::size_t idx = i;
    thds.emplace_back([&,idx]{
      std::size_t nwcount = 0;
      gate.await();  // start
      while (running.load(std::memory_order_relaxed)) {
        mtx.lock();
        PERF_DUMMY_TASK(PERF_WEIGHT_TASK)
        ++nwcount;  // write op
        mtx.unlock();
        PERF_DUMMY_TASK(PERF_WEIGHT_WAIT)
        std::this_thread::yield();
      }
      gate.await();  // end
      counters[idx] = nwcount;
    });
  }

  // setup reader threads
  for (std::size_t i = 0; i < cfg.nreader; i++) {
    std::size_t idx = cfg.nwriter + i;
    thds.emplace_back([&,idx]{
      std::size_t nrcount = 0;
      gate.await();  // start
      while (running.load(std::memory_order_relaxed)) {
        mtx.lock_shared();
        PERF_DUMMY_TASK(PERF_WEIGHT_TASK)
        ++nrcount;  // read op
        mtx.unlock_shared();
        PERF_DUMMY_TASK(PERF_WEIGHT_WAIT)
        std::this_thread::yield();
      }
      gate.await();  // end
      counters[idx] = nrcount;
    });
  }

  // run measurement
  yamc::test::stopwatch<> sw;
  gate.await();  // start
  std::this_thread::sleep_for(PERF_DURATION);
  running.store(0, std::memory_order_relaxed);
  gate.await();  // end
  double elapsed = (double)sw.elapsed().count() / 1000000.;  // [sec]

  // summarize write/read issue count
  for (auto& t : thds) {
    t.join();
  }
  auto pivot = counters.begin();
  std::advance(pivot, cfg.nwriter);
  std::size_t nwissue = std::accumulate(counters.begin(), pivot, 0u);
  std::size_t nrissue = std::accumulate(pivot, counters.end(), 0u);

  // print result
  std::cout
    << cfg.nwriter << '\t' << nwissue << '\t' << ((double)nwissue / elapsed / cfg.nwriter) << '\t'
    << cfg.nreader << '\t' << nrissue << '\t' << ((double)nrissue / elapsed / cfg.nreader) << std::endl;
}


template <typename SharedMutex>
void perf_run(const char* title, unsigned nthread)
{

  std::cout
    << "# " << title
    << " ncpu=" << std::thread::hardware_concurrency() << " nthread=" << nthread
    << " task/wait=" << PERF_WEIGHT_TASK << "/" << PERF_WEIGHT_WAIT
    << " duration=" << PERF_DURATION.count() << std::endl;

  std::cout << "# Write\t[raw]\t[ops]\tRead\t[raw]\t[ops]" << std::endl;
  for (unsigned nwt = 1; nwt < nthread; nwt++) {
    config cfg = { nwt, nthread - nwt };
    perform_rwlock_contention<SharedMutex>(cfg);
  }
  std::cout << "\n\n" << std::flush;
}


int main()
{
  unsigned nthread = 10;

  using reader_prefer_shared_mutex = yamc::alternate::basic_shared_mutex<yamc::rwlock::ReaderPrefer>;
  using writer_prefer_shared_mutex = yamc::alternate::basic_shared_mutex<yamc::rwlock::WriterPrefer>;
  perf_run<reader_prefer_shared_mutex>("ReaderPrefer", nthread);
  perf_run<writer_prefer_shared_mutex>("WriterPrefer", nthread);
  perf_run<yamc::fair::shared_mutex>("PhaseFair", nthread);
}
