#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl/filesystem.h>
#include <pybind11/stl_bind.h>

#include <FINNCppDriver/bindings/BindingFunctions.hpp>

namespace py = pybind11;


PYBIND11_MODULE(finnhpcpy, m) {
    m.doc() = "FINN HPC Driver Python bindings.";

    py::class_<SyncDriver>(m, "FINNSyncDriver", "Synchronous variant of the driver class.")
        // Contructor (can receive both pathlib.Path and str)
        .def(py::init([](const py::object& configFile, unsigned int batchSize, bool enableLogger) {
                 warnUndefinedHeader();
                 if (enableLogger) {
                     Logger::initLogger(true);
                 }
                 return SyncDriver(py::str(configFile), batchSize);
             }),
             py::arg("config_file"), py::arg("batch_size"), py::arg("enable_logger") = false, py::doc("Construct a sync driver. This resets the FPGAs and prepares for inference."))

        // Example numpy array so that the Python side knows what dtype and shape to use.
        .def("example_input_numpy", &generateExampleInputNumpy, py::doc("Return a numpy array with the correct shape and datatype for inference. This takes batch size into account as the first element of the shape."))

        // Config overview
        .def(
            "print_config", [](const SyncDriver& driver) { printConfig(driver); }, py::doc("Print an overview of the driver configuration."))

        // Throughput test
        // TODO(bwintermann): This should not only be a method on the python side, but also here on the C++ side
        .def(
            "throughput_test", [](SyncDriver& driver, unsigned int n) { throughputTest(driver, n); }, py::arg("n"), py::doc("Run a throughput test on the driver with the given number of iterations (total: batchsize x n)."))

        // Getter and setter for batch sizes
        .def(
            "get_batch_size", [](SyncDriver& driver) { return driver.getBatchSize(); }, py::doc("Get the currently set batch size. Update with set_batch_size."))
        .def(
            "set_batch_size", [](SyncDriver& driver, unsigned int batchSize) { driver.setBatchSize(batchSize); }, py::arg("batch_size"),
            py::doc("Set internal batch size of the driver. Inference NumPy arrays must have shape (batch_size, ...)."))

        // Inference of a single numpy array
        // TODO(bwintermann): Have a single `infer` function that receives a variant of all possible data types and
        // decides which one to use automatically.
        .def(
            "infer_numpy", [](SyncDriver& driver, py::array_t<InputDtype, py::array::c_style>& array) { return inferNumpyArraySynchronous(driver, array); }, py::arg("input_data"),
            py::doc("Run inference of a single numpy array. The shape must match the normal input shape expected by the accelerator."))

        // Default device indices
        .def("set_default_input_device_index", &SyncDriver::setDefaultInputDeviceIndex, py::arg("index"), py::doc("Set the device index for the default input device."))
        .def("set_default_output_device_index", &SyncDriver::setDefaultOutputDeviceIndex, py::arg("index"), py::doc("Set the device index for the default output device."))
        .def("get_default_input_device_index", &SyncDriver::getDefaultInputDeviceIndex, py::doc("Get the default input device index."))
        .def("get_default_output_device_index", &SyncDriver::getDefaultOutputDeviceIndex, py::doc("Get the default output device index."));
}