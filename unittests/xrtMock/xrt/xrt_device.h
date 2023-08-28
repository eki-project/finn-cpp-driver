#ifndef XRT_DEVICE_H
#define XRT_DEVICE_H

#include <memory>
#include <string>

#include "xrt_uuid.h"

namespace xrt {
    class device {
         public:
        /**
         * device() - Constructor for empty device
         */
        device() = default;

        /**
         * device() - Dtor
         */
        ~device() = default;

        /**
         * device() - Constructor from device index
         *
         * @param didx
         *  Device index
         *
         * Throws if no device is found matching the specified index.
         */
        explicit device(unsigned int didx);

        /**
         * device() - Constructor from string
         *
         * @param bdf
         *  String identifying the device to open.
         *
         * If the string is in BDF format it matched against devices
         * installed on the system.  Otherwise the string is assumed
         * to be a device index.
         *
         * Throws if string format is invalid or no matching device is
         * found.
         */
        // explicit device(const std::string& bdf);

        /// @cond
        /**
         * device() - Constructor from device index
         *
         * @param didx
         *  Device index
         *
         * Provided to resolve ambiguity in conversion from integral
         * to unsigned int.
         */
        explicit device(int didx) : device(static_cast<unsigned int>(didx)) {}
        /// @endcond

        /// @cond
        /**
         * device() - Constructor from opaque handle
         *
         * Implementation defined constructor
         */
        // explicit device(std::shared_ptr<xrt_core::device> hdl) : handle(std::move(hdl)) {}
        /// @endcond

        /**
         * device() - Create a managed device object from a shim xclDeviceHandle
         *
         * @param dhdl
         *  Shim xclDeviceHandle
         * @return
         *  xrt::device object epresenting the opened device, or exception on error
         */
        // explicit device(xclDeviceHandle dhdl);

        /**
         * device() - Copy ctor
         */
        device(const device& rhs) = default;

        /**
         * operator= () - Move assignment
         */
        device& operator=(const device& rhs) = default;

        /**
         * device() - Move ctor
         */
        device(device&& rhs) = default;

        /**
         * operator= () - Move assignment
         */
        device& operator=(device&& rhs) = default;

        /// @cond
        /// Experimental 2022.2
        /**
         * register_xclbin() - Register an xclbin with the device
         *
         * @param xclbin
         *  xrt::xclbin object
         * @return
         *  UUID of argument xclbin
         *
         * This function registers an xclbin with the device, but
         * does not associate the xclbin with hardware resources.
         */
        // uuid register_xclbin(const xrt::xclbin& xclbin);
        /// @endcond

        /**
         * load_xclbin() - Load an xclbin
         *
         * @param xclbin
         *  Pointer to xclbin in memory image
         * @return
         *  UUID of argument xclbin
         */
        // uuid load_xclbin(const axlf* xclbin);

        /**
         * load_xclbin() - Read and load an xclbin file
         *
         * @param xclbin_fnm
         *  Full path to xclbin file
         * @return
         *  UUID of argument xclbin
         *
         * This function reads the file from disk and loads
         * the xclbin.   Using this function allows one time
         * allocation of data that needs to be kept in memory.
         */
        uuid load_xclbin(const std::string& xclbin_fnm);

        /**
         * load_xclbin() - load an xclin from an xclbin object
         *
         * @param xclbin
         *  xrt::xclbin object
         * @return
         *  UUID of argument xclbin
         *
         * This function uses the specified xrt::xclbin object created by
         * caller.  The xrt::xclbin object must contain the complete axlf
         * structure.
         */
        // uuid load_xclbin(const xrt::xclbin& xclbin);

        /**
         * get_xclbin_uuid() - Get UUID of xclbin image loaded on device
         *
         * @return
         *  UUID of currently loaded xclbin
         *
         * Note that current UUID can be different from the UUID of
         * the xclbin loaded by this process using load_xclbin()
         */
        uuid get_xclbin_uuid() const;


         public:
        // std::shared_ptr<xrt_core::device> get_handle() const { return handle; }

        void reset();

        // explicit operator bool() const { return handle != nullptr; }
        /// @endcond

         protected:
        uuid loadedUUID;
    };
}  // namespace xrt

#endif  // !XRT_DEVICE_H