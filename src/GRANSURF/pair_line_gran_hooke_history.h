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

#ifdef PAIR_CLASS

PairStyle(line/gran/hooke/history,PairLineGranHookeHistory)

#else

#ifndef LMP_PAIR_LINE_GRAN_HOOKE_HISTORY_H
#define LMP_PAIR_LINE_GRAN_HOOKE_HISTORY_H

#include "pair.h"
#include "fix_surface_local.h"

namespace LAMMPS_NS {

class PairLineGranHookeHistory : public Pair {
 public:
  PairLineGranHookeHistory(class LAMMPS *);
  virtual ~PairLineGranHookeHistory();
  virtual void compute(int, int);
  void settings(int, char **);
  void coeff(int, char **);
  void init_style();
  double init_one(int, int);
  void write_restart(FILE *);
  void read_restart(FILE *);
  void write_restart_settings(FILE *);
  void read_restart_settings(FILE *);
  void reset_dt();
  virtual int pack_forward_comm(int, int *, double *, int, int *);
  virtual void unpack_forward_comm(int, int, double *);
  double memory_usage();

 protected:
  double kn,kt,gamman,gammat,xmu;
  int dampflag,limit_damping;
  double dt;
  int freeze_group_bit;
  int history;

  int neighprev;
  double *onerad_dynamic,*onerad_frozen;
  double *maxrad_dynamic,*maxrad_frozen;

  int size_history;
  int surfmoveflag;

  class FixDummy *fix_dummy;
  class FixNeighHistory *fix_history;

  int emax;                // allocated size of endpt list
  double **endpts;         // current end pts of each line
                           // Nall x 4 array for local + ghost atoms

  // ptr to AtomVec for bonus line info

  class AtomVecLine *avec;

  // line connectivity info for owned and ghost lines

  class FixSurfaceLocal *fsl;              // ptr to surface/local fix
  FixSurfaceLocal::Connect2d *connect2d;   // ptr to connectivity info

  // storage of rigid body masses for use in granular interactions

  class Fix *fix_rigid;    // ptr to rigid body fix, NULL if none
  double *mass_rigid;      // rigid mass for owned+ghost atoms
  int nmax;                // allocated size of mass_rigid

  // local methods

  void allocate();
  void calculate_endpts();
  int overlap_sphere_line(int, int, double *, double *, double &);
  int endpt_neigh_check(int, int, int);
};

}

#endif
#endif

/* ERROR/WARNING messages:

E: Pair gran/line has inconsitent internal size

UNDOCUMENTED

E: Pair gran/line iteraction between 2 lines

UNDOCUMENTED

E: Pair gran/line iteraction between 2 spheres

UNDOCUMENTED

E: Illegal ... command

Self-explanatory.  Check the input script syntax and compare to the
documentation for the command.  You can use -echo screen as a
command-line option when running LAMMPS to see the offending line.

E: Incorrect args for pair coefficients

Self-explanatory.  Check the input script or data file.

E: Pair gran/line requires atom style line

UNDOCUMENTED

E: Pair gran/line requires ghost atoms store velocity

UNDOCUMENTED

E: Pair granular with shear history requires newton pair off

This is a current restriction of the implementation of pair
granular styles with history.

E: Could not find pair fix ID

UNDOCUMENTED

E: Pair gran/line found %g line end pts with more than 2 lines

UNDOCUMENTED

U: Pair granular requires atom style sphere

Self-explanatory.

U: Pair granular requires ghost atoms store velocity

Use the comm_modify vel yes command to enable this.

*/
