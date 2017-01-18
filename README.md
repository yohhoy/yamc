# yamc
C++ mutexes (mutual exclusion primitives for multi-threading) collections.
This is header-only C++ library and its implementation depends only C++11 Standard Library.

"yamc" is acronym Yet Another (or Yohhoy's Adhoc) Mutex Collections ;)


# Mutex characteristics
- `yamc::spin::mutex`: TAS spinlock, non-recursive
- `yamc::spin_weak::mutex`: TAS spinlock, non-recursive
- `yamc::spin_ttas::mutex`: TTAS spinlock, non-recursive
- `yamc::checked::mutex`: requirements debugging, non-recursive
- `yamc::checked::recursive_mutex`: requirements debugging, recursive


# Licence
MIT License
