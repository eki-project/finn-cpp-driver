/**
 * @file Logger.h
 * @author Linus Jungemann (linus.jungemann@uni-paderborn.de) and others
 * @brief Provides a easy to use logger for the FINN driver
 * @version 0.2
 * @date 2023-10-31
 *
 * @copyright Copyright (c) 2023-2025
 * @license All rights reserved. This program and the accompanying materials are made available under the terms of the MIT license.
 *
 */

#ifndef LOGGING_H
#define LOGGING_H

#include <plog/Appenders/ColorConsoleAppender.h>
#include <plog/Appenders/RollingFileAppender.h>
#include <plog/Formatters/TxtFormatter.h>
#include <plog/Init.h>
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
class [[maybe_unused]] DevNull {};

static DevNull dev_null;

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
    /**
     * @brief Initialize the logger with optional console output
     *
     * @param console Enable console output in addition to file logging
     */
    void static initLogger(bool console = false) { static Logger log(console); }

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
    Logger(bool console = false) {
        static plog::RollingFileAppender<plog::TxtFormatter> fileAppender("finnLog.log", 10 * 1024 * 1024, 3);
        static plog::ColorConsoleAppender<plog::TxtFormatter> consoleAppender;
        if (console) {
            plog::init(plog::debug, &fileAppender).addAppender(&consoleAppender);
        } else {
            plog::init(plog::debug, &fileAppender);
        }
    }
    const std::string logFormat = "[%TimeStamp%] (%LineID%) [%Severity%]: %Message%";
};

namespace Finn {
    /**
     * @brief First log the message as an error into the logger, then throw the passed error!
     *
     * @tparam E
     * @param msg
     */
    template<typename E>
    [[noreturn]] void logAndError(const std::string& msg) {
        FINN_LOG(loglevel::error) << msg;
        throw E(msg);
    }
}  // namespace Finn


#endif  // !LOGGING_H