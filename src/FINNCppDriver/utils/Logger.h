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

// NOLINTNEXTLINE
#define BOOST_LOG_DYN_LINK 1

#include <boost/log/detail/config.hpp>                // for log
#include <boost/log/sources/severity_feature.hpp>     // for BOOST_LOG_SEV
#include <boost/log/sources/severity_logger.hpp>      // for severity_logger
#include <boost/log/trivial.hpp>                      // for severity_level
#include <boost/smart_ptr/intrusive_ptr.hpp>          // for intrusive_ptr
#include <boost/smart_ptr/intrusive_ref_counter.hpp>  // for intrusive_ptr_a...
#include <string>                                     // for allocator, string

namespace bl = finnBoost::log;
namespace loglevel = bl::trivial;

using logger_type = bl::sources::severity_logger<bl::trivial::severity_level>;

// redefine Boost Logger for FINN
// NOLINTBEGIN
#define FINN_LOG(LOGGER, SEV) BOOST_LOG_SEV(LOGGER, SEV)
#ifdef NDEBUG
extern class [[maybe_unused]] DevNull {
} dev_null;

template<typename T>
DevNull& operator<<(DevNull& dest, [[maybe_unused]] T) {
    return dest;
}
    #define FINN_LOG_DEBUG(LOGGER, SEV) dev_null
#else
    #define FINN_LOG_DEBUG(LOGGER, SEV) FINN_LOG(LOGGER, SEV)
#endif  // NDEBUG
// NOLINTEND

class Logger {
     public:
    static logger_type& getLogger();

    Logger(Logger const&) = delete;
    void operator=(Logger const&) = delete;
    Logger& operator=(Logger&&) = delete;

    ~Logger() = default;
    Logger(Logger&&) = default;

     private:
    void initLogging();
    Logger();
    const std::string logFormat = "[%TimeStamp%] (%LineID%) [%Severity%]: %Message%";
};

#endif  // !LOGGING_H