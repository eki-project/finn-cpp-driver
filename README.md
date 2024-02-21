# A C/C++ Driver for FINN generated accelerators

[![C++ Standard](https://img.shields.io/badge/C++_Standard-C%2B%2B20-blue.svg?style=flat&logo=c%2B%2B)](https://isocpp.org/)
[![GitHub license](https://img.shields.io/badge/license-MIT-blueviolet.svg)](LICENSE)
[![GitHub Releases](https://img.shields.io/github/v/release/eki-project/finn-cpp-driver.svg)](https://github.com/eki-project/finn-cpp-driver/releases)

## Getting Started

### User documentation

If you ever need help on which arguments the driver requires, simply use the ```--help``` flag on the driver.

```console
$ ./finn --help
  -h [ --help ]                  Display help
  -m [ --mode ] arg (=test)      Mode of execution (file or test)
  -c [ --configpath ] arg        Path to the config.json file emitted by the 
                                 FINN compiler
  -i [ --input ] arg             Path to the input file. Only required if mode 
                                 is set to "file"
  -b [ --buffersize ] arg (=100) How large (in samples) the host buffer is 
                                 supposed to be
```

If the execution of the C++ driver fails due to undefined references to boost, it might be necessary to set the `LD_LIBRARY_PATH` environment variable:
```bash
export LD_LIBRARY_PATH="$(pwd)/deps/finn_boost/stage/lib/boost/:$LD_LIBRARY_PATH"
```

### Developer documentation

If you want to contribute some changes to the C++ driver for FINN, please make sure you have the pre-commit git hook installed before you commit a pull-request.

It can be installed using the following command:

```shell
./scripts/install_precommit.sh
```

Without this precommit hook it is very likely that your code can not be merged, because it is blocked by our linter.

**Prerequisites:**

* Install XRT Runtime and development packages. You can find them here: [Download](https://www.xilinx.com/support/download/index.html/content/xilinx/en/downloadNav/alveo/u280.html)
* If you use a non default install directory for XRT (default: ```/opt/xilinx/xrt```) set the XRT env var to your XRT development package installation ```export XILINX_XRT=<path>```.

**General information for building and using the FINN C++ driver**:

The driver requires two types of configurations for operation:

* A header file which defines the type of the driver. The driver depends on which FINN-datatypes are used in the network for input and output, and can run a little faster by optimizing this at compile time. This config is either defined automatically by the FINN compiler or has to be passed to the driver by hand when compiling. The header file location can be set by using the CMAKE macro `FINN_HEADER_LOCATION`. If left undefined, the header path will be automatically defined as ```FINNCppDriver/config/FinnDriverUsedDatatypes.h``` (as included from ```./src/FINNCppDriver/FINNDriver.cpp```).

* A configuration file in the JSON format. This specifies the path to the .xclbin produced by FINN, the devices by their XRT device indices, their inputs and outputs and how they are shaped, which is needed for the folding and packing operations. Note that this is passed during the _runtime_! This means that the location of the xclbin for example does not need to be fixed.

**Using the FINN C++ driver manually:**

The FINN C++ driver can be used without FINN installing itself. As a result of that you have to do some work that is normally done by the FINN compiler.

If you want to build the driver manually on the Noctua 2 cluster, please have a look at the section: [Getting started on Noctua 2](#getting-started-on-the-n2-cluster)

```bash
git clone git@github.com:eki-project/finn-cpp-driver.git
git submodule update --init --recursive
(git checkout <branch>)
./buildDependencies.sh
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release -DFINNC_ENABLE_SANITIZERS=Off -DFINN_HEADER_LOCATION=%YOUR_CONFIG_HEADER_LOCATION% ..
make -j $(nprocs)
```

**Using the FINN pipeline:**

The FINN pipeline will do most of the tasks shown above for you.
To use the FINN C++ driver with the FINN compiler specify `step_make_cpp_drive` as a build step and `build_cfg.DataflowOutputType.CPP_DRIVER` as an output.

By default, the FINN pipeline will not compile the FINN driver for you. After a successful run of the FINN pipeline navigate to the finn-cpp-driver output folder and execute the following commands to build the C++ driver:

```bash
./buildDependencies.sh
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release -DFINNC_ENABLE_SANITIZERS=Off ..
make -j $(nprocs)
```

If you do not want to use the C++ driver frontend, but use it as a library in your own project, please have a look at the section [external use](#external-use).

**Unittests:**

For unittest, the used configuration (meaning the runtime-JSON-config) can also be changed (because when running unittests, the JSON is actually needed at compile time). This can be done by setting

```bash
FINN_CUSTOM_UNITTEST_CONFIG
```

If left undefined, the path will be ```../../src/config/exampleConfig.json``` (as included from ```./unittests/core/UnittestConfig.h```).

### Getting Started on the N2 Cluster

You will first have to load a few dependencies before being able to build the project:

```bash
ml vis/Graphviz/2.50.0-GCCcore-11.2.0
ml fpga
ml xilinx/xrt/2.14
ml devel/Doxygen/1.9.5-GCCcore-12.2.0
ml compiler/GCC/12.2.0
ml devel/CMake/3.24.3-GCCcore-12.2.0
```

To execute the driver on the boards, write a job script. The job script should look something like this:

```bash
#!/bin/bash
#SBATCH -t 0:07:00
#SBATCH -A YOUR_PROJECT
#SBATCH -p fpga
#SBATCH -o cpp-finn_out_%j.out
#SBATCH --constraint=xilinx_u280_xrt2.14

ml vis/Graphviz/2.50.0-GCCcore-11.2.0 &> /dev/null
ml fpga &> /dev/null
ml xilinx/xrt/2.14 &> /dev/null
ml lang/Python/3.10.4-GCCcore-11.3.0-bare &> /dev/null
ml compiler/GCC/12.2.0

#DO YOUR WORK WITH FINN HERE. FOR EXAMPLE CALL ./finn --help
```

Execute it with ```sbatch write.sh```.
Use ```xbutil``` to get information about the cards and configure them manually if necessary.

(Project name, resource usage, output filename, xrt version etc. are all examples and have to be set by the user themselves).

### External Use

**TLDR:**
```bash
git submodule add https://github.com/eki-project/finn-cpp-driver.git
cd finn-cpp-driver && git checkout dev && cd ..
git submodule update --init --recursive
cd finn-cpp-driver
./buildDependencies.sh
cd ..
```

**Long version:**

The Finn C++ Driver can be used as a submodule in your own projects. To get started please first go to your folder where the finn-cpp-driver should be located and add the submodule using `git submodule add https://github.com/eki-project/finn-cpp-driver.git`. Next it is important that you choose the correct branch. Most of the time you want to use the main branch, but if you want to a specific branch, go to the finn-cpp-driver submodule folder and check out the correct branch. After that init the submodule using `git submodule update --init --recursive`. Next you will need to build the dependencies used by the Finn C++ Driver. Go to the finn-cpp-driver folder and execute `./buildDependencies.sh`. This will take a while. After the dependencies finished building, it is possible to use the FINN C++ Driver as a CMake submodule:

```CMake
#Add the C++ driver as a submodule
add_subdirectory(external/finn-cpp-driver)

#Link an example application against the finn driver
#TODO: Maybe rework this so this is nicer with an exported targets?
add_executable(main main.cpp)
target_include_directories(main SYSTEM PRIVATE ${XRT_INCLUDE_DIRS} ${FINNC_SRC_DIR})
target_link_directories(main PRIVATE ${XRT_LIB_CORE_LOCATION} ${XRT_LIB_OCL_LOCATION} ${BOOST_LIBRARYDIR})
target_link_libraries(main PRIVATE finnc_core OpenCL xrt_coreutil uuid finnc_utils ${Boost_LIBRARIES})
```

### Known Issues

* Building against Boost results in undefined references:
  * This issue is caused by namespaces leaking from inside XRT. The C++ has its own renamed (partial) boost version called finnBoost that can be used as a replacement.

Please refer to the git issues for currently known issues and possible ways to mitigate them.
