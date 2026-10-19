#ifndef BINDING_FUNCTIONS_HPP
#define BINDING_FUNCTIONS_HPP
/**
 * Wrapper functions for pybind11 binding generation.
 *
 * TODO(bwintermann): Doxygen and license header.
 */

#include <FINNCppDriver/utils/ConfigurationStructs.h>
#include <FINNCppDriver/utils/FinnUtils.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl/filesystem.h>
#include <pybind11/stl_bind.h>

// Selects the FINN-generated datatype header via FINN_HEADER_LOCATION (or the default test header without it).
// It must not be included here directly: the generated header shares the include guard of the default one, so
// whichever is included first wins.
#include <FINNCppDriver/StandaloneExecutableDriver.hpp>
#include <format>

namespace py = pybind11;

// TODO(bwintermann): FINN should generate a typename for this as well so we don't have to do it manually here
using SyncDriver = Finn::StandaloneExecutableDriver<true, InputFinnType, OutputFinnType>;
using AsyncDriver = Finn::StandaloneExecutableDriver<false, InputFinnType, OutputFinnType>;

// Used to enable a single point of entry for both Sync and Async drivers.
// TODO(bwintermann): This should be moved to a common base class to avoid std::variant usage.
using DriverVariant = std::variant<std::reference_wrapper<SyncDriver>, std::reference_wrapper<AsyncDriver>>;
using ConstDriverVariant = std::variant<std::reference_wrapper<const SyncDriver>, std::reference_wrapper<const AsyncDriver>>;

// Input datatype for inference
using InputDtype = Finn::UnpackingAutoRetType::AutoRetType<InputFinnType>;

/**
 * @brief Print a warning if no user-defined datatype header was found.
 */
inline void warnUndefinedHeader() {
#ifndef FINN_HEADER_LOCATION
    std::cout << "(WARNING) It seems like this library was compiled without a custom "
              << "FINN_HEADER_LOCATION. This should not be the case for normal inference usecases "
              << "and can influence performance!" << std::endl;
#endif
}

/**
 * @brief Generate and return a numpy array object with the correct shape and
 * datatype for inference. Can be used from the python side to create correctly defined inputs.
 */
inline py::array_t<InputDtype> generateExampleInputNumpy(SyncDriver& driver) {
    py::array_t<InputDtype> array(driver.getInputNormalShape(true));
    std::fill(array.mutable_data(), array.mutable_data() + array.size(), InputDtype{});
    return array;
}

/**
 * @brief Run the throughput test. Originally taken from FINNDriver.cpp and modified for wrapper use.
 *
 * @param driver Driver object.
 * @param n Number of iterations. (Total num. of samples = batchsize * n * 1)
 */
inline void throughputTest(DriverVariant driver, unsigned int n) {
    warnUndefinedHeader();
    if (std::holds_alternative<std::reference_wrapper<AsyncDriver>>(driver)) {
        throw std::runtime_error("Async driver does not yet support throughput_test.");
    }
    auto syncdriver = std::get<std::reference_wrapper<SyncDriver>>(driver);
    size_t elementcount = FinnUtils::shapeToElements(syncdriver.get().getInputNormalShape());
    uint batchSize = syncdriver.get().getBatchSize();

    const py::gil_scoped_release release;  // the test runs for a long time and does not touch Python objects
    if constexpr (InputFinnType().isInteger()) {
        using dtype = Finn::UnpackingAutoRetType::IntegralType<InputFinnType>;
        syncdriver.get().runThroughputTestImpl<dtype>(elementcount, batchSize, n);
    } else {
        syncdriver.get().runThroughputTestImpl<float>(elementcount, batchSize, n);
    }
}

/**
 * @brief Print an overview of the current config.
 */
inline void printConfig(ConstDriverVariant driver) {
    warnUndefinedHeader();
    Finn::Config config = std::visit([](const auto& v) { return v.get().getConfig(); }, driver);
    std::cout << "CONFIGURATION" << std::endl << "------------------" << std::endl;
    std::cout << "Input Bitwidth: " << InputFinnType().bitwidth() << std::endl;
    std::cout << "Output Bitwidth: " << OutputFinnType().bitwidth() << std::endl;
    std::cout << "------------------" << std::endl;
    for (const auto& [xrtDeviceIndex, wrapper] : config.deviceWrappers) {
        std::cout << "DEVICE " << xrtDeviceIndex << std::endl;
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
    std::string shapeString = "(";
    for (std::size_t i = 0; i < shape.size(); ++i) {
        shapeString += std::to_string(shape[i]);
        if (i != shape.size() - 1) {
            shapeString += ", ";
        }
    }
    shapeString += ")";
    return shapeString;
}

/**
 * @brief Check that the passed shapes match in length and content. Otherwise throw an std::runtime_error.
 */
template<typename T, typename S>
void checkMatchingShapes(std::span<const T> expected, std::span<const S> received) {
    const auto mismatch = [&]() { return std::format("Shape mismatch: Expected shape {} but got shape {}.", formatShape<T>(expected), formatShape<S>(received)); };
    if (expected.size() != received.size() || expected.empty()) {
        throw std::runtime_error(mismatch());
    }
    if (static_cast<std::int64_t>(expected[0]) != static_cast<std::int64_t>(received[0])) {
        throw std::runtime_error(std::format("Mismatch in batch sizes. Driver expected a batch size of {} (shape: {}) but received batch size of {} (shape: {}). Use the method 'set_batch_size(...)' to update the batch size.", expected[0],
                                             formatShape<T>(expected), received[0], formatShape<S>(received)));
    }
    for (std::size_t i = 1; i < expected.size(); ++i) {
        if (static_cast<std::int64_t>(expected[i]) != static_cast<std::int64_t>(received[i])) {
            throw std::runtime_error(mismatch());
        }
    }
}


/**
 * @brief Infer a single numpy array synchronously. Expects the first dimension to be the batch size.
 */
inline py::array_t<SyncDriver::AutoDeducedRetType> inferNumpyArraySynchronous(SyncDriver& driver, const py::array_t<InputDtype, py::array::c_style | py::array::forcecast>& array) {
    // Read expected shape from driver config. The first element is the batch size.
    const shape_t expectedShape = driver.getInputNormalShape(true);

    // Request buffer info on the incoming array to read its shape
    const py::buffer_info binfo = array.request();

    checkMatchingShapes<unsigned int, pybind11::ssize_t>(expectedShape, binfo.shape);

    // The input is only read, so read-only arrays (views, np.frombuffer) are accepted too
    const InputDtype* data = array.data();
    const auto elements = static_cast<std::size_t>(array.size());
    const shape_t outputShape = driver.getOutputNormalShape(true);

    using ResultVector = Finn::vector<SyncDriver::AutoDeducedRetType>;
    auto results = std::make_unique<ResultVector>();
    {
        const py::gil_scoped_release release;  // let other Python threads run while the FPGA works
        *results = driver.inferSynchronous(data, data + elements);
    }

    if (results->size() != FinnUtils::shapeToElements(outputShape)) {
        throw std::runtime_error(std::format("Driver returned {} output elements, but the output shape {} needs {}.", results->size(), formatShape<unsigned int>(outputShape), FinnUtils::shapeToElements(outputShape)));
    }

    // Hand the result vector over to NumPy instead of copying it: the capsule becomes the array's base object and frees the vector once the array is garbage collected
    auto* resultData = results->data();
    const py::capsule owner(results.get(), [](void* ptr) { delete static_cast<ResultVector*>(ptr); });
    static_cast<void>(results.release());  // owned by the capsule from here on
    return py::array_t<SyncDriver::AutoDeducedRetType>(outputShape, resultData, owner);
}

#endif
