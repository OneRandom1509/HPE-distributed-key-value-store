#include "Timer.hpp"
#include <iostream>

void Timer::start() { start_time = std::chrono::high_resolution_clock::now(); }

void Timer::stop() { end_time = std::chrono::high_resolution_clock::now(); }

double Timer::elapsed_ms() const
{
  std::chrono::duration<double, std::milli> elapsed = end_time - start_time;
  return elapsed.count();
}

void Timer::print(const std::string &label) const
{
  std::cout << "[" << label << "] completed in " << elapsed_ms() << " ms"
            << std::endl;
}
