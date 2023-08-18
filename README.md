A C/C++ Driver for FINN generated accelerators
==============================================

[![C++ Standard](https://img.shields.io/badge/C++_Standard-C%2B%2B20-blue.svg?style=flat&logo=c%2B%2B)](https://isocpp.org/)
[![GitHub license](https://img.shields.io/badge/license-MIT-blueviolet.svg)](LICENSE)

Getting Started
---------------
* Install XRT Runtime and development packages [Download](https://www.xilinx.com/support/download/index.html/content/xilinx/en/downloadNav/alveo/u280.html)
* Set the XRT env var to your XRT development package installation ```export XILINX_XRT=<path>``` (commonly ```/opt/xilinx/xrt```)



```bash
git clone git@github.com:eki-project/finn-cpp-driver.git
git submodule init
git submodule update
mkdir build && cd build
cmake ..
make -j $(nprocs)
```
