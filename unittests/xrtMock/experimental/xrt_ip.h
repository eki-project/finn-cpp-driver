#ifndef XRT_IP
#define XRT_IP
// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2021-2022 Xilinx, Inc. All rights reserved.
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

#include <cstdint>

#include "xrt.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_uuid.h"


namespace xrt {

    /*!
     * @class ip
     *
     * @brief
     * xrt::ip represent the custom IP
     *
     * @details The ip can be controlled through read- and write register
     * only.  If the IP supports interrupt notification, then xrt::ip
     * objects supports enabling and control of underlying IP interrupt.
     *
     * In order to construct an ip object, the following requirements must be met:
     *
     *   - The custom IP must appear in IP_LAYOUT section of xclbin.
     *   - The custom IP must have a base address such that it can be controlled
     *     through register access at offsets from base address.
     *   - The custom IP must have an address range so that write and read access
     *     to base address offset can be validated.
     *   - XRT supports exclusive access only for the custom IP, this is to other
     *     processes from accessing the same IP at the same time.
     */
    class ip {
         public:
        /**
         * ip() - Construct empty ip object
         */
        ip() {}

        /**
         * ip() - Constructor from a device and xclbin
         *
         * @param device
         *  Device programmed with the IP
         * @param xclbin_id
         *  UUID of the xclbin with the IP
         * @param name
         *  Name of IP to construct
         *
         * The IP is opened with exclusive access meaning that no other
         * xrt::ip objects can use the same IP, nor will another process be
         * able to use the IP while one process has been granted access.
         *
         * Constructor throws on error.
         */
        ip(const xrt::device& device, const xrt::uuid& xclbin_id, const std::string& name){};

        /**
         * write_register() - Write to the address range of an ip
         *
         * @param offset
         *  Offset in register space to write to
         * @param data
         *  Data to write
         *
         */
        void write_register(uint32_t offset, uint32_t data){};

        /**
         * read_register() - Read data from ip address range
         *
         * @param offset
         *  Offset in register space to read from
         * @return
         *  Value read from offset
         *
         */
        uint32_t read_register(uint32_t offset) const {
            if (offset == 0x0) {
                return 0x4;
            }
            return 0;
        };
    };

}  // namespace xrt

#endif  // XRT_IP
