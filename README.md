# yamc
[![Build Status](https://travis-ci.org/yohhoy/yamc.svg?branch=master)](https://travis-ci.org/yohhoy/yamc)

C++ mutexes (mutual exclusion primitives for multi-threading) collections.
This is header-only, cross-platform, no external dependency C++ library.

"yamc" is an acronym for Yet Another (or Yohhoy's Ad-hoc) Mutex Collections ;)


# Mutex characteristics
This library provide the following mutex types.
All mutex types fulfil normal mutex or recursive mutex semantics in C++ Standard.
You can replace type `std::mutex` to `yamc::*::mutex`, `std::recursive_mutex` to `yamc::*::recursive_mutex` except some special case.
Note: [`std::mutex`'s default constructor][mutex_ctor] is constexpr, but `yamc::*::mutex` is not.

- `yamc::spin::mutex`: TAS spinlock, non-recursive
- `yamc::spin_weak::mutex`: TAS spinlock, non-recursive
- `yamc::spin_ttas::mutex`: TTAS spinlock, non-recursive
- `yamc::checked::mutex`: requirements debugging, non-recursive
- `yamc::checked::timed_mutex`: requirements debugging, non-recursive, support timeout
- `yamc::checked::recursive_mutex`: requirements debugging, recursive
- `yamc::checked::recursive_timed_mutex`: requirements debugging, recursive, support timeout
- `yamc::fair::mutex`: fairness, non-recursive
- `yamc::fair::recursive_mutex`: fairness, recursive
- `yamc::alternate::recursive_mutex`: recursive

C++11/14/17 Standard Library define variable mutex types:

- [`std::mutex`][std_mutex]: non-recursive, support static initialization
- [`std::timed_mutex`][std_tmutex]: non-recursive, support timeout
- [`std::recursive_mutex`][std_rmutex]: recursive
- [`std::recursive_timed_mutex`][std_rtmutex]: recursive, support timeout
- [`std::shared_mutex`][std_smutex]: RW locking, non-recursive (C++17 or later)
- [`std::shared_timed_mutex`][std_stmutex]: RW locking, non-recursive, support timeout (C++14 or later)

[mutex_ctor]: http://en.cppreference.com/w/cpp/thread/mutex/mutex
[std_mutex]: http://en.cppreference.com/w/cpp/thread/mutex
[std_tmutex]: http://en.cppreference.com/w/cpp/thread/timed_mutex
[std_rmutex]: http://en.cppreference.com/w/cpp/thread/recursive_mutex
[std_rtmutex]: http://en.cppreference.com/w/cpp/thread/recursive_timed_mutex
[std_smutex]: http://en.cppreference.com/w/cpp/thread/shared_mutex
[std_stmutex]: http://en.cppreference.com/w/cpp/thread/shared_timed_mutex


# Tweaks
## Busy waiting for spinlock mutex
The spinlock mutexes use an exponential backoff algorithm in busy waiting to acquire lock as default.
These backoff algorithm of spinlock `mutex`-es are implemented with policy-based template class `basic_mutex<BackoffPolicy>`.
You can tweak the algorithm by specifying BackoffPolicy when you instantiate spinlock mutex type, or define the following macros to change default behavior of all spinlock mutex types.

Customizable macro:

- `YAMC_BACKOFF_SPIN_DEFAULT`: BackoffPolicy of spinlock mutexes. Default policy is `yamc::backoff::exponential<>`.
- `YAMC_BACKOFF_EXPONENTIAL_INITCOUNT`: An initial conut of `yamc::backoff::exponential<N>` policy class. Default value is `4000`.

Pre-defined BackoffPolicy classes:

- `yamc::backoff::exponential<N>`: An exponential backoff waiting algorithm, `N` denotes initial count.
- `yamc::backoff::yield`: Always yield thread by calling `std::this_thread::yield()`.
- `yamc::backoff::busy`: Do nothing. Real busy-loop _may_ waste CPU time and increase power consumtion.

Sample code:
```cpp
// change default BackoffPolicy
#define YAMC_BACKOFF_SPIN_DEFAULT yamc::backoff::yield
#inlucde "naive_spin_mutex.hpp"

// define spinlock mutex type with exponential backoff (initconut=1000)
using MyMutex = yamc::spin::basic_mutex<yamc::backoff::exponential<1000>>;
```


# Licence
MIT License
