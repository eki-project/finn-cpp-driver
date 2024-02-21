#!/bin/bash

# TODO: SLURM ARGS
# SLURM arguments


# Check for existence of files
if [[ "$1" == "" ]]; then
    echo "Please provide the path to a directory containing config.json, finn-accel.xclbin and where the compiled driver will be put!"
    exit 1
fi

if [[ "$2" == "" ]]; then
    echo "Please provide the path to a header file to compile the driver with as second argument!";
    exit 1
fi


# $1: Path with config.json, finn-accel.xclbin and 
# $2: Header file

if [ ! -d "$1" ]; then
    echo "The specified config, xclbin and driver directory does not exist!"
    exit 1
fi

if [["${1::-1}" == "/"]]; then
    target_dir="${1}"
else
    target_dir="${1}/"
fi

# Check for config.json, finn-accel.xclbin
if [ ! -e "${target_dir}config.json" ] || [ ! -e "${target_dir}finn-accel.xclbin" ]; then
    echo "config.json or finn-accel.xclbin not found in ${target_dir}. Required for the FINN driver!"
    exit 1
fi

if [ ! -e "$2" ]; then
    echo "The specified header file for compilation does not exist!"
    exit 1
fi


# Load required modules for the compiler
ml fpga
ml xilinx/xrt/2.14
ml devel/Doxygen/1.9.5-GCCcore-12.2.0
ml compiler/GCC/12.2.0
ml devel/CMake/3.24.3-GCCcore-12.2.0

# Compilation
if [ ! -d "../build/" ]; then
    mkdir ../build
fi
cd ../build/
cmake -DCMAKE_BUILD_TYPE=Release -DFINN_HEADER_LOCATION=${1} .. && cmake --build . --target finn

if [ ! $? -eq 0 ]; then
    echo "Compilation failed. Exiting integration test!"
    exit 1
fi

# Copy driver to target location
cp src/finn "$target_dir"
cd $target_dir

# Loading requirements for running the driver
module reset
ml fpga &> /dev/null
ml xilinx/xrt/2.14 &> /dev/null
ml lang/Python/3.10.4-GCCcore-11.3.0-bare &> /dev/null
ml devel/Boost/1.81.0-GCC-12.2.0
ml compiler/GCC/12.2.0

# ? Remove since this is custom required code
# ? [Restart board for it to work properly]
xbutil examine -d 0
xbutil reset -d 0000:a1:00.1
sleep 2s

echo "Running driver with test inputs"

# TODO: Implement mode "integrationtest". Should write custom/random data and write results to file "integration_test_outputs.txt"
# ! For this to work, the two results MUST have the same formatting and be on one line exactly each at the start of the file!!
./finn --mode integrationtest --input config.json --configpath config.json

if [ ! -e "integration_test_outputs.txt" ]; then
    echo "Driver did NOT produce integration_test_outputs.txt while running. Unable to continue"
    exit 1
fi

# Check output file for correctness
# TODO: Allow for custom requested results instead of always identity
if [[ $(head -n 2 integration_test_outputs.txt | uniq | wc -l) -eq 1 ]]; then
    echo "Test successfull. Outputs equivalent. Exiting with 0"
    exit 0
else
    echo "Test failed, non equal outputs in integration_test_outputs.txt!"
    exit 1
fi