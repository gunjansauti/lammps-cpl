/* -*- c++ -*- ----------------------------------------------------------
   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
   https://www.lammps.org/, Sandia National Laboratories
   LAMMPS Development team: developers@lammps.org

   Copyright (2003) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under
   the GNU General Public License.

   See the README file in the top-level LAMMPS directory.
------------------------------------------------------------------------- */

#ifdef PAIR_CLASS
// clang-format off
PairStyle(amoeba/gpu,PairAmoebaGPU);
// clang-format on
#else

#ifndef LMP_PAIR_AMOEBA_GPU_H
#define LMP_PAIR_AMOEBA_GPU_H

#include "pair_amoeba.h"

namespace LAMMPS_NS {

class PairAmoebaGPU : public PairAmoeba {
 public:
  PairAmoebaGPU(LAMMPS *lmp);
  ~PairAmoebaGPU();
  void init_style();
  double memory_usage();

  enum { GPU_FORCE, GPU_NEIGH, GPU_HYB_NEIGH };

  virtual void induce();

  //virtual void dispersion_real();
  virtual void multipole_real();
  virtual void udirect2b(double **, double **);
  virtual void umutual1(double **, double **);
  virtual void fphi_uind(double ****, double **, double **, double **);
  virtual void umutual2b(double **, double **);
  virtual void ufield0c(double **, double **);
  virtual void polar_real();
  virtual void polar_kspace();

 private:
  int gpu_mode;
  double cpu_time;
  void *tq_pinned;
  void *fieldp_pinned;
  bool tq_single;

  bool gpu_hal_ready;
  bool gpu_repulsion_ready;
  bool gpu_dispersion_real_ready;
  bool gpu_multipole_real_ready;
  bool gpu_udirect2b_ready;
  bool gpu_umutual1_ready;
  bool gpu_fphi_uind_ready;
  bool gpu_umutual2b_ready;
  bool gpu_polar_real_ready;

  void udirect2b_cpu();

  template<class numtyp>
  void compute_force_from_torque(const numtyp*, double**, double*);
};

}    // namespace LAMMPS_NS
#endif
#endif
