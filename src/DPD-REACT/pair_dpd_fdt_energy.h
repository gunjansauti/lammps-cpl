/* -*- c++ -*- ----------------------------------------------------------
   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
   https://www.lammps.org/, Sandia National Laboratories
   Steve Plimpton, sjplimp@sandia.gov

   Copyright (2003) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under
   the GNU General Public License.

   See the README file in the top-level LAMMPS directory.
------------------------------------------------------------------------- */

#ifdef PAIR_CLASS
// clang-format off
PairStyle(dpd/fdt/energy,PairDPDfdtEnergy);
// clang-format on
#else

#ifndef LMP_PAIR_DPD_FDT_ENERGY_H
#define LMP_PAIR_DPD_FDT_ENERGY_H

#include "pair.h"

namespace LAMMPS_NS {

class PairDPDfdtEnergy : public Pair {
 public:
  PairDPDfdtEnergy(class LAMMPS *);
  ~PairDPDfdtEnergy() override;
  void compute(int, int) override;
  void settings(int, char **) override;
  void coeff(int, char **) override;
  void init_style() override;
  double init_one(int, int) override;
  void write_restart(FILE *) override;
  void read_restart(FILE *) override;
  void write_restart_settings(FILE *) override;
  void read_restart_settings(FILE *) override;
  double single(int, int, int, int, double, double, double, double &) override;
  int pack_reverse_comm(int, int, double *) override;
  void unpack_reverse_comm(int, int *, double *) override;

  double **cut;
  double **a0;
  double **sigma, **kappa, **alpha;
  double *duCond, *duMech;

  int seed;
  class RanMars *random;

 protected:
  double cut_global;
  bool splitFDT_flag;
  bool a0_is_zero;

  virtual void allocate();
};

}    // namespace LAMMPS_NS

#endif
#endif

/* ERROR/WARNING messages:

E: Illegal ... command

Self-explanatory.  Check the input script syntax and compare to the
documentation for the command.  You can use -echo screen as a
command-line option when running LAMMPS to see the offending line.

E: Incorrect args for pair coefficients

Self-explanatory.  Check the input script or data file.

E: Pair dpd/fdt/energy requires ghost atoms store velocity

Use the communicate vel yes command to enable this.

E: Pair dpd/fdt/energy requires newton pair on

Self-explanatory.

E: All pair coeffs are not set

All pair coefficients must be set in the data file or by the
pair_coeff command before running a simulation.

*/
