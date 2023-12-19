#include "xrt_bo.h"

#include "../xrt.h"
#include "xrt_device.h"

void xrt::bo::sync(xclBOSyncDirection syncMode) {
    // FINN_LOG(logger, loglevel::debug) << "(xrtMock) xrt::bo object synced!\n";
}

void xrt::bo::sync(xclBOSyncDirection dir, size_t sz, size_t offset){
    // FINN_LOG(logger, loglevel::debug) << "(xrtMock) xrt::bo object synced!\n";
}

/**
 * @brief Destroy the xrt::bo object and free the memory map
 *
 */
xrt::bo::~bo() {
    FINN_LOG(logger, loglevel::debug) << "(xrtMock) Destroying and freeing xrt::bo object!\n";
    free(memmap);
}