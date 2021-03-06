// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.


#include "tiny_dual.h"
#include "tiny_dual_double_utils.h"


typedef double TinyDualScalar;
typedef ::TINY::TinyDualDouble MyScalar;
typedef ::TINY::TinyDualDoubleUtils MyTinyConstants;

#include "pytinydiffsim_includes.h"

using namespace TINY;

namespace py = pybind11;
MyScalar fraction(int a, int b)
{
return MyTinyConstants::fraction(a,b);
}


void MyMassMatrix(TinyMultiBody<MyScalar, MyTinyConstants>& mb, const std::vector<MyScalar>& q,
    TinyMatrixXxX< MyScalar, MyTinyConstants>* M)
{
    mb.mass_matrix( q, M);
}


void MyForwardKinematics(TinyMultiBody<MyScalar, MyTinyConstants>& mb, const std::vector<MyScalar>& q)
{
    mb.forward_kinematics(q);
}

void MyForwardDynamics(TinyMultiBody<MyScalar, MyTinyConstants>& mb, const TinyVector3<MyScalar, MyTinyConstants>& gravity)
{
    mb.forward_dynamics(gravity);
}
void MyIntegrateEuler(TinyMultiBody<MyScalar, MyTinyConstants>& mb, const MyScalar& dt)
{
    mb.integrate(dt);
}


PYBIND11_MODULE(pytinydiffsim_dual, m) {
 
    py::class_<TinyDualDouble>(m, "TinyDualDouble")
        .def(py::init<TinyDualScalar, TinyDualScalar>())
        .def(py::self + py::self)
        .def(py::self - py::self)
        .def(py::self * py::self)
        .def(py::self / py::self)
        .def(py::self += py::self)
        .def(py::self -= py::self)
        .def("real", &TinyDualDouble::real)
        .def("dual", &TinyDualDouble::dual)
        .def(-py::self)
        .def("__repr__",
            [](const TinyDualDouble& a) {
                return "[ real=" + std::to_string(a.real()) + " , dual=" + std::to_string(a.dual()) + "]";
            })
        ;

#include "pytinydiffsim.inl"

}

