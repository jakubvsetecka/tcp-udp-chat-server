#ifndef STOPWATCH_H
#define STOPWATCH_H

#include <chrono>
#include <iostream>

class StopWatch {
  private:
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point end_time;
    bool isStopped;

  public:
    StopWatch()
        : isStopped(false) {
        start_time = end_time = std::chrono::steady_clock::now();
    }

    void start() {
        start_time = std::chrono::steady_clock::now();
        isStopped = false;
    }

    void stop() {
        end_time = std::chrono::steady_clock::now();
        isStopped = true;
    }

    double duration() {
        if (!isStopped) {
            end_time = std::chrono::steady_clock::now(); // Update end if still running
        }
        return std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    }

    void reset() {
        start_time = end_time = std::chrono::steady_clock::now();
    }
};

#endif // STOPWATCH_H