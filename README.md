# A C/C++ Driver for FINN generated accelerators

[![C++ Standard](https://img.shields.io/badge/C++_Standard-C%2B%2B20-blue.svg?style=flat&logo=c%2B%2B)](https://isocpp.org/)
[![GitHub license](https://img.shields.io/badge/license-MIT-blueviolet.svg)](LICENSE)
[![GitHub Releases](https://img.shields.io/github/v/release/eki-project/finn-cpp-driver.svg)](https://github.com/eki-project/finn-cpp-driver/releases)
![GitHub Release Date](https://img.shields.io/github/release-date/eki-project/finn-cpp-driver)
![GitHub branch status](https://img.shields.io/github/checks-status/eki-project/finn-cpp-driver/main)


## Getting Started

If you just want to use the C++ driver for FINN as an alternative for the default PYNQ driver, it is now possible to directly generate the C++ driver and all of its configutation files from [FINN](https://github.com/Xilinx/finn) and [FINN+](https://github.com/eki-project/finn-plus)!

Just select `build_cfg.DataflowOutputType.CPP_DRIVER` instead of `build_cfg.DataflowOutputType.PYNQ_DRIVER` in your build script in `generate_outputs`.

FINN will then generate all config files for you. For FINN it is then necessary to build the driver yourself. See section [Building the Driver](#building-the-driver).

FINN+ on the other hand will configure and build the driver for you completely automatically. We would therefore recommend using FINN+ instead of standard FINN.

### Using the driver

You can either use the driver as a standalone executable or as a library. For the use of the C++ driver as a library, please have a look at the section for [library use](#using-the-driver-as-a-library).

If you ever need help on which arguments the driver requires, simply use the ```--help``` flag on the driver. The executable for the driver is, by default, located in the `build/bin` folder after compiling the driver.

The following options are supported by the C++ driver executable to match the PYNQ driver:

```console
$ ./finnhpc --help
Options:
  -h [ --help ]                        Display help
  -e [ --exec_mode ] arg (=throughput) Please select functional verification ("execute") or throughput test ("throughput")
  -c [ --configpath ] arg              Required: Path to the config.json file emitted by the FINN compiler (Usually in the base folder of the driver.)
  -i [ --input ] arg                   Path to one or more input files (npy format). Only required if mode is set to "execute"
  -o [ --output ] arg                  Path to one or more output files (npy format). Only required if mode is set to "execute"
  -b [ --batchsize ] arg (=1)          Number of samples for inference
  --check                              Outputs the compile time configuration
```

If the execution of the C++ driver fails due to missing libraries such as `libfinn_core.so`, it might be necessary to set the `LD_LIBRARY_PATH` environment variable:

```bash
export LD_LIBRARY_PATH="$(pwd)/build/libs:$LD_LIBRARY_PATH"
```

### Building the Driver

**IMPORTANT:** Building the driver manually is only required for FINN. FINN+ builds the driver for you!

**Prerequisites:**

* Install XRT Runtime and development packages. You can find them here: [Download](https://www.xilinx.com/support/download/index.html/content/xilinx/en/downloadNav/alveo/u280.html)
* If you use a non default install directory for XRT (default: ```/opt/xilinx/xrt```) set the XRT env var to your XRT development package installation ```export XILINX_XRT=<path>```.

**General information for building and using the C++ driver**:

The driver requires two types of configurations for operation:

* A header file which defines the type of the driver. The driver depends on which FINN-datatypes are used in the network for input and output, and can run significantly faster by optimizing this at compile time. This config is either defined automatically by the FINN+ compiler or has to be passed to the driver by hand when compiling. The header file location can be set by using the CMAKE macro `FINN_HEADER_LOCATION`. If left undefined, the header path will be automatically defined as ```FINNCppDriver/config/FinnDriverUsedDatatypes.h``` (as included from ```./src/FINNCppDriver/FINNDriver.cpp```). **Not using a correct header file for your network will lead to problems! The input and output types defined in the header file have to match your network! The default header file included in ```FINNCppDriver/config/FinnDriverUsedDatatypes.h``` is only useful for testing!**

* A configuration file in the JSON format. This specifies the path to the .xclbin produced by FINN, the devices by their XRT device indices, their inputs and outputs and how they are shaped, which is needed for the folding and packing operations. Note that this is passed during the _runtime_! It is passed to the driver using the `-c/--configpath` option. This means that the location of the xclbin for example does not need to be fixed.

**Building the C++ driver on your own:**

This section is mostly needed if you are using FINN and not FINN+. FINN+ will built the driver executable automatically for you.

It is assumed, that you used FINN and now want to build the generated driver. Completely setting up the C++ driver by hand is not recommended anymore!

(If you want to build the driver on the Noctua 2 cluster, please have a look at the section: [Getting started on Noctua 2](#getting-started-on-the-n2-cluster))

Building the driver is as easy as running:

```bash
git submodule update --init --recursive
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release -DFINN_ENABLE_SANITIZERS=OFF -DFINN_HEADER_LOCATION=../AcceleratorDatatypes.h -DFINN_USE_HOST_MEM=OFF ..
make -j $(nprocs)
```

If you are using FINN+, you built your accelerator using the host memory option and your system is configured for Host Memory Access you can enable the option `DFINN_USE_HOST_MEM` option to get an additional performance boost. For FINN Host Memory Access is not supported and therefore host memory has to be disabled!

### Developer documentation

If you want to contribute some changes to the C++ driver, please make sure you have the pre-commit git hook installed before you commit a pull-request.

It can be installed using the following command:

```shell
./scripts/install_precommit.sh
```

Without this precommit hook it is very likely that your code can not be merged, because it is blocked by our linter.

**Unittests:**

For unittest, the used configuration (meaning the runtime-JSON-config) can also be changed (because when running unittests, the JSON is actually needed at compile time). This can be done by setting

```bash
FINN_CUSTOM_UNITTEST_CONFIG
```

If left undefined, the path will be ```../../src/config/exampleConfig.json``` (as included from ```./unittests/core/UnittestConfig.h```).

### Getting Started on the N2 Cluster

You will first have to load a few dependencies before being able to build the project:

```bash
ml compiler/GCCcore/11.3.0 compiler/GCC/11.3.0 lib/pybind11/2.9.2-GCCcore-11.3.0 lib/fmt/9.1.0-GCCcore-11.3.0
ml devel Autoconf/2.71-GCCcore-11.3.0
ml lang Bison/3.8.2-GCCcore-11.3.0 flex/2.6.4-GCCcore-11.3.0
ml fpga xilinx/xrt/2.14
```

To execute the driver on the boards, write a job script. The job script should look something like this:

```bash
#!/bin/bash
#SBATCH -t 0:07:00
#SBATCH -A YOUR_PROJECT
#SBATCH -p fpga
#SBATCH -o cpp-finn_out_%j.out
#SBATCH --constraint=xilinx_u280_xrt2.14

ml compiler/GCCcore/11.3.0 compiler/GCC/11.3.0 lib/pybind11/2.9.2-GCCcore-11.3.0 lib/fmt/9.1.0-GCCcore-11.3.0
ml devel Autoconf/2.71-GCCcore-11.3.0
ml lang Bison/3.8.2-GCCcore-11.3.0 flex/2.6.4-GCCcore-11.3.0
ml fpga xilinx/xrt/2.14

#DO YOUR WORK WITH FINN HERE. FOR EXAMPLE CALL ./finnhpc --help
```

Execute it with ```sbatch write.sh```.
Use ```xbutil``` to get information about the cards and configure them manually if necessary.

(Project name, resource usage, output filename, xrt version etc. are all examples and have to be set by the user themselves).

### Using the driver as a library

The C++ Driver can be used as a submodule in your own projects. **Please make sure to initialize all submodules recursively!** It is then possible to use the C++ Driver as a CMake submodule:

```CMake
#Add the C++ driver as a submodule
#Change the path to match your submodule location
add_subdirectory(external/finn-cpp-driver)

#Link an example application against the finn driver
add_executable(example example.cpp)
target_include_directories(example SYSTEM PRIVATE ${XRT_INCLUDE_DIRS} ${FINN_SRC_DIR})
target_link_directories(example PRIVATE ${XRT_LIB_CORE_LOCATION} ${XRT_LIB_OCL_LOCATION})
target_link_libraries(example PRIVATE finnc_core finnc_options Threads::Threads OpenCL xrt_coreutil uuid finnc_utils finn_config nlohmann_json::nlohmann_json OpenMP::OpenMP_CXX)
```

### Known Issues

Please refer to the git issues for currently known issues and possible ways to mitigate them.
