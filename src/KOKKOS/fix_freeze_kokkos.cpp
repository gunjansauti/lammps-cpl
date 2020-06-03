/* ----------------------------------------------------------------------
   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
   http://lammps.sandia.gov, Sandia National Laboratories
   Steve Plimpton, sjplimp@sandia.gov

   Copyright (2003) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under
   the GNU General Public License.

   See the README file in the top-level LAMMPS directory.
------------------------------------------------------------------------- */

#include "fix_freeze_kokkos.h"
#include "atom_masks.h"
#include "atom_kokkos.h"

using namespace LAMMPS_NS;

/* ---------------------------------------------------------------------- */

template<ExecutionSpace Space>
FixFreezeKokkos<Space>::FixFreezeKokkos(LAMMPS *lmp, int narg, char **arg) :
  FixFreeze(lmp, narg, arg)
{
  kokkosable = 1;
  atomKK = (AtomKokkos *)atom;

  datamask_read = F_MASK | MASK_MASK;
  datamask_modify = F_MASK | TORQUE_MASK;
}

/* ---------------------------------------------------------------------- */

template<ExecutionSpace Space>
int FixFreezeKokkos<Space>::setmask()
{
  return FixFreeze::setmask();
}

/* ---------------------------------------------------------------------- */

template<ExecutionSpace Space>
void FixFreezeKokkos<Space>::init()
{
  FixFreeze::init();
}

/* ---------------------------------------------------------------------- */

template<ExecutionSpace Space>
void FixFreezeKokkos<Space>::setup(int vflag)
{
  FixFreeze::setup(vflag);
}

/* ---------------------------------------------------------------------- */

template<ExecutionSpace Space>
void FixFreezeKokkos<Space>::post_force(int vflag)
{
  atomKK->sync(execution_space,datamask_read);
  atomKK->modified(execution_space,datamask_modify);

  f = DualViewHelper<Space>::view(atomKK->k_f);
  torque = DualViewHelper<Space>::view(atomKK->k_torque);
  mask = DualViewHelper<Space>::view(atomKK->k_mask);

  int nlocal = atom->nlocal;
  if (igroup == atom->firstgroup) nlocal = atom->nfirst;

  force_flag = 0;
  copymode = 1;
  OriginalForce original;
  Kokkos::parallel_reduce(nlocal, *this, original);
  copymode = 0;

  foriginal[0] = original.values[0];
  foriginal[1] = original.values[1];
  foriginal[2] = original.values[2];
}

/* ---------------------------------------------------------------------- */

template<ExecutionSpace Space>
void FixFreezeKokkos<Space>::post_force_respa(int vflag, int ilevel, int iloop)
{
  post_force(vflag);
}

/* ----------------------------------------------------------------------
   return components of total force on fix group before force was changed
------------------------------------------------------------------------- */

template<ExecutionSpace Space>
KK_FLOAT FixFreezeKokkos<Space>::compute_vector(int n)
{
  return FixFreeze::compute_vector(n);
}

/* ---------------------------------------------------------------------- */

template<ExecutionSpace Space>
void FixFreezeKokkos<Space>::operator()(const int i, OriginalForce &original) const {
  if (mask[i] & groupbit) {
    original.values[0] += f(i,0);
    original.values[1] += f(i,1);
    original.values[2] += f(i,2);
    f(i,0) = 0.0;
    f(i,1) = 0.0;
    f(i,2) = 0.0;
    torque(i,0) = 0.0;
    torque(i,1) = 0.0;
    torque(i,2) = 0.0;
  }
}

namespace LAMMPS_NS {
template class FixFreezeKokkos<Device>;
template class FixFreezeKokkos<Host>;
}
