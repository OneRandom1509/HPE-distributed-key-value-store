#ifndef TIMER_HPP
#define TIMER_HPP

#include <chrono>
#include <string>

class Timer
{
public:
  void start();
  void stop();
  double elapsed_ms() const;
  void print(const std::string &label) const;

private:
  std::chrono::time_point<std::chrono::high_resolution_clock> start_time;
  std::chrono::time_point<std::chrono::high_resolution_clock> end_time;
};

#endif
