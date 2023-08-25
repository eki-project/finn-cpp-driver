#include "Logger.h"

using backend_type = bl::sinks::text_file_backend;
using sink_type = bl::sinks::synchronous_sink<backend_type>;
namespace kw = bl::keywords;

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
    auto backend = boost::make_shared<backend_type>(kw::file_name = "finnLog_%N.log", kw::rotation_size = 10 * 1024 * 1024, kw::time_based_rotation = bl::sinks::file::rotation_at_time_point(0, 0, 0), kw::auto_flush = true);

    auto sink = boost::make_shared<sink_type>(backend);
    sink->set_formatter(bl::parse_formatter(logFormat));

    bl::core::get()->add_sink(sink);
    initLogging();
}

void Logger::initLogging() {
    static bool init = false;
    if (!init) {
        init = !init;
        bl::register_simple_formatter_factory<bl::trivial::severity_level, char>("Severity");
        boost::log::add_common_attributes();

        bl::add_console_log(std::clog, bl::keywords::format = logFormat);
        return;
    }
    BOOST_LOG_SEV(Details::boostLogger, bl::trivial::warning) << "Do not init the logger more than once!";
}