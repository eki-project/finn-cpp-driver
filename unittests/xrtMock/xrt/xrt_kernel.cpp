#include "xrt_kernel.h"

#include <iostream>

namespace xrt {
    kernel::kernel(const xrt::device& device, const xrt::uuid& xclbin_id, const std::string& name, cu_access_mode mode) {
        kernel_device.emplace_back(device);
        kernel_uuid.emplace_back(xclbin_id);
        kernel_name.emplace_back(name);
    }

    void run::start() {}

    void run::wait() {}
}  // namespace xrt