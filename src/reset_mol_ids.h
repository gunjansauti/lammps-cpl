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

#ifdef COMMAND_CLASS

CommandStyle(reset_mol_ids,ResetMolIDs)

#else

#ifndef LMP_RESET_MOL_IDS_H
#define LMP_RESET_MOL_IDS_H

#include "pointers.h"

namespace LAMMPS_NS {

class ResetMolIDs : protected Pointers {
 public:
  ResetMolIDs(class LAMMPS *);
  void command(int, char **);

 private:
   int me,nprocs;
   int l2l_nlocal; // local num of local-to-local molID equivs
   tagint **local_l2l; // list local to local equiv molIDs

   int nadd,npion,nmolID;
   int maxnglobal,maxnpion,maxmolID;
   tagint *global_pion,*pionIDs,*molIDlist;
   int *allnpion,*allstarts;
   void gather_molIDs();
};

}

#endif
#endif

/* ERROR/WARNING messages:

E: Reset_mol_ids command before simulation box is defined

UNDOCUMENTED

E: Can only use reset_mol_ids on molecular systems

UNDOCUMENTED

E: Illegal ... command

UNDOCUMENTED

E: Cannot use reset_mol_ids unless molecules have IDs

UNDOCUMENTED

E: Reset_mol_ids missing %d bond topology atom IDs - use comm_modify cutoff

UNDOCUMENTED

*/
