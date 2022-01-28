/* -*- c++ -*- ----------------------------------------------------------
 *
 *                    *** Smooth Mach Dynamics ***
 *
 * This file is part of the MACHDYN package for LAMMPS.
 * Copyright (2014) Georg C. Ganzenmueller, georg.ganzenmueller@emi.fhg.de
 * Fraunhofer Ernst-Mach Institute for High-Speed Dynamics, EMI,
 * Eckerstrasse 4, D-79104 Freiburg i.Br, Germany.
 *
 * ----------------------------------------------------------------------- */

/* ----------------------------------------------------------------------
   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
   https://www.lammps.org/, Sandia National Laboratories
   Steve Plimpton, sjplimp@sandia.gov

   Copyright (2003) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under
   the GNU General Public License.

   See the README file in the top-level LAMMPS directory.
------------------------------------------------------------------------- */

#ifdef COMPUTE_CLASS
// clang-format off
ComputeStyle(smd/internal/energy,ComputeSMDInternalEnergy);
// clang-format on
#else

#ifndef LMP_COMPUTE_SMD_INTERNAL_ENERGY_H
#define LMP_COMPUTE_SMD_INTERNAL_ENERGY_H

#include "compute.h"

namespace LAMMPS_NS {

class ComputeSMDInternalEnergy : public Compute {
 public:
  ComputeSMDInternalEnergy(class LAMMPS *, int, char **);
  ~ComputeSMDInternalEnergy() override;
  void init() override;
  void compute_peratom() override;
  double memory_usage() override;

 private:
  int nmax;
  double *internal_energy_vector;
};

}    // namespace LAMMPS_NS

#endif
#endif
