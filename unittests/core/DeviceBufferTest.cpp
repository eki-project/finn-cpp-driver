//#include "../../src/core/DeviceBuffer.hpp"

#include "gtest/gtest.h"
#include "xrt/xrt_kernel.h"
#include "xrt/xrt_device.h"

//using namespace Finn;

TEST(DeviceBufferTest, DBInitTest) {
    auto device = xrt::device();
    auto kernel = xrt::kernel();
//    shape_t myShape = std::vector<unsigned int>{1,28,28,3};
    std::string myname = "abc";

    //auto inputDB = DeviceInputBuffer<uint8_t, DatatypeUInt<2>>(myname, device, kernel, myShape, 10);

}

int main() {}