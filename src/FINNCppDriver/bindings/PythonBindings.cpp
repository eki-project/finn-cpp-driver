#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>
#include <pybind11/numpy.h>
#include <pybind11/stl/filesystem.h>
#include <FINNCppDriver/bindings/BindingFunctions.hpp>

namespace py = pybind11;


PYBIND11_MODULE(finnhpcpy, m) {
    m.doc() = "FINN HPC Driver Python bindings.";

    py::class_<SyncDriver>(m, "FINNSyncDriver", "Synchronous variant of the driver class.")
        // Contructor (can receive both pathlib.Path and str)
        .def(
            py::init(
                [](py::object config_file, unsigned int batch_size) {
                    return SyncDriver(py::str(config_file), batch_size); 
                }
            ),
            py::arg("config_file"),
            py::arg("batch_size")
        )

        // Config overview
        .def("print_config", [](const SyncDriver& driver) {print_config(driver);}, py::doc("Print an overview of the driver configuration."))

        // Throughput test
        // TODO(bwintermann): This should not only be a method on the python side, but also here on the C++ side
        .def(
            "throughput_test",
            [](SyncDriver& driver, unsigned int n) { throughput_test(driver, n); },
            py::arg("n"),
            py::doc("Run a throughput test on the driver with the given number of iterations (total: batchsize x n).")
        )

        // Inference of a single numpy array
        .def(
            "infer_numpy",
            [](SyncDriver& driver, py::array_t<InputDtype, py::array::c_style>& array) {
                return inferNumpyArraySynchronous(driver, array);
            },
            py::arg("input_data"),
            py::doc("Run inference of a single numpy array. The shape must match the normal input shape expected by the accelerator.")
        );

}