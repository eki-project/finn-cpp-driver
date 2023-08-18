#include <iostream>
#include <numeric>

// Created by FINN during compilation
#include "template_driver.hpp"

// XRT
#include "experimental/xrt_ip.h"
#include "xrt.h"
#include "xrt/xrt_bo.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"

using std::string;


enum DRIVER_MODE { EXECUTE, THROUGHPUT_TEST };


// Create buffers, one buffer per given shape, and bytewidth
std::vector<xrt::bo> createIOBuffers(const xrt::device& device, std::vector<int> widths,
                                     std::vector<std::vector<int>> shape) {
  std::vector<xrt::bo> buffers = {};
#pragma unroll
  for (unsigned int i = 0; i < widths.size(); i++) {
    int elements = std::accumulate(std::begin(shape[i]), std::end(shape[i]), 1, std::multiplies<int>());
    buffers.push_back(xrt::bo(device, widths[i] * elements, xrt::bo::flags::cacheable,
                              1));  // TODO(bwintermann): Correct memory group setting missing, assuming 1 here
  }
  return buffers;
}


// Create mappings of the given datatype for the given buffers
template<typename T>
std::vector<T> createMemoryMaps(std::vector<xrt::bo> buffers) {
  std::vector<T> maps = {};
  for (xrt::bo buffer : buffers) {
    maps.push_back(buffer.map<T>());
  }
  return maps;
}


int main() {
  // TODO(bwintermann): Make into command line arguments
  int deviceIndex = 0;
  string binaryFile = "finn_accel.xclbin";
  DRIVER_MODE driverExecutionMode = DRIVER_MODE::THROUGHPUT_TEST;


  xrt::device device = xrt::device(deviceIndex);
  xrt::uuid uuid = device.load_xclbin(binaryFile);

  // TODO(bwintermann): Replace palceholder kernel name
  xrt::ip kernel_ip = xrt::ip(device, uuid, "PLACEHOLDER_KERNEL_NAME");

  if (transferMode == "memory_buffered") {
    std::vector<xrt::bo> inputBuffers = createIOBuffers(device, INPUT_BYTEWIDTH, ISHAPE_PACKED);
    std::vector<xrt::bo> outputBuffers = createIOBuffers(device, OUTPUT_BYTEWIDTH, OSHAPE_PACKED);
    std::vector<int*> inputBufferMaps = createMemoryMaps<int*>(inputBuffers);
    std::vector<int*> outputBufferMaps = createMemoryMaps<int*>(outputBuffers);
  } else if (transferMode == "stream") {
    // TODO(bwintermann): Add stream implementation
  } else {
    std::cout << "Unknown transfer mode (" << transferMode
              << "). Please specify a known one in the DataflowBuildConfig!" << std::endl;
    return 1;
  }
  return 0;
}
