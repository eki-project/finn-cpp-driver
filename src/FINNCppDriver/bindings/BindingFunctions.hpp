#ifndef BINDING_FUNCTIONS_HPP
#define BINDING_FUNCTIONS_HPP
/**
 * Wrapper functions for pybind11 binding generation.
 * 
 * TODO(bwintermann): Doxygen and license header.
 */

#include <FINNCppDriver/utils/ConfigurationStructs.h>

// TODO(bwintermann): Move several inference related functions from the FINNDriver.cpp
// into its own file/base class so that we can reuse it here and have a nicer structure.
#include <FINNCppDriver/FINNDriver.cpp>

using SyncDriver = Finn::Driver<true>;
using AsyncDriver = Finn::Driver<false>;

// Used to enable a single point of entry for both Sync and Async drivers.
// TODO(bwintermann): This should be moved to a common base class to avoid std::variant usage.
using DriverVariant = std::variant<std::reference_wrapper<SyncDriver>, std::reference_wrapper<AsyncDriver>>;
using ConstDriverVariant = std::variant<std::reference_wrapper<const SyncDriver>, std::reference_wrapper<const AsyncDriver>>;

/**
 * @brief Print a warning if no user-defined datatype header was found.
 */
void warn_undefined_header() {
#ifndef FINN_HEADER_LOCATION
    std::cout
        << "(WARNING) It seems like this library was compiled without a custom "
        << "FINN_HEADER_LOCATION. This should not be the case for normal inference usecases "
        << "and can influence performance!" << std::endl;
#endif
}

/**
 * @brief Run the throughput test. Originally taken from FINNDriver.cpp and modified for wrapper use.
 *
 * @param driver Driver object.
 * @param n Number of iterations. (Total num. of samples = batchsize * n * 1)
 */
void throughput_test(DriverVariant driver, unsigned int n) {
    warn_undefined_header();
    if (std::holds_alternative<std::reference_wrapper<AsyncDriver>>(driver)) {
        throw std::runtime_error("Async driver does not yet support throughput_test.");
    }
    auto d = std::get<std::reference_wrapper<SyncDriver>>(driver);
    size_t elementcount = FinnUtils::shapeToElements(
        (std::static_pointer_cast<Finn::ExtendedBufferDescriptor>(
            d.get().getConfig().deviceWrappers[0].idmas[0])
        )->normalShape
    );
    uint batchSize = d.get().getBatchSize();

    if constexpr(InputFinnType().isInteger()) {
        using dtype = Finn::UnpackingAutoRetType::IntegralType<InputFinnType>;
        runThroughputTestImpl<dtype>(d.get(), elementcount, batchSize, n);
        // benchmark each step in call chain for int
    } else {
        runThroughputTestImpl<float>(d.get(), elementcount, batchSize, n);
    }
}

/**
 * @brief Print an overview of the current config.
 */
void print_config(ConstDriverVariant driver) {
    warn_undefined_header();
    Finn::Config config = std::visit([](const auto& v) { return v.get().getConfig(); }, driver);
    std::cout << "CONFIGURATION" << std::endl << "------------------" << std::endl;
    std::cout << "Input Bitwidth: " << InputFinnType().bitwidth() << std::endl;
    std::cout << "Output Bitwidth: " << OutputFinnType().bitwidth() << std::endl;
    std::cout << "------------------" << std::endl;
    for (Finn::DeviceWrapper &wrapper : config.deviceWrappers) {
        std::cout << "DEVICE " << wrapper.xrtDeviceIndex << std::endl;
        std::cout << "\tXCLBIN: " << wrapper.xclbin << std::endl;
        for (const std::shared_ptr<Finn::BufferDescriptor>& idma : wrapper.idmas) {
            std::cout << "\tIDMA: " << idma->kernelName << std::endl;
            std::cout << "\t\tPacked Shape: ";
            for (auto elem : idma->packedShape) {
                std::cout << elem << " ";
            }
        }
        std::cout << "\n";
        for (const std::shared_ptr<Finn::BufferDescriptor>& odma : wrapper.odmas) {
            std::cout << "\tODMA: " << odma->kernelName << std::endl;
            std::cout << "\t\tPacked Shape: ";
            for (auto elem : odma->packedShape) {
                std::cout << elem << " ";
            }
        }
        std::cout << "\n";
    }
}
#endif