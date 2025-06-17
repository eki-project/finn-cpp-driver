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

#include <plog/Appenders/ColorConsoleAppender.h>
#include <plog/Appenders/RollingFileAppender.h>
#include <plog/Formatters/TxtFormatter.h>
#include <plog/Init.h>

#include <iostream>  // for streamsize


// NOLINTBEGIN
#ifdef NDEBUG
DevNull dev_null;
#endif  // NDEBUG
// NOLINTEND

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