A C/C++ Driver for FINN generated accelerators
==============================================

[![C++ Standard](https://img.shields.io/badge/C++_Standard-C%2B%2B20-blue.svg?style=flat&logo=c%2B%2B)](https://isocpp.org/)
[![GitHub license](https://img.shields.io/badge/license-MIT-blueviolet.svg)](LICENSE)

Getting Started
---------------

* Install XRT Runtime and development packages [Download](https://www.xilinx.com/support/download/index.html/content/xilinx/en/downloadNav/alveo/u280.html)
* If you use a non default install directory for XRT (default: ```/opt/xilinx/xrt```) set the XRT env var to your XRT development package installation ```export XILINX_XRT=<path>```.

```bash
git clone git@github.com:eki-project/finn-cpp-driver.git
./buildDependencies.sh
mkdir build && cd build
cmake ..
make -j $(nprocs)
```

### Getting Started on the N2 Cluster
You will first have to load a few dependencies before being able to build the project:

```bash
ml fpga
ml xilinx/xrt/2.15
ml devel/Doxygen/1.9.5-GCCcore-12.2.0
ml compiler/GCC/12.2.0
ml devel/CMake/3.24.3-GCCcore-12.2.0
```


## TODO
* Check if XRT frees the memory map itself
* Does XRT ALWAYS take uint8? Even if not should we do it all the same?

## Structure
```
Driver (bjarne)
    Accelerator (linus)
        DeviceHandler<uint8>[] OR DeviceHandler<InputType, OutputType>[] (linus)
            XCLBIN-File
            Name
            DeviceIndex
            xrt::kernel[]
            DeviceBuffer[]<Type> (bjarne)
                xrt::bo[]
                bo-map*
                sizes
                Boost CircularBuffer
                Boost CircularBuffer Methoden
```

### Driver
```
entrypoint?()
```

### DeviceHandler
```
DeviceHandler()
initializeDevice()
initializerDeviceBuffers()
getDeviceBuffer()
(getDeviceBufferInputOutputPair())
syncBuffers() (vlt. mehrere)
```


### DeviceBuffer
(only ever write from ringBuffer, never manually!)
```
Flag: IS_INPUT_OR_OUTPUT
(Iterator für BOMap)
fillBOMapRandom()
sync()
loadFromRingBuffer() (operator overloading?)
loadToRingBuffer() (operator overloading?)
[all RingBuffer convenience functions]
getSizes()
isEmpty()
isFull()
clear()
```

## Filetree
* Filenames after class names, with UpperCamelCase
* .h no template, .hpp templated
* Split every non-template files into header and cpp file
* utils.hpp only contains items that usable everywhere


* utils: Utility functions and objects
* utils - types.h: Enums and usings

```
src
    FinnDriverUsercode.cpp
    config
        Config.h
        protobuf_generated_code...
    core
        DeviceHandler.hpp
        DeviceBuffer.hpp
        Accelerator.hpp
        Driver.hpp
    utils
        FinnDatatypes.hpp
        Types.h
        Mdspan.h
```
