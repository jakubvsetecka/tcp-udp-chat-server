#include <chrono>
#include <iostream>

class StopWatch {
  private:
    std::chrono::steady_clock::time_point start;
    std::chrono::steady_clock::time_point end;
    bool isStopped;

  public:
    StopWatch()
        : isStopped(false) {
        start = std::chrono::steady_clock::now();
    }

    void stop() {
        end = std::chrono::steady_clock::now();
        isStopped = true;
    }

    double duration() {
        if (!isStopped) {
            end = std::chrono::steady_clock::now(); // Update end if still running
        }
        return std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    }
};