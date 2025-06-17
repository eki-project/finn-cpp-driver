/**
 * @file Logger.h
 * @author Linus Jungemann (linus.jungemann@uni-paderborn.de) and others
 * @brief Provides a easy to use logger for the FINN driver
 * @version 0.1
 * @date 2023-10-31
 *
 * @copyright Copyright (c) 2023
 * @license All rights reserved. This program and the accompanying materials are made available under the terms of the MIT license.
 *
 */

#ifndef LOGGING_H
#define LOGGING_H

#include <plog/Log.h>

#include <string>  // for allocator, string

namespace loglevel = plog;

/**
 * @brief Redefine plog logging macros for FINN
 *
 */
// NOLINTBEGIN
#define FINN_LOG(SEV) PLOG(SEV)
#ifdef NDEBUG
extern class [[maybe_unused]] DevNull {
} dev_null;

template<typename T>
DevNull& operator<<(DevNull& dest, [[maybe_unused]] T) {
    return dest;
}
    /**
     * @brief Defines debug logging macro that is removed when building in Release mode
     *
     */
    #define FINN_LOG_DEBUG(SEV) dev_null
#else
    /**
     * @brief Defines debug logging macro that is removed when building in Release mode
     *
     */
    #define FINN_LOG_DEBUG(SEV) FINN_LOG(SEV)
#endif  // NDEBUG
        // NOLINTEND

/**
 * @brief Singleton class that provides logger functionality for the driver.
 *
 */
class Logger {
     public:
    void static initLogger(bool console = false);

    /**
     * @brief Construct a new Logger object (Deleted)
     *
     */
    Logger(Logger const&) = delete;
    /**
     * @brief Deleted copy assignment operator
     *
     */
    void operator=(Logger const&) = delete;
    /**
     * @brief Deleted move assignment operator
     *
     * @return Logger&
     */
    Logger& operator=(Logger&&) = delete;

    /**
     * @brief Destroy the Logger object
     *
     */
    ~Logger() = default;
    /**
     * @brief Move constructor
     *
     */
    Logger(Logger&&) = default;

     private:
    Logger(bool console = false);
    const std::string logFormat = "[%TimeStamp%] (%LineID%) [%Severity%]: %Message%";
};

#endif  // !LOGGING_H