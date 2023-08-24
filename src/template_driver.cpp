#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <concepts>
#include <experimental/random>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <string>

// Helper
#include "utils/MemoryMap.hpp"
#include "utils/deviceHandler.hpp"
#include "utils/finn_types/datatype.hpp"
#include "utils/mdspan.h"

// Created by FINN during compilation
#include "config.h"

using std::string;


namespace logging = boost::log;
namespace src = boost::log::sources;
namespace sinks = boost::log::sinks;
namespace keywords = boost::log::keywords;

void initLogging() {
    logging::add_file_log("cpp_driver.log");
    logging::core::get()->set_filter(logging::trivial::severity >= logging::trivial::info);
}

template<typename T>
BOMemoryDefinitionArguments<T> toVariant(const shape_list_t& inp) {
    std::vector<std::variant<shape_t, MemoryMap<T>>> ret;
    ret.reserve(inp.size());
    // TODO(linusjun) Implement
    return std::move(ret);
}


int main() {
    // Initialize logging sink
    initLogging();

    // Add attributes to logging config
    logging::add_common_attributes();

    // Creating a logger with severity suppport
    src::severity_logger<logging::trivial::severity_level> log;
    BOOST_LOG_SEV(log, logging::trivial::info) << "C++ Driver started";

    // TODO(bwintermann): Make into command line arguments
    int deviceIndex = 0;
    string binaryFile = "finn_accel.xclbin";
    DRIVER_MODE driverExecutionMode = DRIVER_MODE::THROUGHPUT_TEST;
    BOOST_LOG_SEV(log, logging::trivial::info) << "Device index: " << deviceIndex << "\nBinary File Path: " << binaryFile << "\n";  // Driver Mode: " << driverExecutionMode << "\n";

    DeviceHandler<int> dha = DeviceHandler<int>("DEVICE1",  // Unique name
                                                false,      // Whether this is a helper node in a multi-fpga application
                                                deviceIndex, binaryFile,
                                                driverExecutionMode,  // Throughput test or execution of specific data
                                                Config::INPUT_BYTEWIDTH, Config::OUTPUT_BYTEWIDTH, toVariant<int>(Config::ISHAPE_PACKED), toVariant<int>(Config::OSHAPE_PACKED),
                                                SHAPE_TYPE::PACKED,  // Input shape type
                                                SHAPE_TYPE::PACKED,  // Output shape type
                                                Config::IDMA_NAMES, Config::ODMA_NAMES,
                                                10,  // Ring Buffer Size Factor
                                                log  // Logger instance
    );
    BOOST_LOG_SEV(log, logging::trivial::info) << "Device handler initiated!";


    return 0;
}
