#ifndef XCLBIN_H
#define XCLBIN_H

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

        std::vector<kernel> get_kernels() const { return {}; }

        class ip
        {
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
        };


    private:
    };

}  // namespace xrt

#endif  // !XCLBIN_H