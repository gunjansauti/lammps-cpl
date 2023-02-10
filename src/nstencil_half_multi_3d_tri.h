/* -*- c++ -*- ----------------------------------------------------------
   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
   https://www.lammps.org/, Sandia National Laboratories
   LAMMPS development team: developers@lammps.org

   Copyright (2003) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under
   the GNU General Public License.

   See the README file in the top-level LAMMPS directory.
------------------------------------------------------------------------- */

#ifdef NSTENCIL_CLASS
// clang-format off
NStencilStyle(half/multi/3d/tri,
              NStencilHalfMulti3dTri, NS_HALF | NS_MULTI | NS_3D | NS_TRI);
// clang-format on
#else

#ifndef LMP_NSTENCIL_HALF_MULTI_3D_TRI_H
#define LMP_NSTENCIL_HALF_MULTI_3D_TRI_H

#include "nstencil.h"

namespace LAMMPS_NS {

class NStencilHalfMulti3dTri : public NStencil {
 public:
  NStencilHalfMulti3dTri(class LAMMPS *);
  void create() override;

 protected:
  void set_stencil_properties() override;
};

}    // namespace LAMMPS_NS

#endif
#endif
