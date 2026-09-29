#include <FINNCppDriver/utils/Logger.hpp>
    
void Logger::initLogger(bool console) { static Logger log(console); }

Logger::Logger(bool console) {
    static plog::RollingFileAppender<plog::TxtFormatter> fileAppender("finnLog.log", 10 * 1024 * 1024, 3);
    static plog::ColorConsoleAppender<plog::TxtFormatter> consoleAppender;
    if (console) {
        plog::init(plog::debug, &fileAppender).addAppender(&consoleAppender);
    } else {
        plog::init(plog::debug, &fileAppender);
    }
}