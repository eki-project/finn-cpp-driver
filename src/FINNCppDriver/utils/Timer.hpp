/**
 * @file Timer.hpp
 * @author Bjarne Wintermann (bjarne.wintermann@uni-paderborn.de) and others
 * @brief Small timer helper class
 * @version 0.1
 * @date 2023-10-31
 *
 * @copyright Copyright (c) 2023
 * @license All rights reserved. This program and the accompanying materials are made available under the terms of the MIT license.
 *
 */

#ifndef TIMER_HPP
#define TIMER_HPP

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

    void start() { startTime = std::chrono::high_resolution_clock::now(); }

    long int stopMs() {
        auto res = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - startTime).count();
        return res;
    }

    long int stopNs() {
        auto res = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - startTime).count();
        return res;
    }
};

#define FINN_BENCHMARK(resultMs, code) \
    auto timer = Timer();              \
    timer.start();                     \
    code resultMs = timer.stopMs;


#endif  // TIMER_HPP
