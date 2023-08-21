#include <concepts>
#include <experimental/random>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <string>

// Helper
#include "utils/driver.h"
#include "utils/finn_types/datatype.hpp"
#include "utils/mdspan.h"
#include "utils/device_handler.hpp"

// Created by FINN during compilation
#include "template_driver.hpp"


using std::string;




int main() {
    // TODO(bwintermann): Make into command line arguments
    int deviceIndex = 0;
    string binaryFile = "finn_accel.xclbin";
    DRIVER_MODE driverExecutionMode = DRIVER_MODE::THROUGHPUT_TEST;

    DeviceHandler<int> dh = DeviceHandler<int>(deviceIndex, binaryFile, driverExecutionMode, INPUT_BYTEWIDTH, OUTPUT_BYTEWIDTH, ISHAPE_PACKED, OSHAPE_PACKED, SHAPE_TYPE::PACKED, SHAPE_TYPE::PACKED);


    return 0;
}
