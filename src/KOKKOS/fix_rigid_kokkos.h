// clang-format off
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

#ifdef FIX_CLASS
// clang-format off
FixStyle(rigid/kk,FixRigidKokkos<LMPDeviceType>)
FixStyle(rigid/kk/device,FixRigidKokkos<LMPDeviceType>)
FixStyle(rigid/kk/host,FixRigidKokkos<LMPHostType>)
// clang-format off
#else

#ifndef LMP_FIX_RIGID_KOKKOS_H
#define LMP_FIX_RIGID_KOKKOS_H

#include "fix_rigid.h"
#include "kokkos_type.h"
#include "Kokkos_Random.hpp"

namespace LAMMPS_NS {

template <class DeviceType>
class FixRigidKokkos;

template <class DeviceType>
class FixRigidKokkos : public FixRigid {
 public:

  // These did not exist or at least I could not find them:
  typedef Kokkos::DualView<F_FLOAT*[4], Kokkos::LayoutRight, LMPDeviceType> tdual_quat_array;
  typedef Kokkos::DualView<F_FLOAT*[6], Kokkos::LayoutRight, LMPDeviceType> tdual_sum_array;
  typedef Kokkos::DualView<T_INT*[4], Kokkos::LayoutRight, LMPDeviceType> tdual_int4_array;


  FixRigidKokkos(class LAMMPS *, int, char **);
  virtual ~FixRigidKokkos();

  // virtual int setmask(); // Masks remain same
  void init();
  void setup(int);
  void initial_integrate(int);
  void post_force(int);
  void final_integrate();

  // pre_neighbor gets called explicitly during init. At this time, not all
  // kokkos-able arrays and stuff is set. We have to bypass this somehow.
  // No need for explicit setup_pre_neighbor, it only calls this method
  // which is virtual.
  void pre_neighbor();
  double compute_scalar();


  // void initial_integrate_respa(int, int, int);
  // void final_integrate_respa(int, int);
  // void write_restart_file(char *);
  // double compute_scalar();

  int dof(int);
  //void deform(int);
  //void enforce2d();
  //void reset_dt();
  //void zero_momentum();
  //void zero_rotation();

  void grow_arrays(int);
  void set_xv_kokkos(); // Original set_xv and set_v are also protected.
  void set_v_kokkos();
  void compute_forces_and_torques_kokkos();
  void image_shift_kokkos();
  void apply_langevin_thermostat_kokkos();

  template <int NEIGHFLAG>
  void v_tally(EV_FLOAT &ev, const int &i, double v_arr[6]) const;

  enum SYNC_MODIFY_FLAGS { HOST = 0, DEVICE = 1 };
  template <int space> void sync_all();
  template <int space> void modify_all();

 private:
  // We need Kokkos style containers for everything in the innner loops:
  DAT::tdual_x_array k_xcm;
  DAT::tdual_v_array k_vcm;
  DAT::tdual_f_array k_fcm;

  DAT::tdual_f_array k_tflag;
  DAT::tdual_f_array k_fflag;

  // Careful. This fix omega, angmom and torque are defined in fix_rigid.
  // They are not the same as those in atom_vec and atom_vec_kokkos!
  DAT::tdual_v_array k_omega;
  DAT::tdual_v_array k_angmom;
  DAT::tdual_f_array k_torque;
  DAT::tdual_x_array k_inertia;

  // k_quat has to be a special array because it is a quaternion!
  tdual_quat_array k_quat;
  tdual_int4_array k_remapflag;

  DAT::tdual_x_array k_ex_space, k_ey_space, k_ez_space;
  DAT::tdual_x_array k_displace;

  tdual_sum_array k_sum, k_all;
  tdual_sum_array k_langextra;

  DAT::tdual_int_1d k_body, k_eflags;
  DAT::tdual_imageint_1d k_xcmimage;
  DAT::tdual_imageint_1d k_imagebody;
  DAT::tdual_float_1d k_masstotal;
  DAT::tdual_int_1d k_nrigid;


  DAT::tdual_x_array k_orient;
  DAT::tdual_x_array k_dorient;
  DAT::tdual_float_1d k_virial;

  // Needed if we apply langvin forces:
  Kokkos::Random_XorShift64_Pool<DeviceType> rand_pool;
  typedef typename Kokkos::Random_XorShift64_Pool<DeviceType>::generator_type rand_type;

  bool bypass_pre_neighbor;

}; // class FixRigidKokkos

} // namespace LAMMPS_NS

#endif // LMP_FIX_RIGID_KOKKOS_H
#endif // FIX_CLASS
