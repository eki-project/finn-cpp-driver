#ifndef XRT_IP
#define XRT_IP
// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2021-2022 Xilinx, Inc. All rights reserved.
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <utility>

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
        inline static std::vector<std::string> kernel_name;
        inline static std::vector<xrt::device> kernel_device;
        inline static std::vector<xrt::uuid> kernel_uuid;
        /**
         * @brief Last value written to each register, per IP name and offset
         */
        inline static std::map<std::pair<std::string, uint32_t>, uint32_t> registers;
        /**
         * @brief (prod, cons) offsets of a ring DMA: reading cons returns the last value written to prod, i.e. every
         * posted descriptor completes immediately
         */
        inline static std::optional<std::pair<uint32_t, uint32_t>> ringCompletesImmediately;
        /**
         * @brief Guards registers against the worker threads of the asynchronous buffers
         */
        inline static std::mutex registersMutex;

        /**
         * @brief Thread-safe read of a register from the test
         */
        static uint32_t peek(const std::string& name, uint32_t offset) {
            const std::lock_guard<std::mutex> lock(registersMutex);
            const auto it = registers.find({name, offset});
            return it == registers.end() ? 0 : it->second;
        }

        /**
         * @brief Thread-safe write of a register from the test, e.g. to complete ring descriptors
         */
        static void poke(const std::string& name, uint32_t offset, uint32_t value) {
            const std::lock_guard<std::mutex> lock(registersMutex);
            registers[{name, offset}] = value;
        }
        std::string ipName;
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
        ip(const xrt::device& device, const xrt::uuid& xclbin_id, const std::string& name) {
            FINN_LOG(loglevel::debug) << "[xrt::ip mock]"
                                      << "Create kernel with name: " << name;
            kernel_device.emplace_back(device);
            kernel_uuid.emplace_back(xclbin_id);
            kernel_name.emplace_back(name);
            ipName = name;
        };

        /**
         * write_register() - Write to the address range of an ip
         *
         * @param offset
         *  Offset in register space to write to
         * @param data
         *  Data to write
         *
         */
        void write_register(uint32_t offset, uint32_t data) { poke(ipName, offset, data); };

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
                return 0x4;  // ap_idle: every invocation completes immediately
            }
            if (ringCompletesImmediately && offset == ringCompletesImmediately->second) {
                offset = ringCompletesImmediately->first;
            }
            return peek(ipName, offset);
        };
    };

}  // namespace xrt

#endif  // XRT_IP
