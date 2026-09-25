#ifndef BINDING_FUNCTIONS_HPP
#define BINDING_FUNCTIONS_HPP
/**
 * Wrapper functions for pybind11 binding generation.
 *
 * TODO(bwintermann): Doxygen and license header.
 */

#include <FINNCppDriver/config/FinnDriverUsedDatatypes.h>
#include <FINNCppDriver/utils/ConfigurationStructs.h>
#include <FINNCppDriver/utils/FinnUtils.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl/filesystem.h>
#include <pybind11/stl_bind.h>

#include <FINNCppDriver/StandaloneExecutableDriver.hpp>

namespace py = pybind11;

// TODO(bwintermann): FINN should generate a typename for this as well so we don't have to do it manually here
using SyncDriver = Finn::StandaloneExecutableDriver<true, InputFinnType, OutputFinnType>;
using AsyncDriver = Finn::StandaloneExecutableDriver<false, InputFinnType, OutputFinnType>;

// Used to enable a single point of entry for both Sync and Async drivers.
// TODO(bwintermann): This should be moved to a common base class to avoid std::variant usage.
using DriverVariant = std::variant<std::reference_wrapper<SyncDriver>, std::reference_wrapper<AsyncDriver>>;
using ConstDriverVariant = std::variant<std::reference_wrapper<const SyncDriver>, std::reference_wrapper<const AsyncDriver>>;

// Input datatype for inference
using InputDtype = Finn::UnpackingAutoRetType::IntegralType<InputFinnType>;

/**
 * @brief Print a warning if no user-defined datatype header was found.
 */
void warnUndefinedHeader() {
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
py::array_t<InputDtype> generateExampleInputNumpy(SyncDriver& driver) { return py::array_t<InputDtype>(driver.getInputNormalShape(true)); }

/**
 * @brief Run the throughput test. Originally taken from FINNDriver.cpp and modified for wrapper use.
 *
 * @param driver Driver object.
 * @param n Number of iterations. (Total num. of samples = batchsize * n * 1)
 */
void throughputTest(DriverVariant driver, unsigned int n) {
    warnUndefinedHeader();
    if (std::holds_alternative<std::reference_wrapper<AsyncDriver>>(driver)) {
        throw std::runtime_error("Async driver does not yet support throughput_test.");
    }
    auto syncdriver = std::get<std::reference_wrapper<SyncDriver>>(driver);
    size_t elementcount = FinnUtils::shapeToElements(syncdriver.get().getInputNormalShape());
    uint batchSize = syncdriver.get().getBatchSize();

    // FIXME: clangd mentions that the content is not constant expression. Is there a reason why isInteger cannot be static?
    if constexpr (InputFinnType().isInteger()) {
        using dtype = Finn::UnpackingAutoRetType::IntegralType<InputFinnType>;
        syncdriver.get().runThroughputTestImpl<dtype>(elementcount, batchSize, n);
        // benchmark each step in call chain for int
    } else {
        syncdriver.get().runThroughputTestImpl<float>(elementcount, batchSize, n);
    }
}

/**
 * @brief Print an overview of the current config.
 */
void printConfig(ConstDriverVariant driver) {
    warnUndefinedHeader();
    Finn::Config config = std::visit([](const auto& v) { return v.get().getConfig(); }, driver);
    std::cout << "CONFIGURATION" << std::endl << "------------------" << std::endl;
    std::cout << "Input Bitwidth: " << InputFinnType().bitwidth() << std::endl;
    std::cout << "Output Bitwidth: " << OutputFinnType().bitwidth() << std::endl;
    std::cout << "------------------" << std::endl;
    for (Finn::DeviceWrapper& wrapper : config.deviceWrappers) {
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
void checkMatchingShapes(std::span<T> expected, std::span<S> received) {
    std::string error = std::format("Shape mismatch: Expected shape {} but got shape {}.", formatShape<T>(expected), formatShape<S>(received));
    if (expected.size() != received.size()) {
        throw std::runtime_error(error);
    }
    if (expected[0] != received[0]) {
        throw std::runtime_error(std::format("Mismatch in batch sizes. Driver expected a batch size of {} (shape: {}) but received batch size of {} (shape: {}). Use the method 'set_batch_size(...)' to update the batch size.", expected[0],
                                             formatShape<T>(expected), received[0], formatShape<S>(received)));
    }
    for (std::size_t i = 0; i < expected.size(); ++i) {
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
    shape_t expectedShape = driver.getInputNormalShape(true);

    // Request buffer info on the incoming array to read its shape
    py::buffer_info binfo = array.request();

    // Make sure shapes match
    // TODO(bwinterman): Remove to allow batched inference as well
    checkMatchingShapes<unsigned int, pybind11::ssize_t>(expectedShape, binfo.shape);

    // ALTERNATIVE: std::span<InputDtype> data(array.mutable_data(), array.size());
    InputDtype* mutData = array.mutable_data();
    Finn::vector<SyncDriver::AutoDeducedRetType> results = driver.inferSynchronous(mutData, mutData + array.size());
    static shape_t outputShape = driver.getOutputNormalShape(true);

    // TODO(bwintermann): The resulting numpy array does NOT own the result data. To fix this run a std::memcpy between the vector data
    // and the py::array_t. Inspect performance
    // FIXME(bwintermann): Currently this returns all zeroes.
    return py::array_t<SyncDriver::AutoDeducedRetType>(outputShape, results.data());
}

#endif