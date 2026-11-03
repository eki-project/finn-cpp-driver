#ifndef XCLBIN_H
#define XCLBIN_H

#include <cstdint>
#include <string>
#include <vector>

namespace xrt {

    class kernel;

    class xclbin {
         public:
        xclbin() = default;
        xclbin(xclbin&&) = default;
        xclbin(const xclbin&) = default;
        explicit xclbin(const std::string& filename){};
        xclbin& operator=(xclbin&&) = default;
        xclbin& operator=(const xclbin&) = default;
        ~xclbin() = default;

        std::vector<kernel> get_kernels() const;

        /**
         * @brief Mock of a memory bank of the memory topology. Every mocked memory is HBM bank 0.
         */
        class mem {
             public:
            enum class memory_type : uint8_t { ddr3, ddr4, dram, streaming, preallocated_global, are, hbm, bram, uram, streaming_connection, host };

            memory_type get_type() const { return memory_type::hbm; }
            int32_t get_index() const { return 0; }
        };

        /**
         * @brief Mock of an IP argument. Every mocked argument is connected to a single memory bank.
         */
        class arg {
             public:
            std::string get_name() const { return "addr"; }
            std::vector<mem> get_mems() const { return {mem{}}; }
        };

        class ip {
             private:
            /* data */
             public:
            ip() = default;
            ~ip() = default;
            ip(ip&&) = default;
            ip(const ip&) = default;
            ip& operator=(ip&&) = default;
            ip& operator=(const ip&) = default;

            std::string get_name() const { return ""; }
            std::vector<arg> get_args() const { return {arg{}}; }
        };

        /**
         * @brief Mock: every name matches exactly one IP with one memory-mapped argument in bank 0
         */
        std::vector<ip> get_ips(const std::string& name) const { return {ip{}}; }


         private:
    };

}  // namespace xrt

#endif  // !XCLBIN_H
