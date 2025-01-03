// #include "libfork/core.hpp"
#include "libfork/schedule.hpp"
#include "Engine.hpp"

int main() {
  
  lf::lazy_pool pool; // 4 worker threads
  Params ps;
  Engine* engine = new Engine(ps);

//   int fib_10 = lf::sync_wait(pool, fib, 10);
}