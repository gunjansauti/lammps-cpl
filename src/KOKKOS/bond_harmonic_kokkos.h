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

#ifdef BOND_CLASS

BondStyle(harmonic/kk,BondHarmonicKokkos<Device>)
BondStyle(harmonic/kk/device,BondHarmonicKokkos<Device>)
BondStyle(harmonic/kk/host,BondHarmonicKokkos<Host>)

#else

#ifndef LMP_BOND_HARMONIC_KOKKOS_H
#define LMP_BOND_HARMONIC_KOKKOS_H

#include "bond_harmonic.h"
#include "kokkos_type.h"

namespace LAMMPS_NS {

template<int NEWTON_BOND, int EVFLAG>
struct TagBondHarmonicCompute{};

template<ExecutionSpace Space>
class BondHarmonicKokkos : public BondHarmonic {

 public:
  typedef typename GetDeviceType<Space>::value DeviceType;
  typedef DeviceType device_type;
  typedef EV_FLOAT value_type;

  BondHarmonicKokkos(class LAMMPS *);
  virtual ~BondHarmonicKokkos();
  void compute(int, int);
  void coeff(int, char **);
  void read_restart(FILE *);

  template<int NEWTON_BOND, int EVFLAG>
  KOKKOS_INLINE_FUNCTION
  void operator()(TagBondHarmonicCompute<NEWTON_BOND,EVFLAG>, const int&, EV_FLOAT&) const;

  template<int NEWTON_BOND, int EVFLAG>
  KOKKOS_INLINE_FUNCTION
  void operator()(TagBondHarmonicCompute<NEWTON_BOND,EVFLAG>, const int&) const;

  //template<int NEWTON_BOND>
  KOKKOS_INLINE_FUNCTION
  void ev_tally(EV_FLOAT &ev, const int &i, const int &j,
      const KK_FLOAT &ebond, const KK_FLOAT &fbond, const KK_FLOAT &delx,
                  const KK_FLOAT &dely, const KK_FLOAT &delz) const;

 protected:

  class NeighborKokkos *neighborKK;

  typedef ArrayTypes<Space> AT;
  typename AT::t_float_1d_3_randomread x;
  typename Kokkos::View<typename AT::t_float_1d_3::data_type,typename AT::t_float_1d_3::array_layout,typename KKDevice<DeviceType>::value,Kokkos::MemoryTraits<Kokkos::Atomic> > f;
  typename AT::t_int_2d bondlist;

  typedef typename KKDevice<DeviceType>::value KKDeviceType;
  DAT::tdual_float_1d k_eatom;
  DAT::tdual_float_1d_6 k_vatom;
  Kokkos::View<typename AT::t_float_1d::data_type,typename AT::t_float_1d::array_layout,KKDeviceType,Kokkos::MemoryTraits<Kokkos::Atomic> > d_eatom;
  Kokkos::View<typename AT::t_float_1d_6::data_type,typename AT::t_float_1d_6::array_layout,KKDeviceType,Kokkos::MemoryTraits<Kokkos::Atomic> > d_vatom;

  int nlocal,newton_bond;
  int eflag,vflag;

  typename AT::t_float_1d d_k;
  typename AT::t_float_1d d_r0;

  void allocate();
};

}

#endif
#endif

/* ERROR/WARNING messages:

*/
