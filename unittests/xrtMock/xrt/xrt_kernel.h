#ifndef XRT_KERNEL
#define XRT_KERNEL

#include <cstdint>
#include <memory>
#include <vector>

#include "../ert.h"
#include "../experimental/xclbin.h"
#include "xrt_bo.h"
#include "xrt_device.h"
#include "xrt_uuid.h"

namespace xrt {

    class run {
         public:
        run() = default;
        void start();
        ert_cmd_state wait();
        ert_cmd_state wait(unsigned int);
        ert_cmd_state state();
    };

    /*!
     * @class kernel
     *
     * A kernel object represents a set of instances matching a specified name.
     * The kernel is created by finding matching kernel instances in the
     * currently loaded xclbin.
     *
     * Most interaction with kernel objects are through \ref xrt::run objects created
     * from the kernel object to represent an execution of the kernel
     */
    // class kernel_impl;
    class kernel {
         public:
        /**
         * cu_access_mode - compute unit access mode
         *
         * @var shared
         *  CUs can be shared between processes
         * @var exclusive
         *  CUs are owned exclusively by this process
         */
        enum class cu_access_mode : uint8_t { exclusive = 0, shared = 1, none = 2 };

        /**
         * kernel() - Construct for empty kernel
         */
        kernel() = default;

        /**
         * kernel() - Constructor from a device and xclbin
         *
         * @param device
         *  Device on which the kernel should execute
         * @param xclbin_id
         *  UUID of the xclbin with the kernel
         * @param name
         *  Name of kernel to construct
         * @param mode
         *  Open the kernel instances with specified access (default shared)
         *
         * The kernel name must uniquely identify compatible kernel
         * instances (compute units).  Optionally specify which kernel
         * instance(s) to open using
         * "kernelname:{instancename1,instancename2,...}" syntax.  The
         * compute units are default opened with shared access, meaning that
         * other kernels and other process will have shared access to same
         * compute units.
         */
        inline static std::vector<std::string> kernel_name;
        inline static std::vector<xrt::device> kernel_device;
        inline static std::vector<xrt::uuid> kernel_uuid;
        kernel(const xrt::device& device, const xrt::uuid& xclbin_id, const std::string& name, cu_access_mode mode = cu_access_mode::shared);


        /// @cond
        /// Experimental in 2022.2

        // kernel(const xrt::hw_context& ctx, const std::string& name);
        /// @endcond


        /**
         * operator() - Invoke the kernel function
         *
         * @param args
         *  Kernel arguments
         * @return
         *  Run object representing this kernel function invocation
         */
        //   template<typename ...Args>
        //   run
        //   operator() (Args&&... args)
        //   {
        //     run r(*this);
        //     r(std::forward<Args>(args)...);
        //     return r;
        //   }

        /**
         * group_id() - Get the memory bank group id of an kernel argument
         *
         * @param argno
         *  The argument index
         * @return
         *  The memory group id to use when allocating buffers (see xrt::bo)
         *
         * The function throws if the group id is ambigious.
         */

        // int group_id(int argno) const;

        // uint32_t read_register(uint32_t offset) const;

        /**
         * get_name() - Return the name of the kernel
         */
        std::string get_name() const;

        /**
         * get_xclbin() - Return the xclbin containing the kernel
         */

        // xrt::xclbin get_xclbin() const;

         public:
        /// @cond
        // const std::shared_ptr<kernel_impl>& get_handle() const { return handle; }
        /// @endcond
        run operator()(xrt::bo& buffer) { return run(); }

        run operator()(xrt::bo& buffer, int batchsize) { return run(); }
    };

}  // namespace xrt

#endif  // XRT_KERNEL
