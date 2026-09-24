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

namespace py = pybind11;

using SyncDriver = Finn::Driver<true>;
using AsyncDriver = Finn::Driver<false>;

// Used to enable a single point of entry for both Sync and Async drivers.
// TODO(bwintermann): This should be moved to a common base class to avoid std::variant usage.
using DriverVariant = std::variant<std::reference_wrapper<SyncDriver>, std::reference_wrapper<AsyncDriver>>;
using ConstDriverVariant = std::variant<std::reference_wrapper<const SyncDriver>, std::reference_wrapper<const AsyncDriver>>;

// Input datatype for inference
using InputDtype = Finn::UnpackingAutoRetType::IntegralType<InputFinnType>;

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


/**
 * @brief Format the given shape into a string for display in errors, etc.
 */
template<typename T>
std::string formatShape(std::span<const T> shape) {
    std::string s = "(";
    for (std::size_t i = 0; i < shape.size(); ++i) {
        s += std::to_string(shape[i]);
        if (i != shape.size() - 1) {
            s += ", ";
        }
    }
    s += ")";
    return s;
}

/**
 * @brief Check that the passed shapes match in length and content. Otherwise throw an std::runtime_error.
 */
template<typename T, typename S>
void checkMatchingShapes(std::span<T> expected, std::span<S> received) {
    std::string error = std::format(
        "Shape mismatch: Expected shape {} but got shape {}.",
        formatShape<T>(expected),
        formatShape<S>(received)
    );
    if (expected.size() != received.size()) {
        throw std::runtime_error(error);
    }
    if (expected[0] != received[0]) {
        throw std::runtime_error(
            std::format(
                "Mismatch in batch sizes. Driver expected a batch size of {} (shape: {}) but received batch size of {} (shape: {}). Use the method 'set_batch_size(...)' to update the batch size.",
                expected[0],
                formatShape<T>(expected),
                received[0],
                formatShape<S>(received)
            )
        );
    }
    for(std::size_t i = 0; i < expected.size(); ++i) {
        if (expected[i] != received[i]) {
            throw std::runtime_error(error);
        }
    }
}


/**
 * @brief Infer a single numpy array synchronously. Expects the first dimension to be the batch size.
 */
py::array_t<SyncDriver::AutoDeducedRetType> inferNumpyArraySynchronous(SyncDriver& driver, py::array_t<InputDtype, py::array::c_style> array) {
    // Read expected shape from driver config. The first element is the batch size.  
    auto expectedShape = getDefaultIDMA(driver)->normalShape;
    expectedShape[0] = driver.getBatchSize();

    // Request buffer info on the incoming array to read its shape
    py::buffer_info binfo = array.request();

    // Make sure shapes match
    // TODO(bwinterman): Remove to allow batched inference as well
    checkMatchingShapes<unsigned int, pybind11::ssize_t>(expectedShape, binfo.shape);

    // ALTERNATIVE: std::span<InputDtype> data(array.mutable_data(), array.size());
    auto mut_data = array.mutable_data();
    Finn::vector<SyncDriver::AutoDeducedRetType> results = driver.inferSynchronous(mut_data, mut_data + array.size());
    static auto outputShape = getDefaultODMA(driver)->normalShape;

    // TODO(bwintermann): The resulting numpy array does NOT own the result data. To fix this run a std::memcpy between the vector data
    // and the py::array_t. Inspect performance
    // FIXME(bwintermann): Currently this returns all zeroes.
    return py::array_t<SyncDriver::AutoDeducedRetType>(outputShape, results.data());
}

#endif