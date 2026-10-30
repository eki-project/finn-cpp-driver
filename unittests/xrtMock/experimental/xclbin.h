#ifndef XCLBIN_H
#define XCLBIN_H

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace xrt {

    class kernel;

    class xclbin {
         public:
        xclbin() = default;
        xclbin(xclbin&&) = default;
        xclbin(const xclbin&) = default;
        explicit xclbin(const std::string& filename) {};
        xclbin& operator=(xclbin&&) = default;
        xclbin& operator=(const xclbin&) = default;
        ~xclbin() = default;

        std::vector<kernel> get_kernels() const;

        /**
         * @brief Memory bank of the xclbin's memory topology
         *
         */
        class mem {
             public:
            enum class memory_type : uint8_t { ddr4, dram, hbm, streaming_connection, host };
            mem() = default;
            mem(int32_t pIndex, memory_type pType) : index(pIndex), type(pType) {}
            int32_t get_index() const { return index; }
            memory_type get_type() const { return type; }

             private:
            int32_t index = 0;
            memory_type type = memory_type::hbm;
        };

        /**
         * @brief Argument of a compute unit and the memories it connects to
         *
         */
        class arg {
             public:
            arg() = default;
            explicit arg(std::vector<mem> pMems) : mems(std::move(pMems)) {}
            std::vector<mem> get_mems() const { return mems; }

             private:
            std::vector<mem> mems;
        };

        /**
         * @brief Compute unit ("kernel:cu") of the xclbin
         *
         */
        class ip {
             public:
            ip() = default;
            ip(std::string pName, std::vector<arg> pArgs) : name(std::move(pName)), args(std::move(pArgs)) {}
            std::string get_name() const { return name; }
            std::vector<arg> get_args() const { return args; }

             private:
            std::string name;
            std::vector<arg> args;
        };

        std::vector<ip> get_ips() const { return mockIps; }

        /**
         * @brief Compute units every mock xclbin reports; tests fill it
         *
         */
        static inline std::vector<ip> mockIps;

        /**
         * @brief Report each given compute unit with a memory port in group `group` and one stream port
         *
         * @param names Compute units as the driver configuration names them ("kernel:{cu}"; the xclbin drops the braces)
         * @param group Memory group of their memory port
         */
        static void mockComputeUnits(const std::vector<std::string>& names, int32_t group = 0) {
            for (std::string name : names) {
                std::erase(name, '{');
                std::erase(name, '}');
                mockIps.emplace_back(name, std::vector<arg>{arg({mem(group, mem::memory_type::hbm)}), arg({mem(39, mem::memory_type::streaming_connection)})});
            }
        }

         private:
    };

}  // namespace xrt

#endif  // !XCLBIN_H