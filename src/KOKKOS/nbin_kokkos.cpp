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

#include "nbin_kokkos.h"
#include "neighbor.h"
#include "atom_kokkos.h"
#include "group.h"
#include "domain.h"
#include "comm.h"
#include "update.h"
#include "error.h"
#include "atom_masks.h"

using namespace LAMMPS_NS;

#define SMALL 1.0e-6
#define CUT2BIN_RATIO 100

/* ---------------------------------------------------------------------- */

template<ExecutionSpace Space>
NBinKokkos<Space>::NBinKokkos(LAMMPS *lmp) : NBinStandard(lmp) {
  atoms_per_bin = 16;

  d_resize = typename AT::t_int_scalar("NeighborKokkosFunctor::resize");
#ifndef KOKKOS_USE_CUDA_UVM
  h_resize = Kokkos::create_mirror_view(d_resize);
#else
  h_resize = d_resize;
#endif
  h_resize() = 1;

  kokkos = 1;
}

/* ----------------------------------------------------------------------
   setup neighbor binning geometry
   bin numbering in each dimension is global:
     0 = 0.0 to binsize, 1 = binsize to 2*binsize, etc
     nbin-1,nbin,etc = bbox-binsize to bbox, bbox to bbox+binsize, etc
     -1,-2,etc = -binsize to 0.0, -2*binsize to -binsize, etc
   code will work for any binsize
     since next(xyz) and stencil extend as far as necessary
     binsize = 1/2 of cutoff is roughly optimal
   for orthogonal boxes:
     a dim must be filled exactly by integer # of bins
     in periodic, procs on both sides of PBC must see same bin boundary
     in non-periodic, coord2bin() still assumes this by use of nbin xyz
   for triclinic boxes:
     tilted simulation box cannot contain integer # of bins
     stencil & neigh list built differently to account for this
   mbinlo = lowest global bin any of my ghost atoms could fall into
   mbinhi = highest global bin any of my ghost atoms could fall into
   mbin = number of bins I need in a dimension
------------------------------------------------------------------------- */

template<ExecutionSpace Space>
void NBinKokkos<Space>::bin_atoms_setup(int nall)
{
  if (mbins > k_bins.d_view.extent(0)) {
    k_bins = DAT::tdual_int_2d("Neighbor::d_bins",mbins,atoms_per_bin);
    bins = DualViewHelper<Space>::view(k_bins);

    k_bincount = DAT::tdual_int_1d("Neighbor::d_bincount",mbins);
    bincount = DualViewHelper<Space>::view(k_bincount);
  }
  if (nall > k_atom2bin.d_view.extent(0)) {
    k_atom2bin = DAT::tdual_int_1d("Neighbor::d_atom2bin",nall);
    atom2bin = DualViewHelper<Space>::view(k_atom2bin);
  }
}

/* ----------------------------------------------------------------------
   bin owned and ghost atoms
------------------------------------------------------------------------- */

template<ExecutionSpace Space>
void NBinKokkos<Space>::bin_atoms()
{
  last_bin = update->ntimestep;

  DualViewHelper<Space>::sync(k_bins);
  DualViewHelper<Space>::sync(k_bincount);
  DualViewHelper<Space>::sync(k_atom2bin);

  h_resize() = 1;

  while(h_resize() > 0) {
    h_resize() = 0;
    deep_copy(d_resize, h_resize);

    MemsetZeroFunctor<DeviceType> f_zero;
    f_zero.ptr = (void*) DualViewHelper<Space>::view(k_bincount).data();
    Kokkos::parallel_for(mbins, f_zero);

    atomKK->sync(Space,X_MASK);
    x = DualViewHelper<Space>::view(atomKK->k_x);

    bboxlo_[0] = bboxlo[0]; bboxlo_[1] = bboxlo[1]; bboxlo_[2] = bboxlo[2];
    bboxhi_[0] = bboxhi[0]; bboxhi_[1] = bboxhi[1]; bboxhi_[2] = bboxhi[2];

    NPairKokkosBinAtomsFunctor<Space> f(*this);

    Kokkos::parallel_for(atom->nlocal+atom->nghost, f);

    deep_copy(h_resize, d_resize);
    if(h_resize()) {

      atoms_per_bin += 16;
      k_bins = DAT::tdual_int_2d("bins", mbins, atoms_per_bin);
      bins = DualViewHelper<Space>::view(k_bins);
      c_bins = bins;
    }
  }

  DualViewHelper<Space>::modify(k_bins);
  DualViewHelper<Space>::modify(k_bincount);
  DualViewHelper<Space>::modify(k_atom2bin);
}

/* ---------------------------------------------------------------------- */

template<ExecutionSpace Space>
KOKKOS_INLINE_FUNCTION
void NBinKokkos<Space>::binatomsItem(const int &i) const
{
  const int ibin = coord2bin(x(i, 0), x(i, 1), x(i, 2));

  atom2bin(i) = ibin;
  const int ac = Kokkos::atomic_fetch_add(&bincount[ibin], (int)1);
  if(ac < bins.extent(1)) {
    bins(ibin, ac) = i;
  } else {
    d_resize() = 1;
  }
}

namespace LAMMPS_NS {
template class NBinKokkos<Device>;
template class NBinKokkos<Host>;
}
