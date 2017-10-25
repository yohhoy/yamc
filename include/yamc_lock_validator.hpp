/*
 * yamc_lock_validator.hpp
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
#ifndef YAMC_LOCK_VALIDATOR_HPP_
#define YAMC_LOCK_VALIDATOR_HPP_

#include <cassert>
#include <cstdint>
#include <algorithm>
#include <iostream>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>


#ifndef YAMC_CHECK_VERBOSE
#define YAMC_CHECK_VERBOSE 0
#endif

namespace yamc {

/*
 * lock validator for checked mutex
 *
 * - yamc::validator::deadlock
 * - yamc::validator::null
 */
namespace validator {

class deadlock {
private:
  struct entry {
    std::size_t mid;
    std::vector<std::thread::id> owners;
    std::vector<std::thread::id> waiters;
  };
  using mutexmap_type = std::unordered_map<uintptr_t, entry>;

  struct table_ref {
    std::unique_lock<std::mutex> lk;
    mutexmap_type& mutexmap;
    std::size_t& counter;
  };

  static table_ref global_table()
  {
    static std::mutex global_guard;
    static mutexmap_type global_mutexmap;
    static std::size_t global_counter = 0;
    return { std::unique_lock<std::mutex>(global_guard), global_mutexmap, global_counter };
  }

  template <typename T>
  static void remove_elem(std::vector<T>& vec, T value)
  {
    vec.erase(std::remove(vec.begin(), vec.end(), value), vec.end());
  }

  static bool find_closepath(const mutexmap_type& mutexmap, uintptr_t end_mkey, std::thread::id tid)
  {
    for (const auto& e : mutexmap) {
      if (std::find(e.second.owners.begin(), e.second.owners.end(), tid) == e.second.owners.end())
        continue;
      if (e.first == end_mkey)
        return true;  // found close path
      for (const auto& next_tid : e.second.waiters) {
        if (find_closepath(mutexmap, end_mkey, next_tid)) {
          return true;
        }
      }
    }
    return false;
  }

  static void dump_mutexmap(std::ostream& os, const mutexmap_type& mutexmap)
  {
    for (const auto& e : mutexmap) {
      os << "  Mutex#" << e.second.mid << ": owners={";
      int i = 0;
      for (const auto& id : e.second.owners) {
        os << (i++ ? "," : "") << id;
      }
      os << "} waiters={";
      i = 0;
      for (const auto& id : e.second.waiters) {
        os << (i++ ? "," : "") << id;
      }
      os << "}\n";
    }
    os << std::endl;
  }

public:
  static void ctor(uintptr_t mkey)
  {
    auto&& table = global_table();
    table.mutexmap[mkey] = { ++table.counter, {}, {} };
  }

  static void dtor(uintptr_t mkey)
  {
    auto&& table = global_table();
    table.mutexmap.erase(mkey);
  }

  static void locked(uintptr_t mkey, std::thread::id tid, bool shared)
  {
    auto&& table = global_table();
    table.mutexmap[mkey].owners.push_back(tid);
#if YAMC_CHECK_VERBOSE
    std::cout << "Thread#" << tid << " acquired Mutex#" << table.mutexmap[mkey].mid
      << " " << (shared ? "shared-lock" : "lock") << '\n';
    dump_mutexmap(std::cout, table.mutexmap);
#else
    (void)shared;  // suppress "unused variable" warning
#endif
  }

  static void unlocked(uintptr_t mkey, std::thread::id tid, bool shared)
  {
    auto&& table = global_table();
    remove_elem(table.mutexmap[mkey].owners, tid);
#if YAMC_CHECK_VERBOSE
    std::cout << "Thread#" << tid << " released Mutex#" << table.mutexmap[mkey].mid
      << " " << (shared ? "shared-lock" : "lock") << '\n';
    dump_mutexmap(std::cout, table.mutexmap);
#else
    (void)shared;  // suppress "unused variable" warning
#endif
  }

  static bool enqueue(uintptr_t mkey, std::thread::id tid, bool shared)
  {
    auto&& table = global_table();
    table.mutexmap[mkey].waiters.push_back(tid);
    if (find_closepath(table.mutexmap, mkey, tid)) {
      // detect deadlock
      std::cout << "Thread#" << tid << " wait for Mutex#" << table.mutexmap[mkey].mid
        << " " << (shared ? "shared-lock" : "lock") << '\n';
      dump_mutexmap(std::cout, table.mutexmap);
      std::cout << "==== DEADLOCK DETECTED ====" << std::endl;
      return false;
    }
#if YAMC_CHECK_VERBOSE
    std::cout << "Thread#" << tid << " wait for Mutex#" << table.mutexmap[mkey].mid
      << " " << (shared ? "shared-lock" : "lock") << '\n';
    dump_mutexmap(std::cout, table.mutexmap);
#endif
    return true;
  }

  static void dequeue(uintptr_t mkey, std::thread::id tid)
  {
    auto&& table = global_table();
    remove_elem(table.mutexmap[mkey].waiters, tid);
  }
};


class null {
public:
  static void ctor(uintptr_t) {}
  static void dtor(uintptr_t) {}
  static void locked(uintptr_t, std::thread::id, bool) {}
  static void unlocked(uintptr_t, std::thread::id, bool) {}
  static bool enqueue(uintptr_t, std::thread::id, bool) { return true; }
  static void dequeue(uintptr_t, std::thread::id) {}
};


} // namespace validator
} // namespace yamc

#endif
