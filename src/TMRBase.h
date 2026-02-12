/*
  This file is part of the package TMR for adaptive mesh refinement.

  Copyright (C) 2015 Georgia Tech Research Corporation.
  Additional copyright (C) 2015 Graeme Kennedy.
  All rights reserved.

  TMR is licensed under the Apache License, Version 2.0 (the "License");
  you may not use this software except in compliance with the License.
  You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#ifndef TMR_BASE_H
#define TMR_BASE_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "mpi.h"

/*
  The following constants define the maximum octant depth and maximum
  element order within the code.
*/
static const int TMR_MAX_LEVEL = 30;

/**
  Base class for all point-evaluation algorithms
*/
class TMRPoint {
 public:
  TMRPoint() {}
  TMRPoint(const TMRPoint& p) {
    x = p.x;
    y = p.y;
    z = p.z;
  }
  TMRPoint(double _x, double _y, double _z) {
    x = _x;
    y = _y;
    z = _z;
  }
  inline void zero() { x = y = z = 0.0; }
  inline double dot(const TMRPoint& p) const {
    return x * p.x + y * p.y + z * p.z;
  }
  inline double dot(const TMRPoint* p) const {
    return x * p->x + y * p->y + z * p->z;
  }

  double x, y, z;
};

/**
  Data type for a single STL triangle
*/
class TMR_STLTriangle {
 public:
  TMRPoint p[3];
};

/*
  The MPI TMROctant data type
*/
extern MPI_Datatype TMROctant_MPI_type;
extern MPI_Datatype TMRQuadrant_MPI_type;
extern MPI_Datatype TMRPoint_MPI_type;
extern MPI_Datatype TMRIndexWeight_MPI_type;
extern MPI_Datatype TMR_STLTriangle_MPI_type;

// Initialize and finalize the data type
void TMRInitialize();
int TMRIsInitialized();
void TMRFinalize();

#endif  // TMR_BASE_H
