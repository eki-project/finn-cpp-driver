#ifndef XRT_BO_H
#define XRT_BO_H

#include <FINNCppDriver/utils/Logger.h>

#include "../xrt.h"
#include "xrt_device.h"

namespace xrt {
    class bo {
         private:
        xrt::device device;
        size_t byteSize;
        unsigned int group;

        void* memmap = nullptr;

        logger_type& logger;

         public:
        /**
         * XCL BO Flags bits layout
         *
         * bits  0 ~ 15: DDR BANK index
         * bits 24 ~ 31: BO flags
         */
#define XRT_BO_FLAGS_MEMIDX_MASK (0xFFFFFFUL)
#define XCL_BO_FLAGS_NONE        (0)
#define XRT_BO_FLAGS_CACHEABLE   (1U << 24)
#define XCL_BO_FLAGS_KERNBUF     (1U << 25)
#define XCL_BO_FLAGS_SGL         (1U << 26)
#define XRT_BO_FLAGS_SVM         (1U << 27)
#define XRT_BO_FLAGS_DEV_ONLY    (1U << 28)
#define XRT_BO_FLAGS_HOST_ONLY   (1U << 29)
#define XRT_BO_FLAGS_P2P         (1U << 30)
#define XCL_BO_FLAGS_EXECBUF     (1U << 31)
        /**
         * @enum flags - buffer object flags
         *
         * @var normal
         *  Create normal BO with host side and device side buffers
         * @var cacheable
         *  Create cacheable BO.  Only effective on embedded platforms.
         * @var device_only
         *  Create a BO with a device side buffer only
         * @var host_only
         *  Create a BO with a host side buffer only
         * @var p2p
         *  Create a BO for peer-to-peer use
         * @var svm
         *  Create a BO for SVM (supported on specific platforms only)
         *
         * The flags used by xrt::bo are compatible with XCL style
         * flags as define in ``xrt_mem.h``
         */
        enum class flags : uint32_t {
            normal = 0,
            cacheable = XRT_BO_FLAGS_CACHEABLE,
            device_only = XRT_BO_FLAGS_DEV_ONLY,
            host_only = XRT_BO_FLAGS_HOST_ONLY,
            p2p = XRT_BO_FLAGS_P2P,
            svm = XRT_BO_FLAGS_SVM,
        };
        bo(xrt::device pDevice, size_t pBytesize, unsigned int pGroup) : device(pDevice), byteSize(pBytesize), group(pGroup), logger(Logger::getLogger()) { FINN_LOG(logger, loglevel::debug) << "(xrtMock) xrt::bo object created!\n"; }
        bo(const xrt::device& pDevice, size_t pBytesize, bo::flags flags, uint32_t pGroup) : device(pDevice), byteSize(pBytesize), group(pGroup), logger(Logger::getLogger()) {
            FINN_LOG(logger, loglevel::debug) << "(xrtMock) xrt::bo object created with flag!\n";
        }

        bo(bo&& other) noexcept : device(std::move(other.device)), byteSize(other.byteSize), group(other.group), memmap(nullptr), logger(Logger::getLogger()) { std::swap(memmap, other.memmap); }

        void sync(xclBOSyncDirection);
        void sync(xclBOSyncDirection dir, size_t sz, size_t offset);
        ~bo();

        /**
         * @brief Create a T and return it - T should be a pointer type. Additionally save copy of it as a void pointer for freeing in destructor
         * @attention This class takes care of freeing the map, this is evil pointer stuff  :<
         *
         * @tparam T
         * @return T
         */
        template<typename T>
        T map() {
            FINN_LOG(logger, loglevel::debug) << "(xrtMock) Map created from xrt::bo with byte size " << byteSize << "!\n";
            T createdMap = static_cast<T>(malloc(byteSize));
            memmap = createdMap;
            return createdMap;
        }

        uint64_t address() const { return 0; };
    };
}  // namespace xrt

#endif