#include <mpi4py/mpi4py.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "TMROctForest.h"

PYBIND11_MODULE(TMR, mod) {
  mod.doc() = "TMR: Tools for AMR";

  // Import mpi4py
  if (import_mpi4py() < 0) {
    throw pybind11::error_already_set();
  }

  py::class_<TMROctForest>(mod, "OctForest")
      .def(py::init([](py::object pyobj) {
        MPI_Comm comm = MPI_COMM_SELF;
        if (!pyobj.is_none()) {
          comm = *PyMPIComm_Get(pyobj.ptr());
        }
        return new TMROctForest(comm);
      }))
      .def("setConnectivity", [](TMROctForest& self, py::array_t<int> conn) {
        int num_blocks = conn self.setConnectivity(num_blocks, conn);
      });
}