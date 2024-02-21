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

/**
 * @brief Define boost logging to be linked dynamically
 *
 */
// NOLINTNEXTLINE
#define BOOST_LOG_DYN_LINK 1

// IWYU pragma: no_include <FINNCppDriver/utils/Logger.h>
#include <boost/log/detail/config.hpp>                // IWYU pragma: keep
#include <boost/log/sources/severity_feature.hpp>     // IWYU pragma: keep
#include <boost/log/sources/severity_logger.hpp>      // IWYU pragma: keep
#include <boost/log/trivial.hpp>                      // IWYU pragma: keep
#include <boost/smart_ptr/intrusive_ptr.hpp>          // IWYU pragma: keep
#include <boost/smart_ptr/intrusive_ref_counter.hpp>  // IWYU pragma: keep
#include <string>                                     // for allocator, string

namespace bl = finnBoost::log;
namespace loglevel = bl::trivial;

/**
 * @brief Abrieviation of boost logging type
 *
 */
using logger_type = bl::sources::severity_logger<bl::trivial::severity_level>;

/**
 * @brief redefine Boost Logger for FINN
 *
 */
// NOLINTBEGIN
#define FINN_LOG(LOGGER, SEV) BOOST_LOG_SEV(LOGGER, SEV)
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
    #define FINN_LOG_DEBUG(LOGGER, SEV) dev_null
#else
    /**
     * @brief Defines debug logging macro that is removed when building in Release mode
     *
     */
    #define FINN_LOG_DEBUG(LOGGER, SEV) FINN_LOG(LOGGER, SEV)
#endif  // NDEBUG
        // NOLINTEND

/**
 * @brief Singleton class that provides logger functionality for the driver. Based on the boost severity logger
 *
 */
class Logger {
     public:
    /**
     * @brief Get the Logger object
     *
     * @return logger_type&
     */
    static logger_type& getLogger();

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
    void initLogging();
    Logger();
    const std::string logFormat = "[%TimeStamp%] (%LineID%) [%Severity%]: %Message%";
};

#endif  // !LOGGING_H