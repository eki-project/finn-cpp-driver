#include <concepts>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <string>

// Helper
#include "core/DeviceBuffer.hpp"
#include "utils/FinnDatatypes.hpp"
#include "utils/Logger.h"

// Created by FINN during compilation
#include "config/config.h"

// XRT
#include "xrt/xrt_device.h"

using std::string;


template<typename T>
BOMemoryDefinitionArguments<T> toVariant(const shape_list_t& inp) {
    std::vector<std::variant<shape_t, MemoryMap<T>>> ret;
    ret.reserve(inp.size());

    std::transform(inp.begin(), inp.end(), std::back_inserter(ret), [](const shape_t& shape) { return std::variant<shape_t, MemoryMap<T>>(shape); });
    return ret;
}

int main() {
    auto logger = Logger::getLogger();
    FINN_LOG(logger, loglevel::info) << "C++ Driver started";
    FINN_LOG_DEBUG(logger, loglevel::info) << "Test";

    // int deviceIndex = 0;
    // string binaryFile = "finn_accel.xclbin";
    // // DRIVER_MODE driverExecutionMode = DRIVER_MODE::THROUGHPUT_TEST;
    // BOOST_LOG_SEV(log, logging::trivial::info) << "Device index: " << deviceIndex << "\nBinary File Path: " << binaryFile << "\n";  // Driver Mode: " << driverExecutionMode << "\n";

    // DeviceHandler<int> dha = DeviceHandler<int>("DEVICE1",  // Unique name
    //                                             false,      // Whether this is a helper node in a multi-fpga application
    //                                             deviceIndex, binaryFile,
    //                                             driverExecutionMode,  // Throughput test or execution of specific data
    //                                             Config::INPUT_BYTEWIDTH, Config::OUTPUT_BYTEWIDTH, toVariant<int>(Config::ISHAPE_PACKED), toVariant<int>(Config::OSHAPE_PACKED),
    //                                             SHAPE_TYPE::PACKED,  // Input shape type
    //                                             SHAPE_TYPE::PACKED,  // Output shape type
    //                                             Config::IDMA_NAMES, Config::ODMA_NAMES,
    //                                             10,  // Ring Buffer Size Factor
    //                                             log  // Logger instance
    // );
    // BOOST_LOG_SEV(log, logging::trivial::info) << "Device handler initiated!";


    auto myDevice = xrt::device();
    shape_t myShape = std::vector<unsigned int> {1,2,3};
    DatatypeInt<2> myDatatype = DatatypeInt<2>();
    DeviceBuffer<uint8_t, DatatypeInt<2>> db = DeviceBuffer<uint8_t, DatatypeInt<2>>(myDevice, myShape, 100, IO::INPUT, myDatatype);

    return 0;
}
