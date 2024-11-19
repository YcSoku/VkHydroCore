//
// Created by Yucheng Soku on 2024/11/16.
//

#include <pybind11/pybind11.h>
#include "HydroCore/Core.h"
#include "pybind11/stl.h"

namespace py = pybind11;

void register_core(py::module & m) {
    py::class_<NextHydro::Core>(m, "Core")
            .def(py::init<>())
            .def("initialization", &NextHydro::Core::initialization)
            .def("step", &NextHydro::Core::step);
}

PYBIND11_MODULE(pyHydroCore, m) {
    register_core(m);
}
