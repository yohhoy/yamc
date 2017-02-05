/*
 * yamc_backoff_spin.hpp
 *
 * MIT License
 *
 * Copyright (c) 2017 yohhoy
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#ifndef YAMC_BACKOFF_SPIN_HPP_
#define YAMC_BACKOFF_SPIN_HPP_

#include <thread>


/// default backoff spin policy
#ifndef YAMC_BACKOFF_SPIN_DEFAULT
#define YAMC_BACKOFF_SPIN_DEFAULT yamc::backoff::exponential<>
#endif


/// initial count for yamc::backoff::exponential<>
#ifndef YAMC_BACKOFF_EXPONENTIAL_INITCOUNT
#define YAMC_BACKOFF_EXPONENTIAL_INITCOUNT 4000
#endif


namespace yamc {

/*
 * backoff algorithm for spinlock basic_mutex<BackoffPolicy>
 *
 * - yamc::backoff::exponential<InitCount>
 * - yamc::backoff::yield
 * - yamc::backoff::busy
 */
namespace backoff {

/// exponential backoff spin policy
template <
  unsigned int InitCount = YAMC_BACKOFF_EXPONENTIAL_INITCOUNT
>
struct exponential {
  struct state {
    unsigned int initcount = InitCount;
    unsigned int counter = InitCount;
  };

  static void wait(state& s)
  {
    if (s.counter == 0) {
      // yield thread at exponential decreasing interval
      std::this_thread::yield();
      s.initcount = (s.initcount >> 1) | 1;
      s.counter = s.initcount;
    }
    --s.counter;
  }
};


/// simple yield thread policy
struct yield {
  struct state {};

  static void wait(state&)
  {
    std::this_thread::yield();
  }
};


/// 'real' busy-loop policy
///
/// ATTENTION:
///   This policy may waste your CPU time.
///
struct busy {
  struct state {};
  static void wait(state&) {}
};

} // namespace backoff
} // namespace yamc

#endif
