#ifndef XCLBIN_H
#define XCLBIN_H

namespace xrt {

    class xclbin {
         public:
        xclbin() = default;
        xclbin(xclbin&&) = default;
        xclbin(const xclbin&) = default;
        xclbin& operator=(xclbin&&) = default;
        xclbin& operator=(const xclbin&) = default;
        ~xclbin() = default;

         private:
    };

}  // namespace xrt

#endif  // !XCLBIN_H