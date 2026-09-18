#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>
#include <pybind11/numpy.h>
#include <FINNCppDriver/bindings/BindingFunctions.hpp>

namespace py = pybind11;


PYBIND11_MODULE(finnhpcpy, m) {
    m.doc() = "FINN HPC Driver Python bindings.";

    py::class_<SyncDriver>(m, "FINNSyncDriver", "Synchronous variant of the driver class.")
        // Contructor
        .def(py::init<const std::string&, unsigned int>(), py::arg("config_file"), py::arg("batch_size"))

        // Config overview
        .def("print_config", [](const SyncDriver& driver) {print_config(driver);}, "Print an overview of the driver configuration.")

        // Throughput test
        // TODO(bwintermann): This should not only be a method on the python side, but also here on the C++ side
        .def(
            "throughput_test",
            [](SyncDriver& driver, unsigned int n) { throughput_test(driver, n); },
            "Run a throughput test on the driver with the given number of iterations (total: batchsize x n).",
            py::arg("n")
        )

        .def(
            "infer_numpy",
            [](SyncDriver& driver, py::array& array) {
                // TODO
                //auto results = driver.inferSynchronous(array.begin(), array.end());
            }
        );

}