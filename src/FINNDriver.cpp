#include <concepts>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <string>

// Helper
#include "core/Accelerator.h"
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

    Finn::DeviceWrapper devWrap;
    devWrap.xclbin = "design.xclbin";
    devWrap.name = "SomeName";
    devWrap.idmas = Config::idmaNames;
    devWrap.odmas = Config::odmaNames;

    Finn::Accelerator acc(devWrap);


    auto myDevice = xrt::device();
    shape_t myShape = std::vector<unsigned int>{1, 2, 3};
    DatatypeInt<2> myDatatype = DatatypeInt<2>();
    DeviceBuffer<uint8_t, DatatypeInt<2>> dbuffer = DeviceBuffer<uint8_t, DatatypeInt<2>>("MyDeviceBuffer", myDevice, myShape, 100, IO::INPUT);

    return 0;
}
