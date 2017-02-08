/*
 * perf_rwlock.cpp
 */
#include <cmath>
#include <atomic>
#include <chrono>
#include <iostream>
#include <iterator>
#include <numeric>
#include <mutex>
#include <thread>
#include <vector>
#include "alternate_shared_mutex.hpp"
#include "fair_mutex.hpp"
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

  // average [count/sec/thread]
  double wavg = (double)nwissue / cfg.nwriter / elapsed;
  double ravg = (double)nrissue / cfg.nreader / elapsed;
  // SD(standard deviation) [count/sec/thread]
  double wsd = std::sqrt(std::accumulate(counters.begin(), pivot, 0.,
                                          [wavg, elapsed](double acc, std::size_t v) {
                                            return acc + (v / elapsed - wavg) * (v / elapsed - wavg);
                                          }) / cfg.nwriter);
  double rsd = std::sqrt(std::accumulate(pivot, counters.end(), 0.,
                                          [ravg, elapsed](double acc, std::size_t v) {
                                            return acc + (v / elapsed - ravg) * (v / elapsed - ravg);
                                          }) / cfg.nreader);

  // print result
  std::cout
    << cfg.nwriter << '\t' << nwissue << '\t' << wavg << '\t' << wsd << '\t'
    << cfg.nreader << '\t' << nrissue << '\t' << ravg << '\t' << rsd << std::endl;
}


template <typename Mutex>
void perform_lock_contention(const config& cfg)
{
  std::size_t nthread = cfg.nwriter + cfg.nreader;
  yamc::test::barrier gate(nthread + 1);
  std::vector<std::thread> thds;
  std::vector<std::size_t> counters(nthread);

  std::atomic<int> running = {1};
  Mutex mtx;

  // writer/reader threads
  for (std::size_t i = 0; i < nthread; i++) {
    std::size_t idx = i;
    thds.emplace_back([&,idx]{
      std::size_t ncount = 0;
      gate.await();  // start
      while (running.load(std::memory_order_relaxed)) {
        mtx.lock();
        PERF_DUMMY_TASK(PERF_WEIGHT_TASK)
        ++ncount;  // write/read op
        mtx.unlock();
        PERF_DUMMY_TASK(PERF_WEIGHT_WAIT)
        std::this_thread::yield();
      }
      gate.await();  // end
      counters[idx] = ncount;
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
  std::size_t nissue = std::accumulate(counters.begin(), counters.end(), 0u);

  // average [count/sec/thread]
  double avg = (double)nissue / nthread / elapsed;
  // SD(standard deviation) [count/sec/thread]
  double sd = std::sqrt(std::accumulate(counters.begin(), counters.end(), 0.,
                                         [avg, elapsed](double acc, std::size_t v) {
                                            return acc + (v / elapsed - avg) * (v / elapsed - avg);
                                         }) / nthread);

  // print result
  std::cout
    << nthread << '\t' << nissue << '\t' << avg << '\t' << sd << "\t-\t-\t-\t-" << std::endl;
}


void print_header(const char* title, unsigned nthread)
{
  std::cout
    << "# " << title
    << " ncpu=" << std::thread::hardware_concurrency() << " nthread=" << nthread
    << " task/wait=" << PERF_WEIGHT_TASK << "/" << PERF_WEIGHT_WAIT
    << " duration=" << PERF_DURATION.count() << std::endl;
}


template <typename Mutex>
void perf_lock(const char* title, unsigned nthread)
{
  print_header(title, nthread);
  std::cout << "# Wt/Rd\t[raw]\t[ops]\t[sd]\t-\t-\t-\t-" << std::endl;
  perform_lock_contention<Mutex>({ nthread, 0 });
  std::cout << "\n\n" << std::flush;
}


template <typename SharedMutex>
void perf_rwlock(const char* title, unsigned nthread)
{
  print_header(title, nthread);
  std::cout << "# Write\t[raw]\t[ops]\t[sd]\tRead\t[raw]\t[ops]\t[sd]" << std::endl;
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

  perf_lock<std::mutex>       ("StdMutex", nthread);
  perf_lock<yamc::fair::mutex>("FifoMutex", nthread);

  perf_rwlock<reader_prefer_shared_mutex>("ReaderPrefer", nthread);
  perf_rwlock<writer_prefer_shared_mutex>("WriterPrefer", nthread);
  perf_rwlock<yamc::fair::shared_mutex> ("PhaseFair", nthread);
}
