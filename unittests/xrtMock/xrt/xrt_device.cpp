#include "xrt_device.h"

#include <array>
#include <numeric>

namespace xrt {

    device::device(unsigned int didx) {
        ++device_costum_constructor_called;
        device_param_didx = didx;
    }

    uuid device::load_xclbin(const std::string& xclbin_fnm) {
        loaded_xclbin = xclbin_fnm;
        std::array<unsigned char, 16> id;
        std::iota(id.begin(), id.end(), 1);
        loadedUUID = uuid(id.data());
        return loadedUUID;
    }

    uuid device::get_xclbin_uuid() const { return loadedUUID; }

    void device::reset() {}
}  // namespace xrt