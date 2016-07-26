/* -*- c++ -*- ----------------------------------------------------------
   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
   http://lammps.sandia.gov, Sandia National Laboratories
   Steve Plimpton, sjplimp@sandia.gov

   Copyright (2003) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under
   the GNU General Public License.

   See the README file in the top-level LAMMPS directory.
------------------------------------------------------------------------- */

#ifdef NEIGH_PAIR_CLASS

NeighPairStyle(NEIGH_PAIR_HALF_BIN_NEWTON_SSA,NeighPairHalfBinNewtonSSA)

#else

#ifndef LMP_NEIGH_PAIR_HALF_BIN_NEWTON_SSA_H
#define LMP_NEIGH_PAIR_HALF_BIN_NEWTON_SSA_H

#include "neigh_pair.h"

namespace LAMMPS_NS {

class NeighPairHalfBinNewtonSSA : public NeighPair {
 public:
  NeighPairHalfBinNewtonSSA(class LAMMPS *);
  ~NeighPairHalfBinNewtonSSA() {}
  void build(class NeighList *);
};

}

#endif
#endif

/* ERROR/WARNING messages:

*/
