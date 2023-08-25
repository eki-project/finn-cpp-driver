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
#include "core/DeviceBuffer.hpp"
#include "utils/FinnDatatypes.hpp"
#include "utils/mdspan.h"

// Created by FINN during compilation
#include "config/config.h"

// XRT
#include "xrt/xrt_device.h"

using std::string;


namespace logging = boost::log;
namespace src = boost::log::sources;

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


    auto myDevice = xrt::device();
    shape_t myShape = std::vector<unsigned int> {1,2,3};
    DatatypeInt<2> myDatatype = DatatypeInt<2>();
    DeviceBuffer<uint8_t, DatatypeInt<2>> db = DeviceBuffer<uint8_t, DatatypeInt<2>>(myDevice, myShape, 100, IO::INPUT, myDatatype);

    return 0;
}
