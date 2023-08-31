#include "../xrt.h"
#include "xrt_device.h"
#include "xrt_bo.h"
#include "../../../src/utils/Logger.h"

void xrt::bo::sync(XCL_SYNC_MODES syncMode) {
    //FINN_LOG(logger, loglevel::debug) << "(xrtMock) xrt::bo object synced!\n";
}

/**
* @brief Destroy the xrt::bo object and free the memory map 
* 
*/
xrt::bo::~bo() {
    //FINN_LOG(logger, loglevel::debug) << "(xrtMock) Destroying and freeing xrt::bo object!\n";
    free(memmap);
}