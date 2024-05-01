/***************************************************************************
                             born_coul_long.cpp
                             -------------------
                           Trung Dac Nguyen (ORNL)

  Class for acceleration of the born/coul/long pair style.

 __________________________________________________________________________
    This file is part of the LAMMPS Accelerator Library (LAMMPS_AL)
 __________________________________________________________________________

    begin                :
    email                : nguyentd@ornl.gov
 ***************************************************************************/

#ifdef USE_OPENCL
#include "born_coul_long_cl.h"
#elif defined(USE_CUDART)
const char *born_coul_long=0;
#else
#include "born_coul_long_cubin.h"
#endif

#include "lal_born_coul_long.h"
#include <cassert>
namespace LAMMPS_AL {
#define BornCoulLongT BornCoulLong<numtyp, acctyp>

extern Device<PRECISION,ACC_PRECISION> device;

template <class numtyp, class acctyp>
BornCoulLongT::BornCoulLong() : BaseCharge<numtyp,acctyp>(),
                                    _allocated(false) {
}

template <class numtyp, class acctyp>
BornCoulLongT::~BornCoulLong() {
  clear();
}

template <class numtyp, class acctyp>
int BornCoulLongT::bytes_per_atom(const int max_nbors) const {
  return this->bytes_per_atom_atomic(max_nbors);
}

template <class numtyp, class acctyp>
int BornCoulLongT::init(const int ntypes, double **host_cutsq, double **host_rhoinv,
                       double **host_born1, double **host_born2, double **host_born3,
                       double **host_a, double **host_c, double **host_d,
                       double **host_sigma, double **host_offset,
                       double *host_special_lj, const int nlocal,
                       const int nall, const int max_nbors,
                       const int maxspecial, const double cell_size,
                       const double gpu_split, FILE *_screen,
                       double **host_cut_ljsq, const double host_cut_coulsq,
                       double *host_special_coul, const double qqrd2e,
                       const double g_ewald) {
  int success;
  success=this->init_atomic(nlocal,nall,max_nbors,maxspecial,cell_size,gpu_split,
                            _screen,born_coul_long,"k_born_coul_long");
  if (success!=0)
    return success;

  // If atom type constants fit in shared memory use fast kernel
  int lj_types=ntypes;
  shared_types=false;
  int max_shared_types=this->device->max_shared_types();
  if (lj_types<=max_shared_types && this->_block_size>=max_shared_types) {
    lj_types=max_shared_types;
    shared_types=true;
  }
  _lj_types=lj_types;

  // Allocate a host write buffer for data initialization
  UCL_H_Vec<numtyp> host_write(lj_types*lj_types*32,*(this->ucl_device),
                               UCL_WRITE_ONLY);

  for (int i=0; i<lj_types*lj_types; i++)
    host_write[i]=0.0;

  coeff1.alloc(lj_types*lj_types,*(this->ucl_device),UCL_READ_ONLY);
  this->atom->type_pack4(ntypes,lj_types,coeff1,host_write,host_rhoinv,
                         host_born1,host_born2,host_born3);

  coeff2.alloc(lj_types*lj_types,*(this->ucl_device),UCL_READ_ONLY);
  this->atom->type_pack4(ntypes,lj_types,coeff2,host_write,host_a,host_c,
                         host_d,host_offset);

  cutsq_sigma.alloc(lj_types*lj_types,*(this->ucl_device),UCL_READ_ONLY);
  this->atom->type_pack4(ntypes,lj_types,cutsq_sigma,host_write,host_cutsq,
             host_cut_ljsq,host_sigma);

  sp_lj.alloc(8,*(this->ucl_device),UCL_READ_ONLY);
  for (int i=0; i<4; i++) {
    host_write[i]=host_special_lj[i];
    host_write[i+4]=host_special_coul[i];
  }
  ucl_copy(sp_lj,host_write,8,false);

  _cut_coulsq=host_cut_coulsq;
  _qqrd2e=qqrd2e;
  _g_ewald=g_ewald;

  _allocated=true;
  this->_max_bytes=coeff1.row_bytes()+coeff2.row_bytes()
      +cutsq_sigma.row_bytes()+sp_lj.row_bytes();
  return 0;
}

template <class numtyp, class acctyp>
void BornCoulLongT::clear() {
  if (!_allocated)
    return;
  _allocated=false;

  coeff1.clear();
  coeff2.clear();
  cutsq_sigma.clear();
  sp_lj.clear();
  this->clear_atomic();
}

template <class numtyp, class acctyp>
double BornCoulLongT::host_memory_usage() const {
  return this->host_memory_usage_atomic()+sizeof(BornCoulLong<numtyp,acctyp>);
}

// ---------------------------------------------------------------------------
// Calculate energies, forces, and torques
// ---------------------------------------------------------------------------
template <class numtyp, class acctyp>
int BornCoulLongT::loop(const int eflag, const int vflag) {
  // Compute the block size and grid size to keep all cores busy
  int BX=this->block_size();
  int GX=static_cast<int>(ceil(static_cast<double>(this->ans->inum())/(BX/this->_threads_per_atom)));
  // Increase block size to reduce the block count
  if (GX > 65535) {
    int newBX = static_cast<int>(ceil(static_cast<double>(this->ans->inum()) / 65535.0));
    newBX = ((newBX + this->_threads_per_atom - 1) / this->_threads_per_atom) * this->_threads_per_atom;
    if (newBX <= 1024) {
        BX = newBX;
        GX = static_cast<int>(ceil(static_cast<double>(this->ans->inum()) / (BX / this->_threads_per_atom)));
    }
  };

  int ainum=this->ans->inum();
  int nbor_pitch=this->nbor->nbor_pitch();
  this->time_pair.start();
  if (shared_types) {
    this->k_pair_sel->set_size(GX,BX);
    this->k_pair_sel->run(&this->atom->x, &coeff1, &coeff2, &sp_lj,
                          &this->nbor->dev_nbor,
                          &this->_nbor_data->begin(),
                          &this->ans->force,
                          &this->ans->engv, &eflag, &vflag,
                          &ainum, &nbor_pitch, &this->atom->q,
                          &cutsq_sigma, &_cut_coulsq, &_qqrd2e,
                          &_g_ewald, &this->_threads_per_atom);
  } else {
    this->k_pair.set_size(GX,BX);
    this->k_pair.run(&this->atom->x, &coeff1, &coeff2, &_lj_types, &sp_lj,
                   &this->nbor->dev_nbor, &this->_nbor_data->begin(),
                   &this->ans->force, &this->ans->engv,
                   &eflag, &vflag, &ainum,
                   &nbor_pitch, &this->atom->q,
                   &cutsq_sigma, &_cut_coulsq,
                   &_qqrd2e, &_g_ewald, &this->_threads_per_atom);
  }
  this->time_pair.stop();
  return GX;
}

template class BornCoulLong<PRECISION,ACC_PRECISION>;
}
