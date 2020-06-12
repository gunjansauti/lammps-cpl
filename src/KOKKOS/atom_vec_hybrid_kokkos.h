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

#ifdef ATOM_CLASS

AtomStyle(hybrid/kk,AtomVecHybridKokkos)

#else

#ifndef LMP_ATOM_VEC_HYBRID_KOKKOS_H
#define LMP_ATOM_VEC_HYBRID_KOKKOS_H

#include "atom_vec_kokkos.h"
#include "kokkos_type.h"

namespace LAMMPS_NS {

class AtomVecHybridKokkos : public AtomVecKokkos {
 public:
  int nstyles;
  class AtomVec **styles;
  char **keywords;

  AtomVecHybridKokkos(class LAMMPS *);
  ~AtomVecHybridKokkos();
  void process_args(int, char **);
  void init();
  void grow(int);
  void grow_pointers();
  void copy(int, int, int);
  void clear_bonus();
  void force_clear(int, size_t);
  int pack_comm(int, int *, double *, int, int *);
  int pack_comm_vel(int, int *, double *, int, int *);
  void unpack_comm(int, int, double *);
  void unpack_comm_vel(int, int, double *);
  int pack_reverse(int, int, double *);
  void unpack_reverse(int, int *, double *);
  int pack_border(int, int *, double *, int, int *);
  int pack_border_vel(int, int *, double *, int, int *);
  void unpack_border(int, int, double *);
  void unpack_border_vel(int, int, double *);
  int pack_exchange(int, double *);
  int unpack_exchange(double *);
  int size_restart();
  int pack_restart(int, double *);
  int unpack_restart(double *);
  void create_atom(int, double *);
  void data_atom(double *, imageint, char **);
  int data_atom_hybrid(int, char **) {return 0;}
  void data_vel(int, char **);
  void pack_data(double **);
  void write_data(FILE *, int, double **);
  void pack_vel(double **);
  void write_vel(FILE *, int, double **);
  int property_atom(char *);
  void pack_property_atom(int, double *, int, int);
  bigint memory_usage();

  int pack_comm_kokkos(const int &n, const DAT::tdual_int_2d &k_sendlist,
                       const int & iswap,
                       const DAT::tdual_float_2d &buf,
                       const int &pbc_flag, const int pbc[]);
  void unpack_comm_kokkos(const int &n, const int &nfirst,
                          const DAT::tdual_float_2d &buf);
  int pack_comm_self(const int &n, const DAT::tdual_int_2d &list,
                     const int & iswap, const int nfirst,
                     const int &pbc_flag, const int pbc[]);
  int pack_border_kokkos(int n, DAT::tdual_int_2d k_sendlist,
                         DAT::tdual_double_2d_lr buf,int iswap,
                         int pbc_flag, int *pbc, ExecutionSpace space);
  void unpack_border_kokkos(const int &n, const int &nfirst,
                            const DAT::tdual_double_2d_lr &buf,
                            ExecutionSpace space);
  int pack_exchange_kokkos(const int &nsend,DAT::tdual_double_2d_lr &buf,
                           DAT::tdual_int_1d k_sendlist,
                           DAT::tdual_int_1d k_copylist,
                           ExecutionSpace space, int dim,
                           KK_FLOAT lo, KK_FLOAT hi);
  int unpack_exchange_kokkos(DAT::tdual_double_2d_lr &k_buf, int nrecv,
                             int nlocal, int dim, KK_FLOAT lo, KK_FLOAT hi,
                             ExecutionSpace space);

  void sync(ExecutionSpace space, unsigned int mask);
  void modified(ExecutionSpace space, unsigned int mask);
  void sync_overlapping_device(ExecutionSpace space, unsigned int mask);

 private:
  double **omega,**angmom;

  DAT::t_tagint_1d d_tag;
  DAT::t_float_1d_3 d_omega, d_angmom;
  HAT::t_float_1d_3 h_omega, h_angmom;

  int nallstyles;
  char **allstyles;

  void build_styles();
  int known_style(char *);
};

}

#endif
#endif

/* ERROR/WARNING messages:

E: Atom style hybrid cannot have hybrid as an argument

Self-explanatory.

E: Atom style hybrid cannot use same atom style twice

Self-explanatory.

E: Cannot mix molecular and molecule template atom styles

Self-explanatory.

E: Per-processor system is too big

The number of owned atoms plus ghost atoms on a single
processor must fit in 32-bit integer.

E: AtomVecHybridKokkos doesn't yet support threaded comm

UNDOCUMENTED

E: Invalid atom h_type in Atoms section of data file

UNDOCUMENTED

U: Invalid atom type in Atoms section of data file

Atom types must range from 1 to specified # of types.

*/
