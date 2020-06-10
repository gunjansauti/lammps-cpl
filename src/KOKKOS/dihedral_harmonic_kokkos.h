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

#ifdef DIHEDRAL_CLASS

DihedralStyle(harmonic/kk,DihedralHarmonicKokkos<Device>)
DihedralStyle(harmonic/kk/device,DihedralHarmonicKokkos<Device>)
DihedralStyle(harmonic/kk/host,DihedralHarmonicKokkos<Host>)

#else

#ifndef LMP_DIHEDRAL_HARMONIC_KOKKOS_H
#define LMP_DIHEDRAL_HARMONIC_KOKKOS_H

#include "dihedral_harmonic.h"
#include "kokkos_type.h"

namespace LAMMPS_NS {

template<int NEWTON_BOND, int EVFLAG>
struct TagDihedralHarmonicCompute{};

template<ExecutionSpace Space>
class DihedralHarmonicKokkos : public DihedralHarmonic {
 public:
  typedef typename GetDeviceType<Space>::value DeviceType;
  typedef DeviceType device_type;
  typedef EV_FLOAT value_type;
  typedef ArrayTypes<Space> AT;

  DihedralHarmonicKokkos(class LAMMPS *);
  virtual ~DihedralHarmonicKokkos();
  void compute(int, int);
  void coeff(int, char **);
  void read_restart(FILE *);

  template<int NEWTON_BOND, int EVFLAG>
  KOKKOS_INLINE_FUNCTION
  void operator()(TagDihedralHarmonicCompute<NEWTON_BOND,EVFLAG>, const int&, EV_FLOAT&) const;

  template<int NEWTON_BOND, int EVFLAG>
  KOKKOS_INLINE_FUNCTION
  void operator()(TagDihedralHarmonicCompute<NEWTON_BOND,EVFLAG>, const int&) const;

  //template<int NEWTON_BOND>
  KOKKOS_INLINE_FUNCTION
  void ev_tally(EV_FLOAT &ev, const int i1, const int i2, const int i3, const int i4,
                          KK_FLOAT &edihedral, KK_FLOAT *f1, KK_FLOAT *f3, KK_FLOAT *f4,
                          const KK_FLOAT &vb1x, const KK_FLOAT &vb1y, const KK_FLOAT &vb1z,
                          const KK_FLOAT &vb2x, const KK_FLOAT &vb2y, const KK_FLOAT &vb2z,
                          const KK_FLOAT &vb3x, const KK_FLOAT &vb3y, const KK_FLOAT &vb3z) const;

 protected:

  class NeighborKokkos *neighborKK;

  typename AT::t_float_1d_3_lr_randomread x;
  typename AT::t_float_1d_3 f;
  typename AT::t_int_2d dihedrallist;

  DAT::tdual_float_1d k_eatom;
  DAT::tdual_float_1d_6 k_vatom;
  typename AT::t_float_1d d_eatom;
  typename AT::t_float_1d_6 d_vatom;

  int nlocal,newton_bond;
  int eflag,vflag;

  DAT::tdual_int_scalar k_warning_flag;
  typename AT::t_int_scalar d_warning_flag;
  HAT::t_int_scalar h_warning_flag;

  DAT::tdual_float_1d k_k;
  DAT::tdual_float_1d k_cos_shift;
  DAT::tdual_float_1d k_sin_shift;
  DAT::tdual_int_1d k_sign;
  DAT::tdual_int_1d k_multiplicity;

  typename AT::t_float_1d d_k;
  typename AT::t_float_1d d_cos_shift;
  typename AT::t_float_1d d_sin_shift;
  typename AT::t_int_1d d_sign;
  typename AT::t_int_1d d_multiplicity;

  void allocate();
};

}

#endif
#endif

/* ERROR/WARNING messages:

*/
