#ifndef LOGGING_H
#define LOGGING_H

#include <boost/log/attributes/scoped_attribute.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks.hpp>
#include <boost/log/sources/severity_channel_logger.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/formatter_parser.hpp>
#include <fstream>
#include <string>

namespace bl = boost::log;
namespace loglevel = bl::trivial;

using logger_type = bl::sources::severity_logger<bl::trivial::severity_level>;

// redefine Boost Logger for FINN
// NOLINTBEGIN
#define FINN_LOG(LOGGER, SEV) BOOST_LOG_SEV(LOGGER, SEV)
#ifdef NDEBUG
static class DevNull {
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