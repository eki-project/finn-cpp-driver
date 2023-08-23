#include <concepts>
#include <experimental/random>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <string>

// Helper
#include "utils/deviceHandler.hpp"
#include "utils/driver.h"
#include "utils/finn_types/datatype.hpp"
#include "utils/mdspan.h"

// Created by FINN during compilation
#include "template_driver.hpp"



#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>

using std::string;


namespace logging = boost::log;
namespace src = boost::log::sources;
namespace sinks = boost::log::sinks;
namespace keywords = boost::log::keywords;

void init_logging() {
    logging::add_file_log("cpp_driver.log");
    logging::core::get()->set_filter
    (
        logging::trivial::severity >= logging::trivial::info
    );
}


int main() {
    // Initialize logging sink
    init_logging();

    // Add attributes to logging config
    logging::add_common_attributes();
    
    // Creating a logger with severity suppport
    src::severity_logger<logging::trivial::severity_level> lg;
    BOOST_LOG_SEV(lg, logging::trivial::info) << "C++ Driver started";

    // TODO(bwintermann): Make into command line arguments
    int deviceIndex = 0;
    string binaryFile = "finn_accel.xclbin";
    DRIVER_MODE driverExecutionMode = DRIVER_MODE::THROUGHPUT_TEST;
    BOOST_LOG_SEV(lg, logging::trivial::info) << "Device index: " << deviceIndex << "\nBinary File Path: " << binaryFile << "\n"; //Driver Mode: " << driverExecutionMode << "\n";

    DeviceHandler<int> dha = DeviceHandler<int>(
        "DEVICE1",              // Unique name
        false,                  // Whether this is a helper node in a multi-fpga application
        deviceIndex,        
        binaryFile,
        driverExecutionMode,    // Throughput test or execution of specific data
        INPUT_BYTEWIDTH,
        OUTPUT_BYTEWIDTH,
        ISHAPE_PACKED,
        OSHAPE_PACKED,
        SHAPE_TYPE::PACKED,     // Input shape type
        SHAPE_TYPE::PACKED,     // Output shape type
        IDMA_NAMES,
        ODMA_NAMES,
        10,                     // Ring Buffer Size Factor
        lg                      // Logger instance
    );
    BOOST_LOG_SEV(lg, logging::trivial::info) << "Device handler initiated!";


    return 0;
}
