# yamc
[![Build Status](https://travis-ci.org/yohhoy/yamc.svg?branch=master)](https://travis-ci.org/yohhoy/yamc)

C++ mutexes (mutual exclusion primitives for multi-threading) collections.
This is header-only, cross-platform, no external dependency C++ library.

"yamc" is an acronym for Yet Another (or Yohhoy's Ad-hoc) Mutex Collections ;)


# Mutex characteristics
- `yamc::spin::mutex`: TAS spinlock, non-recursive
- `yamc::spin_weak::mutex`: TAS spinlock, non-recursive
- `yamc::spin_ttas::mutex`: TTAS spinlock, non-recursive
- `yamc::checked::mutex`: requirements debugging, non-recursive
- `yamc::checked::recursive_mutex`: requirements debugging, recursive


# Licence
MIT License
