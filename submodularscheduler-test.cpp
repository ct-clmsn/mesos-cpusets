#include <set>
#include <algorithm>
#include <iostream>

#include "SubmodularScheduler.hpp"
#include "submodularscheduler-test.hpp"

int main(int argc, char** argv) {
  std::set<int> cpusets;

  SubmodularScheduler<TestPolicy> scheduler;
  scheduler(cpusets, 2.0);

  std::for_each(std::begin(cpusets), std::end(cpusets),
    [](int cpu) {
    std::cout << "cpu\t" << cpu << std::endl;
  });

  return 1;
}

