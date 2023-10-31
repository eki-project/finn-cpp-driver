/**
 * @file Logger.cpp
 * @author Linus Jungemann (linus.jungemann@uni-paderborn.de) and others
 * @brief Provides a easy to use logger for the FINN driver
 * @version 0.1
 * @date 2023-10-31
 *
 * @copyright Copyright (c) 2023
 * @license All rights reserved. This program and the accompanying materials are made available under the terms of the MIT license.
 *
 */

#include "Logger.h"

#include <boost/core/enable_if.hpp>                       // for lazy_enable...
#include <boost/exception/exception.hpp>                  // for exception
#include <boost/log/core/core.hpp>                        // for core
#include <boost/log/core/record.hpp>                      // for record
#include <boost/log/detail/attachable_sstream_buf.hpp>    // for basic_ostri...
#include <boost/log/detail/attr_output_impl.hpp>          // for operator<<
#include <boost/log/expressions/formatter.hpp>            // for basic_forma...
#include <boost/log/keywords/auto_flush.hpp>              // for auto_flush
#include <boost/log/keywords/file_name.hpp>               // for file_name
#include <boost/log/keywords/format.hpp>                  // for format, key...
#include <boost/log/keywords/rotation_size.hpp>           // for rotation_size
#include <boost/log/keywords/time_based_rotation.hpp>     // for time_based_...
#include <boost/log/sinks/sync_frontend.hpp>              // for synchronous...
#include <boost/log/sinks/text_file_backend.hpp>          // for rotation_at...
#include <boost/log/sources/record_ostream.hpp>           // for record_pump...
#include <boost/log/utility/setup/common_attributes.hpp>  // for add_common_...
#include <boost/log/utility/setup/console.hpp>            // for add_console...
#include <boost/log/utility/setup/formatter_parser.hpp>   // for parse_forma...
#include <boost/parameter/keyword.hpp>                    // for keyword
#include <boost/preprocessor/seq/limits/enum_256.hpp>     // for BOOST_PP_SE...
#include <boost/preprocessor/seq/limits/size_256.hpp>     // for BOOST_PP_SE...
#include <boost/smart_ptr/make_shared_object.hpp>         // for make_shared
#include <boost/smart_ptr/shared_ptr.hpp>                 // for shared_ptr
#include <boost/thread/exceptions.hpp>                    // for thread_inte...
#include <fstream>                                        // for streamsize
#include <iostream>                                       // for clog

using backend_type = bl::sinks::text_file_backend;
using sink_type = bl::sinks::synchronous_sink<backend_type>;
namespace kw = bl::keywords;

// NOLINTBEGIN
#ifdef NDEBUG
DevNull dev_null;
#endif  // NDEBUG
// NOLINTEND

namespace Details {
    // NOLINTNEXTLINE
    logger_type boostLogger;
}  // namespace Details

// cppcheck-suppress unusedFunction
logger_type& Logger::getLogger() {
    static Logger log;
    return Details::boostLogger;
}

Logger::Logger() {
    auto backend = finnBoost::make_shared<backend_type>(kw::file_name = "finnLog_%N.log", kw::rotation_size = 10 * 1024 * 1024, kw::time_based_rotation = bl::sinks::file::rotation_at_time_point(0, 0, 0), kw::auto_flush = true);

    auto sink = finnBoost::make_shared<sink_type>(backend);
    sink->set_formatter(bl::parse_formatter(logFormat));

    bl::core::get()->add_sink(sink);
    initLogging();
}

void Logger::initLogging() {
    static bool init = false;
    if (!init) {
        init = !init;
        bl::register_simple_formatter_factory<bl::trivial::severity_level, char>("Severity");
        finnBoost::log::add_common_attributes();

        bl::add_console_log(std::clog, bl::keywords::format = logFormat);
        return;
    }
    BOOST_LOG_SEV(Details::boostLogger, bl::trivial::warning) << "Do not init the logger more than once!";
}