#include "xrt_device.h"

#include <iostream>

namespace xrt {

    device::device(unsigned int didx) {
        // Should do something with didx and throw for certain ids
        std::cout << "something\n";
    }

    uuid device::load_xclbin(const std::string& xclbin_fnm) {
        loadedUUID = uuid();
        return loadedUUID;
    }

    uuid device::get_xclbin_uuid() const { return loadedUUID; }

    void device::reset() {}
}  // namespace xrt