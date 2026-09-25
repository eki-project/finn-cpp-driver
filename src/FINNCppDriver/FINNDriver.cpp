/**
 * @file FINNDriver.cpp
 * @author Linus Jungemann (linus.jungemann@uni-paderborn.de), Bjarne Wintermann (bjarne.wintermann@uni-paderborn.de) and others
 * @brief Main file for the pre packaged C++ FINN driver
 * @version 0.1
 * @date 2023-10-31
 *
 * @copyright Copyright (c) 2023
 * @license All rights reserved. This program and the accompanying materials are made available under the terms of the MIT license.
 *
 */

#include <algorithm>    // for generate
#include <chrono>       // for nanoseconds, ...
#include <cstddef>      // for size_t
#include <cstdint>      // for uint64_t, uint8_t, ...
#include <exception>    // for exception
#include <filesystem>   // for path, exists
#include <iostream>     // for streamsize
#include <memory>       // for allocator_trai...
#include <random>       // for random_device, ...
#include <stdexcept>    // for invalid_argument
#include <string>       // for string
#include <tuple>        // for tuple
#include <type_traits>  // for remove_ref...
#include <utility>      // for move
#include <vector>       // for vector

// Helper
#include <FINNCppDriver/StandaloneExecutableDriver.hpp>


// XRT
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"

/**
 * @brief Configure ASanitizer to circumvent some buggy behavior with std::to_string
 *
 * @return const char*
 */
// NOLINTBEGIN
//  cppcheck-suppress unusedFunction
extern "C" const char* __asan_default_options() { return "detect_odr_violation=1"; }
// NOLINTEND

std::string mainLogPrefix() { return "[MAIN] "; }

/**
 * @brief Main entrypoint for the frontend of the C++ Finn driver
 *
 * @param argc Number of command line parameters
 * @param argv Array of command line parameters
 * @return int Exit status code
 */
int main(int argc, char* argv[]) {
    Logger::initLogger();
    FINN_LOG(loglevel::info) << "C++ Driver started";

    try {
        // Command Line Argument Parser
        popl::OptionParser options("Options");

        auto help_option = options.add<popl::Switch>("h", "help", "Display help");
        auto mode_option = options.add<popl::Value<std::string>>("e", "exec_mode", R"(Please select functional verification ("execute") or throughput test ("throughput")", "throughput");
        auto config_option = options.add<popl::Value<std::string>>("c", "configpath", "Required: Path to the config.json file emitted by the FINN compiler");
        auto input_option = options.add<popl::Value<std::string>>("i", "input", "Path to one or more input files (npy format). Only required if mode is set to \"file\"");
        auto output_option = options.add<popl::Value<std::string>>("o", "output", "Path to one or more output files (npy format). Only required if mode is set to \"file\"");
        auto batch_option = options.add<popl::Value<unsigned>>("b", "batchsize", "Number of samples for inference", 1);
        auto check_option = options.add<popl::Switch>("", "check", "Outputs the compile time configuration");

        options.parse(argc, argv);

        // Display help screen
        if (help_option->is_set()) {
            std::cout << options << "\n";
            return 0;
        }

        if (check_option->is_set()) {
            std::cout << "input_t: " << Finn::type_name<InputFinnType>() << "\n";
            std::cout << "output_t: " << Finn::type_name<OutputFinnType>() << "\n";
            return 0;
        }


        if (mode_option->count() > 1) {
            throw std::runtime_error("Command Line Argument Error: exec_mode can only be set once!");
        }

        std::string mode = mode_option->value();

        if (mode != "execute" && mode != "throughput") {
            throw std::runtime_error("Command Line Argument Error:'" + mode + "' is not a valid driver mode!");
        }

        FINN_LOG(loglevel::info) << mainLogPrefix() << "Driver Mode: " << mode;


        if (config_option->is_set()) {
            if (config_option->count() != 1) {
                throw std::runtime_error("Command Line Argument Error: configpath can only be set once!");
            }

            auto configFilePath = std::filesystem::path(config_option->value());
            if (!std::filesystem::exists(configFilePath)) {
                throw std::runtime_error("Command Line Argument Error: Cannot find config file at " + configFilePath.string());
            }

            FINN_LOG(loglevel::info) << mainLogPrefix() << "Config file found at " << configFilePath.string();
        } else {
            throw std::runtime_error("Command Line Argument Error: configpath is required to be set!");
        }

        if (input_option->is_set()) {
            for (size_t i = 0; i < input_option->count(); ++i) {
                std::string elem = input_option->value(i);
                auto inputFilePath = std::filesystem::path(elem);
                if (!std::filesystem::exists(inputFilePath)) {
                    throw std::runtime_error("Command Line Argument Error: Cannot find input file at " + inputFilePath.string());
                }
                FINN_LOG_DEBUG(loglevel::info) << mainLogPrefix() << "Input file found at " << inputFilePath.string();
            }
        }

        FINN_LOG(loglevel::info) << mainLogPrefix() << "Parsed command line params";

        // Switch on modes
        auto driver = Finn::StandaloneExecutableDriver<true, InputFinnType, OutputFinnType>(config_option->value(), batch_option->value());
        if (mode_option->value() == "execute") {
            if (!input_option->is_set()) {
                Finn::logAndError<std::invalid_argument>("No input file(s) specified for file execution mode!");
            }
            if (!output_option->is_set()) {
                Finn::logAndError<std::invalid_argument>("No output file(s) specified for file execution mode!");
            }
            if (input_option->count() != output_option->count()) {
                Finn::logAndError<std::invalid_argument>("Same amount of input and output files required!");
            }

            std::vector<std::string> inputVec;
            for (size_t i = 0; i < input_option->count(); ++i) {
                inputVec.emplace_back(input_option->value(i));
            }
            std::vector<std::string> outputVec;
            for (size_t i = 0; i < output_option->count(); ++i) {
                outputVec.emplace_back(output_option->value(i));
            }

            driver.runWithInputFile(inputVec, outputVec);
        } else if (mode_option->value() == "throughput") {
            driver.runThroughputTest();
        } else {
            Finn::logAndError<std::invalid_argument>("Unknown driver mode: " + mode_option->value());
        }

        return 1;
    } catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 0;
    } catch (...)  // Catch everything that is not an exception class
    {
        std::cerr << "Unknown error!"
                  << "\n";
        return 0;
    }
}