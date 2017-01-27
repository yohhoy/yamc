# yamc
[![Build Status](https://travis-ci.org/yohhoy/yamc.svg?branch=master)](https://travis-ci.org/yohhoy/yamc)
[![Build status](https://ci.appveyor.com/api/projects/status/omke97drkdcmntfh/branch/master?svg=true)](https://ci.appveyor.com/project/yohhoy/yamc/branch/master)

C++ mutexes (mutual exclusion primitives for multi-threading) collections.
This is header-only, cross-platform, no external dependency C++ library.
A compiler which support C++11 are required.

CI building and unit-testing with C++ compilers:
- Linux/G++ 5.4
- Linux/Clang 3.7
- OSX/Clang (Xcode 8.2)
- Windows/MSVC 14.0 (Visual Studio 2015)

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
- `yamc::alternate::timed_mutex`: non-recursive, support timeout
- `yamc::alternate::recursive_timed_mutex`: recursive, support timeout

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
## Busy waiting in spinlock mutex
The spinlock mutexes use an exponential backoff algorithm in busy waiting to acquire lock as default.
These backoff algorithm of spinlock `mutex`-es are implemented with policy-based template class `basic_mutex<BackoffPolicy>`.
You can tweak the algorithm by specifying BackoffPolicy when you instantiate spinlock mutex type, or define the following macros to change default behavior of all spinlock mutex types.

Customizable macros:

- `YAMC_BACKOFF_SPIN_DEFAULT`: BackoffPolicy of spinlock mutexes. Default policy is `yamc::backoff::exponential<>`.
- `YAMC_BACKOFF_EXPONENTIAL_INITCOUNT`: An initial count of `yamc::backoff::exponential<N>` policy class. Default value is `4000`.

Pre-defined BackoffPolicy classes:

- `yamc::backoff::exponential<N>`: An exponential backoff waiting algorithm, `N` denotes initial count.
- `yamc::backoff::yield`: Always yield thread by calling [`std::this_thread::yield()`][yield].
- `yamc::backoff::busy`: Do nothing. Real busy-loop _may_ waste CPU time and increase power consumption.

Sample code:
```cpp
// change default BackoffPolicy
#define YAMC_BACKOFF_SPIN_DEFAULT yamc::backoff::yield
#include "naive_spin_mutex.hpp"

// define spinlock mutex type with exponential backoff (initconut=1000)
using MyMutex = yamc::spin::basic_mutex<yamc::backoff::exponential<1000>>;
```

[yield]: http://en.cppreference.com/w/cpp/thread/yield


## Check requirements of mutex operation
Some operation of mutex type has pre-condition statement, for instance, the thread which call `m.unlock()` shall own its lock of mutex `m`.
C++ Standard say that the behavior is _undefined_ when your program violate any requirements.
This means incorrect usage of mutex might cause deadlock, data corruption, or anything wrong.

Checked mutex types which are defined `yamc::checked::*` validate the following requirements on run-time:

- A thread that call `unlock()` SHALL own its lock. (unpaired Lock/Unlock)
- For `mutex` and `timed_mutex`, a thread that call `lock()` or `try_lock` family SHALL NOT own its lock. (non-recursive semantics)
- When a thread destruct mutex object, all threads (include this thread) SHALL NOT own its lock. (abandoned lock)

An operation on checked mutex have some overhead, so they are designed for debugging purpose only.
The default behavior is throwing [`std::system_error`][system_error] exception when checked mutex detect any violation.
If you `#define YAMC_CHECKED_CALL_ABORT 1` before `#include "checked_mutex.hpp"`, checked mutex call `std::abort()` and the program will immediately terminate.

[system_error]: http://en.cppreference.com/w/cpp/error/system_error


# Licence
MIT License
