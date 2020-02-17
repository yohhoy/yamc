# yamc
[![Build Status](https://travis-ci.org/yohhoy/yamc.svg?branch=master)](https://travis-ci.org/yohhoy/yamc)
[![Build status](https://ci.appveyor.com/api/projects/status/omke97drkdcmntfh/branch/master?svg=true)](https://ci.appveyor.com/project/yohhoy/yamc/branch/master)
[![MIT License](http://img.shields.io/badge/license-MIT-blue.svg?style=flat)](LICENSE)

C++ mutex (mutual exclusion primitive for multi-threading) collections.
This is header-only, cross-platform, no external dependency C++11 library.

"yamc" is an acronym for Yet Another (or Yohhoy's Ad-hoc) Mutex Collections ;)


# Description
This library includes:
- Spinlock mutex, support configurable backoff algorithm.
- Shared mutex for readers-writer locking in C++11, support reader-prefer/writer-prefer scheduling.
- Checked mutex for debugging, compatible with requirements in C++11/14/17 Standard.
- Fair mutex and fair shared mutex, support FIFO scheduling to prevent from starvation.
- `shared_lock<Mutex>`, `scoped_lock<Mutexes...>` utilities in C++14/17 Standard.
- Wrapper class of platform native mutex-like synchronization primitives.
- C++20 synchronization primitives; `counting_semaphore`, `latch`, `barrier`.


## Example
All mutex types in this library are compatible with corresponding mutex types in C++ Standard Library.
The following toy example use spinlock mutex (`yamc::spin_ttas::mutex`) and scoped locking by [`std::lock_guard<>`][std_lockguard].

```cpp
#include <mutex>  // std::lock_guard<>
#include "ttas_spin_mutex.hpp"

template <typename T>
class ValueHolder {
  // declare mutex type for this class implementation
  using MutexType = yamc::spin_ttas::mutex;

  T value_;
  mutable MutexType guard_;  // guard to value_ access

public:
  ValueHolder(const T& v = T())
    : value_(v) {}

  void set(const T& v)
  {
    std::lock_guard<MutexType> lk(guard_);  // acquire lock
    value_ = v;
  }

  T get() const
  {
    std::lock_guard<MutexType> lk(guard_);  // acquire lock
    return value_;
  }
};
```

[std_lockguard]: http://en.cppreference.com/w/cpp/thread/lock_guard


## Mutex characteristics
This mutex collections library provide the following types:

- `yamc::spin::mutex`: TAS spinlock, non-recursive
- `yamc::spin_weak::mutex`: TAS spinlock, non-recursive
- `yamc::spin_ttas::mutex`: TTAS spinlock, non-recursive
- `yamc::checked::mutex`: requirements debugging, non-recursive
- `yamc::checked::timed_mutex`: requirements debugging, non-recursive, support timeout
- `yamc::checked::recursive_mutex`: requirements debugging, recursive
- `yamc::checked::recursive_timed_mutex`: requirements debugging, recursive, support timeout
- `yamc::checked::shared_mutex`: requirements debugging, RW locking, non-recursive
- `yamc::checked::shared_timed_mutex`: requirements debugging, RW locking, non-recursive, support timeout
- `yamc::fair::mutex`: fairness, non-recursive
- `yamc::fair::recursive_mutex`: fairness, recursive
- `yamc::fair::timed_mutex`: fairness, non-recursive, support timeout
- `yamc::fair::recursive_timed_mutex`: fairness, recursive, support timeout
- `yamc::fair::shared_mutex`: fairness, RW locking, non-recursive
- `yamc::fair::shared_timed_mutex`: fairness, RW locking, non-recursive, support timeout
- `yamc::alternate::recursive_mutex`: recursive
- `yamc::alternate::timed_mutex`: non-recursive, support timeout
- `yamc::alternate::recursive_timed_mutex`: recursive, support timeout
- `yamc::alternate::shared_mutex`: RW locking, non-recursive
- `yamc::alternate::shared_timed_mutex`: RW locking, non-recursive, support timeout

These mutex types fulfill corresponding mutex semantics in C++ Standard.
You can replace type `std::mutex` to `yamc::*::mutex`, `std::recursive_mutex` to `yamc::*::recursive_mutex` likewise, except some special case.
_Note:_ [`std::mutex`'s default constructor][mutex_ctor] is constexpr, but `yamc::*::mutex` is not.
All mutex types in C++ Standard are [standard-layout][standardlayout] class, but not all types in `yamc` namespace are.

C++11/14/17 Standard Library define variable mutex types:

- [`std::mutex`][std_mutex]: non-recursive, support static initialization
- [`std::timed_mutex`][std_tmutex]: non-recursive, support timeout
- [`std::recursive_mutex`][std_rmutex]: recursive
- [`std::recursive_timed_mutex`][std_rtmutex]: recursive, support timeout
- [`std::shared_mutex`][std_smutex]: RW locking, non-recursive (C++17 or later)
- [`std::shared_timed_mutex`][std_stmutex]: RW locking, non-recursive, support timeout (C++14 or later)

[mutex_ctor]: http://en.cppreference.com/w/cpp/thread/mutex/mutex
[standardlayout]: http://en.cppreference.com/w/cpp/concept/StandardLayoutType
[std_mutex]: http://en.cppreference.com/w/cpp/thread/mutex
[std_tmutex]: http://en.cppreference.com/w/cpp/thread/timed_mutex
[std_rmutex]: http://en.cppreference.com/w/cpp/thread/recursive_mutex
[std_rtmutex]: http://en.cppreference.com/w/cpp/thread/recursive_timed_mutex
[std_smutex]: http://en.cppreference.com/w/cpp/thread/shared_mutex
[std_stmutex]: http://en.cppreference.com/w/cpp/thread/shared_timed_mutex


## Which mutex should I use?
Basically, you _should_ use `std::mutex` or variants of C++ Standard Library in your products.
Period.

- When you debug misuse of mutex object, checked mutex in `yamc::checked::*` will help you.
- When you _really_ need spinlock mutex, I suppose `yamc::spin_ttas::mutex` may be best choice.
- When you _actually_ need fairness of locking order, try to use fair mutex in `yamc::fair::*`.
- Mutex in `yamc::alternate::*` has the same semantics of C++ Standard mutex, no additional features.
- When your compiler doesn't support C++14/17 Standard Library, shared mutex in `yamc::alternate::*` and `yamc::shared_lock<Mutex>` which emulate C++14 [`std::shared_lock<Mutex>`][std_sharedlock] are useful.

[std_sharedlock]: http://en.cppreference.com/w/cpp/thread/shared_lock


## Wrapper of platform native mutex
For POSIX-compatible platforms (see Note below):
- `yamc::posix::native_mutex` for native [Mutex][posix_mutex] (`pthread_mutex_t` type).
- `yamc::posix::native_recursive_mutex` for [Mutex][posix_mutex] with recursive semantics (`pthread_mutex_t` type).
- `yamc::posix::rwlock` for [Read-Write Lock][posix_rwlock] (`pthread_rwlock_t` type).
- `yamc::posix::spinlock` for [Spin Lock][posix_spinlock] (`pthread_spinlock_t` type).

_Note:_ Some platform (at least macOS) does not provide timed locking functions, spinlock primitives in POSIX Standard.

For Windows OS platform:
- `yamc::win::native_mutex` for native [Mutex object][win_mutex] (`HANDLE` type).
- `yamc::win::critial_section` for [Critical Section object][win_csobj] (`CRITICAL_SECTION` type).
- `yamc::win::slim_rwlock` for [Slim Reader/Writer(SRW) Locks][win_srwlock] (`SRWLOCK` type).

For macOS/iOS platform:
- `yamc::apple::unfair_lock` for [Unfair Lock][apple_oslock] (`os_unfair_lock_t` type).

[posix_mutex]: https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap03.html#tag_03_234
[posix_rwlock]: https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap03.html#tag_03_311
[posix_spinlock]: https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap03.html#tag_03_364
[win_mutex]: https://docs.microsoft.com/windows/win32/sync/mutex-objects
[win_csobj]: https://docs.microsoft.com/windows/win32/sync/critical-section-objects
[win_srwlock]: https://docs.microsoft.com/windows/win32/sync/slim-reader-writer--srw--locks
[apple_oslock]: https://developer.apple.com/documentation/os/os_unfair_lock


## C++20 synchronization primitives
This library also provides a part of C++20 synchronization primitives emulation.
These primitives have the same interfaces and emulate runtime behaviors described in C++20 Standard Library specification.

- `<semaphore>` header
    - `counting_semaphore<N>` is [counting semaphore][semaphore].
    - `binary_semaphore` is binary semaphore; alias of `counting_semaphore<1>`.
- `<latch>` header
    - `latch` is countdown latch; one-time rendezvous point.
- `<barrier>` header
    - `barrier` is [cyclic barrier][barrier] with completion handler; reusable rendezvous point.

There are two categories of the semaphore implementation:
- "Generic": Cross-platform with C++11 primitives only, inefficient compared to "Native" version.
- "Native": High performance, platform dependent with POSIX/macOS/Windows native APIs.

[semaphore]: https://en.wikipedia.org/wiki/Semaphore_(programming)
[barrier]: https://en.wikipedia.org/wiki/Barrier_(computer_science)


## Requirements
A C++ compiler and standard library support C++11.
No need for external library.

_Note:_ The implementation of this library depends on C++11 Standard threading primitives only `std::mutex`, [`std::condition_variable`][std_condvar] and [`std::atomic<T>`][std_atomic].
This means that you can use shared mutex variants (`shared_mutex`, `shared_timed_mutex`) with C++11 compiler which doesn't not support C++14/17 yet.

CI building and unit-testing on the following environments:
- Linux/G++ 5.4
- Linux/Clang 3.7
- macOS/Clang (Xcode 8.3)
- Windows/MSVC 14.0 (Visual Studio 2015)

[std_condvar]: http://en.cppreference.com/w/cpp/thread/condition_variable
[std_atomic]: http://en.cppreference.com/w/cpp/atomic/atomic


# Tweaks
## Busy waiting in spinlock mutex
The spinlock mutexes use an exponential backoff algorithm in busy waiting to acquire lock as default.
These backoff algorithm of spinlock `mutex`-es are implemented with policy-based template class `basic_mutex<BackoffPolicy>`.
You can tweak the algorithm by specifying `BackoffPolicy` when you instantiate spinlock mutex type, or define the following macros to change default behavior of all spinlock mutex types.

Customizable macros:

- `YAMC_BACKOFF_SPIN_DEFAULT`: BackoffPolicy of spinlock mutex types. Default policy is `yamc::backoff::exponential<>`.
- `YAMC_BACKOFF_EXPONENTIAL_INITCOUNT`: An initial count of `yamc::backoff::exponential<N>` policy class. Default value is `4000`.

Pre-defined BackoffPolicy classes:

- `yamc::backoff::exponential<N>`: An exponential backoff waiting algorithm, `N` denotes initial count. Yield the thread at an exponential decaying intervals in busy waiting loop.
- `yamc::backoff::yield`: Always yield the thread by calling [`std::this_thread::yield()`][yield].
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


## Readers-Writer lock by shared mutex
The shared mutex types provide "[Readers-Writer lock][rwlock]" (a.k.a. "Shared-Exclusive lock") semantics.
They implement data sharing mechanism between multiple-readers and single-writer threads.
Multiple threads can acquire shared lock to concurrently read shared data, or single thread can acquire exclusive lock to modify shared data.
When readers and writers threads try to acquire lock simultaneously, there are some scheduling algorithm that determinate which thread can acquire next lock.

These scheduling algorithm of shared mutex types are implemented with policy-based template class `basic_shared_(timed_)mutex<RwLockPolicy>`, except `yamc::fair::shared_(timed_)mutex` which implement fairness locking between readers and writers.
You can tweak the algorithm by specifying `RwLockPolicy` when you instantiate shared mutex type, or define the following macro to change default behavior of these shared mutex types.

Customizable macro:

- `YAMC_RWLOCK_SCHED_DEFAULT`: `RwLockPolicy` of shared mutex types. Default policy is `yamc::rwlock::ReaderPrefer`.

Pre-defined RwLockPolicy classes:

- `yamc::rwlock::ReaderPrefer`: Reader prefer locking.
  While any reader thread owns shared lock, subsequent other reader threads can immediately acquire shared lock, but subsequent writer threads will be blocked until all reader threads release shared lock.
  This policy might introduce "Writer Starvation" if reader threads continuously hold shared lock.
- `yamc::rwlock::WriterPrefer`: Writer prefer locking.
  While any reader thread owns shared lock and there are a waiting writer thread, subsequent other reader threads which try to acquire shared lock are blocked until writer thread's work is done.
  This policy might introduce "Reader Starvation" if writer threads continuously try to acquire exclusive lock.

Sample code:
```cpp
// change default RwLockPolicy
#define YAMC_RWLOCK_SCHED_DEFAULT yamc::rwlock::WriterPrefer
#include "alternate_shared_mutex.hpp"

// define shared mutex type with ReaderPrefer policy
using MySharedMutex = yamc::alternate::basic_shared_mutex<yamc::rwlock::ReaderPrefer>;
```

[rwlock]: https://en.wikipedia.org/wiki/Readers%E2%80%93writer_lock


## Readers-Writer lock fairness
The shared mutex types in `yamc::fair` namespace implement "Task/Phase-fair Readers-Writer lock", which ensure not to cause writer starvation nor reader starvation.
That fair shared mutex has FIFO(First-In-First-Out) queue of threads wait for lock acquisition, and switch the turn that threads acquire exclusive lock or shared locks.
For example, 4 threads try to acquire lock in W1 -> R2 -> R3 -> W4 order (W=exclusive lock / R=shared lock), each threads will acquire the lock in that order.
In this case, 2 reader threads can concurrently acquire R2 and R3.

These scheduling algorithm of fair shared mutex types are implemented with policy-based template class `yamc::fair::basic_shared_(timed_)mutex<RwLockFairness>`.
You can tweak the algorithm by specifying `RwLockFairness` when you instantiate fair shared mutex type, or define the following macro to change default behavior of these fair shared mutex types.

Customizable macro:

- `YAMC_RWLOCK_FAIRNESS_DEFAULT`: `RwLockFairness` of fair shared mutex types. Default policy is `yamc::rwlock::PhaseFairness`.

Pre-defined RwLockFairness classes:

- `yamc::rwlock::TaskFairness`: Task-fairness RW locking schedule, which provides simple FIFO lock ordering. When lock request order is W1 -> R2 -> W3 -> R4, each waiting threads will acquire RW lock in the request order.
- `yamc::rwlock::PhaseFairness`: Phase-fairness RW locking schedule, which provides "phasing" FIFO lock ordering. When lock request order is W1 -> R2 -> W3 -> R4, the acquisition order will be W1 -> R2,R4 -> W3. Because releasing exclusive lock (W1) switches the RW phase, so all waiting reader threads acquire shared locks (R2,R4) concurrently.


## Check requirements of mutex operation
Some operation of mutex type has pre-condition statement, for instance, the thread which call `m.unlock()` shall own its lock of mutex `m`.
C++ Standard say that the behavior is __undefined__ when your program violate any requirements.
This means incorrect usage of mutex might cause deadlock, data corruption, or anything wrong.

Checked mutex types which are defined in `yamc::checked::*` validate the following requirements on run-time:

- A thread that call `unlock()` SHALL own its lock. (_Unpaired Lock/Unlock_)
- For `mutex` and `timed_mutex`, a thread that call `lock()` or `try_lock` family SHALL NOT own its lock. (_Non-recursive Semantics_)
- When a thread destruct mutex object, all threads (include this thread) SHALL NOT own its lock. (_Abandoned Lock_)

Checked mutexes are designed for debugging purpose, so the operation on checked mutex have some overhead.
The default behavior is throwing [`std::system_error`][system_error] exception when checked mutex detect any violation.
If you `#define YAMC_CHECKED_CALL_ABORT 1` before `#include "checked_(shared_)mutex.hpp"`, checked mutex will call [`std::abort()`][abort] instead of throwing exception and the program immediately terminate.

[system_error]: http://en.cppreference.com/w/cpp/error/system_error
[abort]: http://en.cppreference.com/w/cpp/utility/program/abort


## Deadlock detection
Checked mutex types (`yamc::checked::*`) also provide "[Deadlock][deadlock] detection" by default.
The runtime deadlock detector tracks all lock ownership and waiting thread on checked mutexes, `lock()` and `lock_shared()` which cause deadlock methods will throw exception or abort the program (described in above section).

Such tracking increase additional runtime overhead, and will affect whole program progress.
To disable deadlock detection, `#define YAMC_CHECKED_DETECT_DEADLOCK 0` before `#include "checked_(shared_)mutex.hpp"`.

CAVEAT:
This feature depends on the intrinsic mechanism of `yamc::checked::*` mutexes.
The deadlock detector CAN NOT detect any deadlock come from other mutex types.
If you need to detect general deadlock, consider [Valgrind/Helgrind][helgrind] and [Clang/ThreadSanitizer][clang-tsan], etc.

[deadlock]: https://en.wikipedia.org/wiki/Deadlock
[helgrind]: http://valgrind.org/docs/manual/hg-manual.html
[clang-tsan]: https://clang.llvm.org/docs/ThreadSanitizer.html


# License
MIT License
