#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <FINNCppDriver/bindings/BindingFunctions.hpp>

namespace py = pybind11;


PYBIND11_MODULE(finnhpcpy, m) {
    m.doc() = "FINN HPC Driver Python bindings.";

    // Throughput test
    // TODO(bwintermann): This should be a method of the driver. See TODO in BindingFunctions.hpp
    // m.def(
    //     "throughput_test",
    //     &throughput_test,
    //     "Run a throughput test on the driver with the given number of iterations (total: batchsize x n)."
    // );

    py::class_<SyncDriver>(m, "FINNSyncDriver", "Synchronous variant of the driver class.")
        
        // Contructor
        .def(py::init<const std::string&, unsigned int>())

        // Config overview
        .def("print_config", [](const SyncDriver& driver) {print_config(driver);}, "Print an overview of the driver configuration.");
}