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

#ifdef PAIR_CLASS

PairStyle(coul/dsf/kk,PairCoulDSFKokkos<Device>)
PairStyle(coul/dsf/kk/device,PairCoulDSFKokkos<Device>)
PairStyle(coul/dsf/kk/host,PairCoulDSFKokkos<Host>)

#else

#ifndef LMP_PAIR_COUL_DSF_KOKKOS_H
#define LMP_PAIR_COUL_DSF_KOKKOS_H

#include "pair_kokkos.h"
#include "pair_coul_dsf.h"
#include "neigh_list_kokkos.h"

namespace LAMMPS_NS {

template<int NEIGHFLAG, int NEWTON_PAIR, int EVFLAG>
struct TagPairCoulDSFKernelA{};

template<ExecutionSpace Space>
class PairCoulDSFKokkos : public PairCoulDSF {
 public:
  enum {EnabledNeighFlags=FULL|HALFTHREAD|HALF};
  enum {COUL_FLAG=1};
  typedef typename GetDeviceType<Space>::value DeviceType;
  typedef DeviceType device_type;
  typedef ArrayTypes<Space> AT;
  typedef EV_FLOAT value_type;
  PairCoulDSFKokkos(class LAMMPS *);
  ~PairCoulDSFKokkos();

  void compute(int, int);
  void init_style();

  template<int NEIGHFLAG, int NEWTON_PAIR, int EVFLAG>
  KOKKOS_INLINE_FUNCTION
  void operator()(TagPairCoulDSFKernelA<NEIGHFLAG,NEWTON_PAIR,EVFLAG>, const int&, EV_FLOAT&) const;

  template<int NEIGHFLAG, int NEWTON_PAIR, int EVFLAG>
  KOKKOS_INLINE_FUNCTION
  void operator()(TagPairCoulDSFKernelA<NEIGHFLAG,NEWTON_PAIR,EVFLAG>, const int&) const;

  template<int NEIGHFLAG, int NEWTON_PAIR>
  KOKKOS_INLINE_FUNCTION
  void ev_tally(EV_FLOAT &ev, const int &i, const int &j,
      const KK_FLOAT &epair, const KK_FLOAT &fpair, const KK_FLOAT &delx,
                  const KK_FLOAT &dely, const KK_FLOAT &delz) const;

  KOKKOS_INLINE_FUNCTION
  int sbmask(const int& j) const;

  typename AT::t_float_1d_3_lr_randomread x;
  typename AT::t_float_1d_3 f;
  typename AT::t_float_1d_randomread q;

 protected:

  DAT::tdual_float_1d k_eatom;
  DAT::tdual_float_1d_6 k_vatom;
  typename AT::t_float_1d d_eatom;
  typename AT::t_float_1d_6 d_vatom;


  int neighflag,newton_pair;
  int nlocal,nall,eflag,vflag;

  KK_FLOAT special_coul[4];
  KK_FLOAT qqrd2e;

  typename AT::t_neighbors_2d d_neighbors;
  typename AT::t_int_1d_randomread d_ilist;
  typename AT::t_int_1d_randomread d_numneigh;
  //NeighListKokkos<Space> k_list;

  friend void pair_virial_fdotr_compute<Space,PairCoulDSFKokkos>(PairCoulDSFKokkos*);

};

}

#endif
#endif

/* ERROR/WARNING messages:

E: Cannot use chosen neighbor list style with coul/dsf/kk

That style is not supported by Kokkos.

*/
