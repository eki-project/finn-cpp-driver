#include "xrt_kernel.h"

#include <iostream>

#include "../ert.h"

namespace xrt {
    kernel::kernel(const xrt::device& device, const xrt::uuid& xclbin_id, const std::string& name, cu_access_mode mode) {
        kernel_device.emplace_back(device);
        kernel_uuid.emplace_back(xclbin_id);
        kernel_name.emplace_back(name);
    }

    void run::start() {}

    void run::wait() {}

    void run::wait(unsigned int ms) {}

    ert_cmd_state run::state() { return ERT_CMD_STATE_COMPLETED; }


    std::string kernel::get_name() const { return "testkernel"; }
}  // namespace xrt