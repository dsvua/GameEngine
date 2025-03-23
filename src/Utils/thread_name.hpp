#pragma once
/// Generate human-readable thread names for various executors. Used by examples
/// that demonstrate switching tasks between executors.

#include "tmc/ex_cpu.hpp"

#include <sstream>
#include <string>

namespace this_thread {
// I'd like to make this constinit, but it doesn't work on current version of
// libstdc++. Works fine with libc++ though.
inline thread_local std::string thread_name{};
inline thread_local size_t thread_id{};
} // namespace this_thread

/// This must be called before calling init() on the executor.
inline void hook_init_ex_cpu_thread_name(tmc::ex_cpu& Executor) {
  Executor.set_thread_init_hook([](size_t Slot) {
    this_thread::thread_name = std::string("cpu thread ") + std::to_string(Slot);
  });
}

/// This must be called before calling init() on the executor.
inline void hook_init_ex_cpu_thread_id(tmc::ex_cpu& Executor) {
  Executor.set_thread_init_hook([](size_t Slot) {
    this_thread::thread_id = Slot;
  });
}

// This has been observed to produce the wrong results (always prints the same
// thread name) on Clang 16, due to incorrectly caching thread_locals across
// suspend points. The issue has been resolved in Clang 17.
inline std::string get_thread_name() {
  std::string tmc_tid = this_thread::thread_name;
  if (!tmc_tid.empty()) {
    return tmc_tid;
  } else {
    std::ostringstream id;
    id << std::this_thread::get_id();
    return "external thread " + id.str();
  }
}

inline size_t get_thread_id() {
  return this_thread::thread_id;
}

inline void print_thread_name() {
  std::printf("%s\n", get_thread_name().c_str());
}
