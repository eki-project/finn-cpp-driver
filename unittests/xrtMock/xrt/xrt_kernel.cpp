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

    ert_cmd_state run::wait() { return ERT_CMD_STATE_COMPLETED; }

    ert_cmd_state run::wait(unsigned int ms) { return ERT_CMD_STATE_COMPLETED; }

    ert_cmd_state run::state() { return ERT_CMD_STATE_COMPLETED; }

    int kernel::group_id(int argno) const { return 0; }


    std::string kernel::get_name() const { return "testkernel"; }
}  // namespace xrt