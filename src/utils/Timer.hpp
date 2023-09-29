#include <chrono>

/**
 * @brief Can be used to measure time between two points. Start with start() and get the end time when using stopMs (or stopNs, stopS, ...) 
 * 
 */

// TODO(bwintermann): Make templated
class Timer { 
    std::chrono::time_point<std::chrono::high_resolution_clock> startTime;


     public:
    Timer() = default;
    Timer(Timer&&) = default;
    Timer(const Timer&) = default;
    Timer(Timer&) = default;

    void start() {
        startTime = std::chrono::high_resolution_clock::now();
    }

    long int stopMs() {
        auto res = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - startTime).count();
        return res;
    }
};