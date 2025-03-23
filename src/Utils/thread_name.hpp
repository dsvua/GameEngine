#pragma once
/// Generate human-readable thread names for various executors. Used by examples
/// that demonstrate switching tasks between executors.

#include "tmc/ex_cpu.hpp"

#include <sstream>
#include <string>

namespace this_thread {
// I'd like to make this constinit, but it doesn't work on current version of
// libstdc++. Works fine with libc++ though.
inline thread_local std::string threadName{};
inline thread_local size_t threadId{};
} // namespace this_thread

/**
 * Sets up thread name initialization hook for CPU executor threads.
 * This must be called before calling init() on the executor.
 * @param Executor The CPU executor to set up the hook for
 */
inline void hookInitExCpuThreadName(tmc::ex_cpu& Executor) {
    Executor.set_thread_init_hook([](size_t Slot) {
        this_thread::threadName = std::string("cpu thread ") + std::to_string(Slot);
    });
}

/**
 * Sets up thread ID initialization hook for CPU executor threads.
 * This must be called before calling init() on the executor.
 * @param Executor The CPU executor to set up the hook for
 */
inline void hookInitExCpuThreadId(tmc::ex_cpu& Executor) {
    Executor.set_thread_init_hook([](size_t Slot) {
        this_thread::threadId = Slot;
    });
}

/**
 * Gets the current thread's name.
 * This has been observed to produce the wrong results (always prints the same
 * thread name) on Clang 16, due to incorrectly caching thread_locals across
 * suspend points. The issue has been resolved in Clang 17.
 * @return String containing the thread name
 */
inline std::string getThreadName() {
    std::string tmc_tid = this_thread::threadName;
    if (!tmc_tid.empty()) {
        return tmc_tid;
    } else {
        std::ostringstream id;
        id << std::this_thread::get_id();
        return "external thread " + id.str();
    }
}

/**
 * Gets the current thread's ID as assigned by the executor.
 * @return Size_t containing the thread ID
 */
inline size_t getThreadId() {
    return this_thread::threadId;
}

/**
 * Prints the current thread's name to standard output.
 */
inline void printThreadName() {
    std::printf("%s\n", getThreadName().c_str());
}
