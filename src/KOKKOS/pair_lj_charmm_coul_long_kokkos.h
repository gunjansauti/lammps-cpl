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

PairStyle(lj/charmm/coul/long/kk,PairLJCharmmCoulLongKokkos<Device>)
PairStyle(lj/charmm/coul/long/kk/device,PairLJCharmmCoulLongKokkos<Device>)
PairStyle(lj/charmm/coul/long/kk/host,PairLJCharmmCoulLongKokkos<Host>)

#else

#ifndef LMP_PAIR_LJ_CHARMM_COUL_LONG_KOKKOS_H
#define LMP_PAIR_LJ_CHARMM_COUL_LONG_KOKKOS_H

#include "pair_kokkos.h"
#include "pair_lj_charmm_coul_long.h"
#include "neigh_list_kokkos.h"

namespace LAMMPS_NS {

template<ExecutionSpace Space>
class PairLJCharmmCoulLongKokkos : public PairLJCharmmCoulLong {
 public:
  enum {EnabledNeighFlags=FULL|HALFTHREAD|HALF};
  enum {COUL_FLAG=1};
  typedef typename GetDeviceType<Space>::value DeviceType;
  typedef DeviceType device_type;
  typedef ArrayTypes<Space> AT;
  PairLJCharmmCoulLongKokkos(class LAMMPS *);
  ~PairLJCharmmCoulLongKokkos();

  void compute(int, int);

  void init_tables(double cut_coul, double *cut_respa);
  void init_style();
  double init_one(int, int);

 protected:
  void cleanup_copy();

  template<bool STACKPARAMS, class Specialisation>
  KOKKOS_INLINE_FUNCTION
  KK_FLOAT compute_fpair(const KK_FLOAT& rsq, const int& i, const int&j,
                        const int& itype, const int& jtype) const;

  template<bool STACKPARAMS, class Specialisation>
  KOKKOS_INLINE_FUNCTION
  KK_FLOAT compute_fcoul(const KK_FLOAT& rsq, const int& i, const int&j, const int& itype,
                        const int& jtype, const KK_FLOAT& factor_coul, const KK_FLOAT& qtmp) const;

  template<bool STACKPARAMS, class Specialisation>
  KOKKOS_INLINE_FUNCTION
  KK_FLOAT compute_evdwl(const KK_FLOAT& rsq, const int& i, const int&j,
                        const int& itype, const int& jtype) const;

  template<bool STACKPARAMS, class Specialisation>
  KOKKOS_INLINE_FUNCTION
  KK_FLOAT compute_ecoul(const KK_FLOAT& rsq, const int& i, const int&j,
                        const int& itype, const int& jtype, const KK_FLOAT& factor_coul, const KK_FLOAT& qtmp) const;

  Kokkos::DualView<params_lj_coul**,Kokkos::LayoutRight,DeviceType> k_params;
  typename Kokkos::DualView<params_lj_coul**,
    Kokkos::LayoutRight,DeviceType>::t_dev_const_um params;
  // hardwired to space for 12 atom types
  params_lj_coul m_params[MAX_TYPES_STACKPARAMS+1][MAX_TYPES_STACKPARAMS+1];

  KK_FLOAT m_cutsq[MAX_TYPES_STACKPARAMS+1][MAX_TYPES_STACKPARAMS+1];
  KK_FLOAT m_cut_ljsq[MAX_TYPES_STACKPARAMS+1][MAX_TYPES_STACKPARAMS+1];
  KK_FLOAT m_cut_coulsq[MAX_TYPES_STACKPARAMS+1][MAX_TYPES_STACKPARAMS+1];
  typename AT::t_float_1d_3_lr_randomread x;
  typename AT::t_float_1d_3 f;
  typename AT::t_int_1d_randomread type;
  typename AT::t_float_1d_randomread q;

  DAT::tdual_float_1d k_eatom;
  DAT::tdual_float_1d_6 k_vatom;
  typename AT::t_float_1d d_eatom;
  typename AT::t_float_1d_6 d_vatom;

  int newton_pair;

  DAT::tdual_float_2d k_cutsq;
  typename AT::t_float_2d d_cutsq;
  DAT::tdual_float_2d k_cut_ljsq;
  typename AT::t_float_2d d_cut_ljsq;
  DAT::tdual_float_2d k_cut_coulsq;
  typename AT::t_float_2d d_cut_coulsq;

  typename AT::t_float_1d_randomread
    d_rtable, d_drtable, d_ftable, d_dftable,
    d_ctable, d_dctable, d_etable, d_detable;

  int neighflag;
  int nlocal,nall,eflag,vflag;

  KK_FLOAT special_coul[4];
  KK_FLOAT special_lj[4];
  KK_FLOAT qqrd2e;

  void allocate();

  friend class PairComputeFunctor<Space,PairLJCharmmCoulLongKokkos,FULL,true,CoulLongTable<1> >;
  friend class PairComputeFunctor<Space,PairLJCharmmCoulLongKokkos,HALF,true,CoulLongTable<1> >;
  friend class PairComputeFunctor<Space,PairLJCharmmCoulLongKokkos,HALFTHREAD,true,CoulLongTable<1> >;
  friend class PairComputeFunctor<Space,PairLJCharmmCoulLongKokkos,FULL,false,CoulLongTable<1> >;
  friend class PairComputeFunctor<Space,PairLJCharmmCoulLongKokkos,HALF,false,CoulLongTable<1> >;
  friend class PairComputeFunctor<Space,PairLJCharmmCoulLongKokkos,HALFTHREAD,false,CoulLongTable<1> >;
  friend EV_FLOAT pair_compute_neighlist<Space,PairLJCharmmCoulLongKokkos,FULL,CoulLongTable<1> >(PairLJCharmmCoulLongKokkos*,NeighListKokkos<Space>*);
  friend EV_FLOAT pair_compute_neighlist<Space,PairLJCharmmCoulLongKokkos,HALF,CoulLongTable<1> >(PairLJCharmmCoulLongKokkos*,NeighListKokkos<Space>*);
  friend EV_FLOAT pair_compute_neighlist<Space,PairLJCharmmCoulLongKokkos,HALFTHREAD,CoulLongTable<1> >(PairLJCharmmCoulLongKokkos*,NeighListKokkos<Space>*);
  friend EV_FLOAT pair_compute<Space,PairLJCharmmCoulLongKokkos,CoulLongTable<1> >(PairLJCharmmCoulLongKokkos*,
                                                            NeighListKokkos<Space>*);
  friend class PairComputeFunctor<Space,PairLJCharmmCoulLongKokkos,FULL,true,CoulLongTable<0> >;
  friend class PairComputeFunctor<Space,PairLJCharmmCoulLongKokkos,HALF,true,CoulLongTable<0> >;
  friend class PairComputeFunctor<Space,PairLJCharmmCoulLongKokkos,HALFTHREAD,true,CoulLongTable<0> >;
  friend class PairComputeFunctor<Space,PairLJCharmmCoulLongKokkos,FULL,false,CoulLongTable<0> >;
  friend class PairComputeFunctor<Space,PairLJCharmmCoulLongKokkos,HALF,false,CoulLongTable<0> >;
  friend class PairComputeFunctor<Space,PairLJCharmmCoulLongKokkos,HALFTHREAD,false,CoulLongTable<0> >;
  friend EV_FLOAT pair_compute_neighlist<Space,PairLJCharmmCoulLongKokkos,FULL,CoulLongTable<0> >(PairLJCharmmCoulLongKokkos*,NeighListKokkos<Space>*);
  friend EV_FLOAT pair_compute_neighlist<Space,PairLJCharmmCoulLongKokkos,HALF,CoulLongTable<0> >(PairLJCharmmCoulLongKokkos*,NeighListKokkos<Space>*);
  friend EV_FLOAT pair_compute_neighlist<Space,PairLJCharmmCoulLongKokkos,HALFTHREAD,CoulLongTable<0> >(PairLJCharmmCoulLongKokkos*,NeighListKokkos<Space>*);
  friend EV_FLOAT pair_compute<Space,PairLJCharmmCoulLongKokkos,CoulLongTable<0> >(PairLJCharmmCoulLongKokkos*,
                                                            NeighListKokkos<Space>*);
  friend void pair_virial_fdotr_compute<Space,PairLJCharmmCoulLongKokkos>(PairLJCharmmCoulLongKokkos*);

};

}

#endif
#endif

/* ERROR/WARNING messages:

E: Cannot use Kokkos pair style with rRESPA inner/middle

Self-explanatory.

E: Cannot use chosen neighbor list style with lj/charmm/coul/long/kk

Self-explanatory.

*/
