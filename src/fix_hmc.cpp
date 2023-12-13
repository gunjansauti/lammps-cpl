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

/* ----------------------------------------------------------------------
   Contributing authors: Charlles R. A. Abreu (abreu@eq.ufrj.br)
                         Ana J. Silveira (asilveira@plapiqui.edu.ar)
------------------------------------------------------------------------- */

#include "stdlib.h"
#include "string.h"
#include "math_extra.h"
#include "fix_hmc.h"
#include "atom.h"
#include "atom_vec.h"
#include "force.h"
#include "pair.h"
#include "bond.h"
#include "angle.h"
#include "dihedral.h"
#include "improper.h"
#include "kspace.h"
#include "domain.h"
#include "memory.h"
#include "error.h"
#include "comm.h"
#include "random_park.h"
#include "update.h"
#include "modify.h"
#include "fix.h"
#include "group.h"
#include "compute.h"
#include "output.h"
#include "neighbor.h"
#include "fix_rigid_nve_small.h"

using namespace LAMMPS_NS;
using namespace FixConst;

#define EPSILON 1.0e-7

enum{ ATOMS, VCM_OMEGA, XCM, ITENSOR, ROTATION, FORCE_TORQUE };

/* ---------------------------------------------------------------------- */

FixHMC::FixHMC(LAMMPS *lmp, int narg, char **arg) : Fix(lmp, narg, arg), random_equal(nullptr)
{
  // set defaults
  mom_flag = 1;
  if (narg < 7) error->all(FLERR, "Illegal fix hmc command");

  // Retrieve user-defined options:
  nevery = utils::numeric(FLERR, arg[3], false, lmp);         // Number of MD steps per MC step
  int seed = utils::numeric(FLERR, arg[4], false, lmp);       // Seed for random number generation
  double temp = utils::numeric(FLERR, arg[5], false, lmp);    // System temperature

  // Retrieve the molecular dynamics integrator type:
  mdi = arg[6];
  if (strcmp(mdi, "rigid") != 0 && strcmp(mdi, "flexible") != 0)
    error->all(FLERR, "Illegal fix hmc command");

  KT = force->boltz * temp / force->mvv2e;    // K*T in mvv units
  mbeta = -1.0 / (force->boltz * temp);       // -1/(K*T) in energy units

  // Check keywords:
  int iarg = 7;
  while (iarg < narg) {
    if (strcmp(arg[iarg], "mom") == 0) {
      if (iarg + 2 > narg) utils::missing_cmd_args(FLERR, "hmc mom", error);
      mom_flag = utils::logical(FLERR, arg[iarg + 1], false, lmp);
      iarg += 2;
    //} else if (strcmp(arg[iarg], "rot") == 0) {
    //  if (iarg + 2 > narg) utils::missing_cmd_args(FLERR, "hmc rot", error);
    //  rot_flag = utils::logical(FLERR, arg[iarg + 1], false, lmp);
    //  iarg += 2;
    } else
      error->all(FLERR, "Illegal fix hmc command");
  }

  // Initialize RNG with a different seed for each process:
  random = new RanPark(lmp, seed + comm->me);
  for (int i = 0; i < 100; i++) random->gaussian();
  random_equal = new RanPark(lmp, seed);

  // Perform initialization of per-atom arrays:
  eatom = nullptr;
  vatom = nullptr;
  stored_body = nullptr;
  stored_tag = nullptr;
  stored_bodyown = nullptr;
  stored_bodytag = nullptr;
  stored_atom2body = nullptr;
  stored_xcmimage = nullptr;
  stored_displace = nullptr;
  stored_eflags = nullptr;
  stored_orient = nullptr;
  stored_dorient = nullptr;

  // Register callback:
  atom->add_callback(0);

  // Initialize arrays and pointers for saving/restoration of states:
  setup_arrays_and_pointers();

  // Add new computes for global and per-atom properties:
  add_new_computes();

  // Define non-default fix attributes:
  global_freq = 1;
  scalar_flag = 1;
  extscalar = 0;
  vector_flag = 1;
  extvector = 0;
  size_vector = 4;
}

/* ---------------------------------------------------------------------- */

FixHMC::~FixHMC()
{
  atom->delete_callback(id,0);
  memory->destroy(eglobal);
  memory->destroy(vglobal);
  memory->destroy(eatom);
  memory->destroy(vatom);
  delete [] eglobalptr;
  delete [] vglobalptr;
  delete [] eatomptr;
  delete [] vatomptr;
  delete [] rev_comm;
  delete random_equal;
  modify->delete_compute("hmc_ke");
  modify->delete_compute("hmc_pe");
  modify->delete_compute("hmc_peatom");
  modify->delete_compute("hmc_press");
  modify->delete_compute("hmc_pressatom");

  memory->destroy(stored_tag);
  memory->destroy(stored_bodyown);
  memory->destroy(stored_bodytag);
  memory->destroy(stored_atom2body);
  memory->destroy(stored_xcmimage);
  memory->destroy(stored_displace);
  memory->destroy(stored_eflags);
  memory->destroy(stored_orient);
  memory->destroy(stored_dorient);

  for (Atom::PerAtom &stored_peratom_member : stored_peratom) {
    free(stored_peratom_member.address);
    free(stored_peratom_member.address_maxcols);
    free(stored_peratom_member.address_length);
  }
}

/* ---------------------------------------------------------------------- */

void FixHMC::post_constructor()
{
  char **newarg = new char*[4];
  newarg[0] = (char *) "hmc_mdi";
  newarg[1] = group->names[igroup];
  if (strcmp(mdi,"flexible") == 0) {
    newarg[2] = (char *) "nve";
    modify->add_fix(3,newarg);
  }
  else {
    newarg[2] = (char *) "rigid/nve/small";
    newarg[3] = (char *) "molecule";
    modify->add_fix(4,newarg);
  }
  class Fix* mdfix = modify->fix[modify->find_fix("hmc_mdi")];
  rigid_flag = mdfix->rigid_flag;
  if (rigid_flag)
    fix_rigid = (class FixRigidSmall*) mdfix;
}

/* ---------------------------------------------------------------------- */

template <typename T>
void FixHMC::store_peratom_member(Atom::PerAtom &stored_peratom_member,
                          Atom::PerAtom current_peratom_member, int nlocal)
{
  if (stored_peratom_member.name.compare(current_peratom_member.name)) {
        error->all(FLERR, "fix hmc tried to store incorrect peratom data");
  }
  int cols;
  // free old memory if stored_peratom_member isn't a copy of current_peratom_member
  if (stored_peratom_member.address != current_peratom_member.address) {
    free(stored_peratom_member.address);
    stored_peratom_member.address = nullptr;
  }
  if (stored_peratom_member.address_maxcols != current_peratom_member.address_maxcols) {
    free(stored_peratom_member.address_maxcols);
    stored_peratom_member.address_maxcols = nullptr;
  }
  // peratom scalers
  if (current_peratom_member.cols == 0) {
    if (*(T **) current_peratom_member.address != nullptr) {
      stored_peratom_member.address = malloc(sizeof(T) * nlocal);
      memcpy(stored_peratom_member.address, *(T **) current_peratom_member.address,
             nlocal * sizeof(T));
    } else {
      stored_peratom_member.address = nullptr;
    }
  } else {
    // peratom vectors
    if (current_peratom_member.cols < 0) {
      // variable column case
      cols = *(current_peratom_member.address_maxcols);
      stored_peratom_member.address_maxcols = (int *) malloc(sizeof(int));
      *(stored_peratom_member.address_maxcols) = *(current_peratom_member.address_maxcols);
    } else {
      // non-variable column case
      cols = current_peratom_member.cols;
    }
    if (*(T ***) current_peratom_member.address != nullptr) {
      stored_peratom_member.address = malloc(sizeof(T) * nlocal * cols);
      for (int i = 0; i < nlocal; i++) {
        memcpy((T *) stored_peratom_member.address + i * cols,
               (**(T ***) current_peratom_member.address) + i * cols, sizeof(T) * cols);
      }
    } else {
      stored_peratom_member.address = nullptr;
    }
  }
  stored_peratom_member.cols = current_peratom_member.cols;
  stored_peratom_member.collength = current_peratom_member.collength;
  stored_peratom_member.address_length = nullptr;
}


template <typename T>
void FixHMC::restore_peratom_member(Atom::PerAtom stored_peratom_member,
                          Atom::PerAtom &current_peratom_member, int nlocal)
{
  if (stored_peratom_member.name.compare(current_peratom_member.name)) {
    error->all(FLERR, "fix hmc tried to store incorrect peratom data");
  }
  if (stored_peratom_member.address == nullptr) return;
  int cols;
  // peratom scalers
  if (stored_peratom_member.cols == 0) {
    if (*(T **) current_peratom_member.address != nullptr) {
      memcpy(*(T **) current_peratom_member.address, stored_peratom_member.address,
             nlocal * sizeof(T));
    }
  } else {
    // peratom vectors
    if (stored_peratom_member.cols < 0) {
      // variable column case
      cols = *(stored_peratom_member.address_maxcols);
      *(current_peratom_member.address_maxcols) = *(stored_peratom_member.address_maxcols);
    } else {
      // non-variable column case
      cols = stored_peratom_member.cols;
    }
    if (*(T ***) current_peratom_member.address != nullptr) {
      for (int i = 0; i < nlocal; i++) {
        memcpy((**(T ***) current_peratom_member.address) + i * cols,
               (T *) stored_peratom_member.address + i * cols, sizeof(T) * cols);
      }
    }
  }
  current_peratom_member.cols = stored_peratom_member.cols;
  current_peratom_member.collength = stored_peratom_member.collength;
}


void FixHMC::setup_arrays_and_pointers()
{
  int i, j, m;
  int pair_flag;
  int bond_flag;
  int angle_flag;
  int dihedral_flag;
  int improper_flag;
  int kspace_flag;

  // Per-atom scalar properties to be saved and restored:

  current_peratom = atom->peratom;
  stored_nmax = 0;  // initialize so the memory gets allocated on first save

  // Determine which energy contributions must be computed:
  ne = 0;
  if (force->pair) { pair_flag = 1; ne++; } else pair_flag = 0;
  if (force->bond) { bond_flag = 1; ne++; } else bond_flag = 0;
  if (force->angle) { angle_flag = 1; ne++; } else angle_flag = 0;
  if (force->dihedral) { dihedral_flag = 1; ne++; } else dihedral_flag = 0;
  if (force->improper) { improper_flag = 1; ne++; } else improper_flag = 0;
  if (force->kspace) { kspace_flag = 1; ne++; } else kspace_flag = 0;

  // Initialize arrays for managing global energy terms:
  neg = pair_flag ? ne + 1 : ne;
  memory->create(eglobal,neg,"fix_hmc:eglobal");
  eglobalptr = new double*[neg];
  m = 0;
  if (pair_flag) {
    eglobalptr[m++] = &force->pair->eng_vdwl;
    eglobalptr[m++] = &force->pair->eng_coul;
  }
  if (bond_flag) eglobalptr[m++] = &force->bond->energy;
  if (angle_flag) eglobalptr[m++] = &force->angle->energy;
  if (dihedral_flag) eglobalptr[m++] = &force->dihedral->energy;
  if (improper_flag) eglobalptr[m++] = &force->improper->energy;
  if (kspace_flag) eglobalptr[m++] = &force->kspace->energy;

  // Initialize arrays for managing global virial terms:
  nv = ne;
  for (j = 0; j < modify->nfix; j++)
    if (modify->fix[j]->virial_global_flag) nv++;
  memory->create(vglobal,nv,6,"fix_hmc:vglobal");
  vglobalptr = new double**[nv];
  for (m = 0; m < nv; m++) vglobalptr[m] = new double*[6];
  for (i = 0; i < 6; i++) {
    m = 0;
    if (pair_flag) vglobalptr[m++][i] = &force->pair->virial[i];
    if (bond_flag) vglobalptr[m++][i] = &force->bond->virial[i];
    if (angle_flag) vglobalptr[m++][i] = &force->angle->virial[i];
    if (dihedral_flag) vglobalptr[m++][i] = &force->dihedral->virial[i];
    if (improper_flag) vglobalptr[m++][i] = &force->improper->virial[i];
    if (kspace_flag) vglobalptr[m++][i] = &force->kspace->virial[i];
    for (j = 0; j < modify->nfix; j++)
      if (modify->fix[j]->virial_global_flag)
        vglobalptr[m++][i] = &modify->fix[j]->virial[i];
  }

  // Determine which per-atom energy terms require reverse communication:
  rev_comm = new int[nv];
  m = 0;
  if (pair_flag) rev_comm[m++] = force->newton;
  if (bond_flag) rev_comm[m++] = force->newton_bond;
  if (angle_flag) rev_comm[m++] = force->newton_bond;
  if (dihedral_flag) rev_comm[m++] = force->newton_bond;
  if (improper_flag) rev_comm[m++] = force->newton_bond;
  if (kspace_flag) rev_comm[m++] = force->kspace->tip4pflag;
  for (i = ne; i < nv; i++) rev_comm[m++] = 0;

  // Initialize array of pointers to manage per-atom energies:
  eatomptr = new double**[ne];
  m = 0;
  if (pair_flag) eatomptr[m++] = &force->pair->eatom;
  if (bond_flag) eatomptr[m++] = &force->bond->eatom;
  if (angle_flag) eatomptr[m++] = &force->angle->eatom;
  if (dihedral_flag) eatomptr[m++] = &force->dihedral->eatom;
  if (improper_flag) eatomptr[m++] = &force->improper->eatom;
  if (kspace_flag) eatomptr[m++] = &force->kspace->eatom;

  // Initialize array of pointers to manage per-atom virials:
  vatomptr = new double***[nv];
  m = 0;
  if (pair_flag) vatomptr[m++] = &force->pair->vatom;
  if (bond_flag) vatomptr[m++] = &force->bond->vatom;
  if (angle_flag) vatomptr[m++] = &force->angle->vatom;
  if (dihedral_flag) vatomptr[m++] = &force->dihedral->vatom;
  if (improper_flag) vatomptr[m++] = &force->improper->vatom;
  if (kspace_flag) vatomptr[m++] = &force->kspace->vatom;
  for (i = 0; i < modify->nfix; i++)
    if (modify->fix[i]->virial_peratom_flag)
      vatomptr[m++] = &modify->fix[i]->vatom;

  // Determine the maximum and the actual numbers of per-atom variables in reverse
  // communications:
  // Note: fix-related virials do not communicate (thus 'ne' used instead of 'nv')
  comm_reverse = 0;
  ncommrev = 0;
  for (m = 0; m < ne; m++)
    if (rev_comm[m]) {
      comm_reverse += 7;  // 1 energy + 6 virials
      if (peatom_flag) ncommrev++;
      if (pressatom_flag) ncommrev += 6;
    }

  // Determine maximum number of per-atom variables in forward and reverse
  // communications when dealing with rigid bodies:
  if (rigid_flag) {
    comm_reverse = MAX(comm_reverse,6);
    comm_forward = 12;
  }
}

/* ---------------------------------------------------------------------- */

void FixHMC::add_new_computes()
{
  char **newarg = new char*[5];

  // Add all new computes for group "all":
  newarg[1] = (char *) "all";

  // Kinetic energy:
  newarg[0] = (char *) "hmc_ke";
  newarg[2] = (char *) "ke";
  modify->add_compute(3,newarg);
  ke = modify->compute[modify->ncompute-1];

  // Potential energy:
  newarg[0] = (char *) "hmc_pe";
  newarg[2] = (char *) "pe";
  modify->add_compute(3,newarg);
  pe = modify->compute[modify->ncompute-1];

  // Per-atom potential energy:
  newarg[0] = (char *) "hmc_peatom";
  newarg[2] = (char *) "pe/atom";
  modify->add_compute(3,newarg);
  peatom = modify->compute[modify->ncompute-1];

  // System pressure:
  newarg[0] = (char *) "hmc_press";
  newarg[2] = (char *) "pressure";
  newarg[3] = (char *) "NULL";
  newarg[4] = (char *) "virial";
  modify->add_compute(5,newarg);
  press = modify->compute[modify->ncompute-1];

  // Per-atom stress tensor:
  newarg[0] = (char *) "hmc_pressatom";
  newarg[2] = (char *) "stress/atom";
  modify->add_compute(5,newarg);
  pressatom = modify->compute[modify->ncompute-1];

  delete [] newarg;
}

/* ---------------------------------------------------------------------- */

int FixHMC::setmask()
{
  int mask = 0;
  mask |= END_OF_STEP;
  return mask;
}

/* ---------------------------------------------------------------------- */

void FixHMC::init()
{
  int ntimestep = update->ntimestep;

  // Check whether there is any fixes that change box size and/or shape:
  for (int i = 0; i < modify->nfix; i++) {
    if (modify->fix[i]->box_change)
      error->all(FLERR,"fix hmc is incompatible with fixes that change box size or shape");
  }

  // Check whether there are subsequent fixes with active virial_flag:
  int first = modify->find_fix(this->id) + 1;
  if (rigid_flag) first++;
  for (int i = first; i < modify->nfix; i++)
    if (modify->fix[i]->virial_peratom_flag || modify->fix[i]->virial_global_flag) {
      if (comm->me == 0)
        printf("Fix %s defined after fix hmc.\n",modify->fix[i]->style);
      error->all(FLERR,"fix hmc cannot precede fixes that modify the system pressure");
    }

  // Look for computes with active peatomflag, press_flag, or pressatomflag:
  peatom_flag = 0;
  press_flag = 0;
  pressatom_flag = 0;
  for (int i = 0; i < modify->ncompute; i++)
    if (strncmp(modify->compute[i]->id,"hmc_",4) != 0) {
      peatom_flag = peatom_flag | modify->compute[i]->peatomflag;
      press_flag = press_flag | modify->compute[i]->pressflag;
      pressatom_flag = pressatom_flag | modify->compute[i]->pressatomflag;
    }

  // Count per-atom properties to be exchanged:
  nvalues = 0;
  if (peatom_flag) nvalues += ne;
  if (pressatom_flag) nvalues += 6*nv;

  // (Re)allocate array of per-atom properties:
  grow_arrays(atom->nmax);

  // Activate potential energy and other necessary calculations at setup:
  pe->addstep(ntimestep);
  if (peatom_flag) peatom->addstep(ntimestep);
  if (press_flag) press->addstep(ntimestep);
  if (pressatom_flag) pressatom->addstep(ntimestep);
}

/* ----------------------------------------------------------------------
   Initialize MC move, save current state, and activate computes
------------------------------------------------------------------------- */

void FixHMC::setup(int vflag)
{

  // Compute properties of the initial state:
  nattempts = 0;
  naccepts = 0;
  DeltaPE = 0;
  DeltaKE = 0;
  if (rigid_flag) {
    rigid_body_random_velocities();
  }
  else {
    random_velocities();
  }

  update->eflag_global = update->ntimestep;
  PE = pe->compute_scalar();
  KE = ke->compute_scalar();
  save_current_state();

  // Activate potential energy and other necessary calculations:
  int nextstep = update->ntimestep + nevery;
  pe->addstep(nextstep);
  if (peatom_flag) peatom->addstep(nextstep);
  if (press_flag) press->addstep(nextstep);
  if (pressatom_flag) pressatom->addstep(nextstep);
}

/* ----------------------------------------------------------------------
   Apply the Metropolis acceptance criterion
   Restore saved system state if move is rejected
   Activate computes for the next MC step
------------------------------------------------------------------------- */

void FixHMC::end_of_step()
{
  nattempts++;

  // Compute potential and kinetic energy variations:
  update->eflag_global = update->ntimestep;
  double newPE = pe->compute_scalar();
  double newKE = ke->compute_scalar();
  DeltaPE = newPE - PE;
  DeltaKE = newKE - KE;

  // Apply the Metropolis criterion:
  double DeltaE = DeltaPE + DeltaKE;
  int accept = DeltaE < 0.0;
  if (~accept) {
    accept = random_equal->uniform() <= exp(mbeta*DeltaE);
    MPI_Bcast(&accept,1,MPI_INT,0,world);
  }
  if (accept) {
    // Update potential energy and save the current state:
    naccepts++;
    PE = newPE;
    save_current_state();
  }
  else {
    // Restore saved state and enforce check_distance/reneighboring in the next step:
    restore_saved_state();
    neighbor->ago = (neighbor->delay/neighbor->every + 1)*neighbor->every;
  }

  // Choose new velocities and compute kinetic energy:
  if (~accept) {
    if (rigid_flag)
      rigid_body_random_velocities();
    else
      random_velocities();
    KE = ke->compute_scalar();
  }

  // Activate potential energy and other necessary calculations:
  int nextstep = update->ntimestep + nevery;
  if (nextstep <= update->laststep) {
    pe->addstep(nextstep);
    if (peatom_flag) peatom->addstep(nextstep);
    if (press_flag) press->addstep(nextstep);
    if (pressatom_flag) pressatom->addstep(nextstep);
  }
}

/* ----------------------------------------------------------------------
   Return the acceptance fraction of proposed MC moves
------------------------------------------------------------------------- */

double FixHMC::compute_scalar()
{
  double acc_frac = naccepts;
  acc_frac /= MAX(1,nattempts);
  return acc_frac;
}

/* ----------------------------------------------------------------------
   Return the acceptance fraction of proposed MC moves, or
   the total energy variation of the last proposed MC move, or
   the mean-square atom displacement in the last proposed MC move
------------------------------------------------------------------------- */

double FixHMC::compute_vector(int item)
{
  int n = item + 1;
  if (n == 1) {
    double acc_frac = naccepts;
    acc_frac /= MAX(1,nattempts);
    return acc_frac;
  }
  else if (n == 2)
    return DeltaPE;
  else if (n == 3)
    return DeltaKE;
  else if (n == 4)
    return DeltaPE + DeltaKE;
  else
    return 0.0;
}

/* ----------------------------------------------------------------------
   Save the system state for eventual restoration if move is rejected
------------------------------------------------------------------------- */
void FixHMC::save_current_state()
{
  int i, m, n;
  int nlocal = atom->nlocal;
  int ntotal = nlocal + atom->nghost;
  int nmax = atom->nmax;
  double *scalar, **vector, *energy, **stress;

  if (nmax > stored_nmax) {
    // reallocate tag array
    stored_nmax = nmax;
    memory->destroy(stored_tag);
    stored_tag = memory->create(stored_tag, stored_nmax, "hmc:stored_tag");

    // reallocate body peratom data
    if (rigid_flag) {
      memory->destroy(stored_bodyown);
      memory->destroy(stored_bodytag);
      memory->destroy(stored_atom2body);
      memory->destroy(stored_xcmimage);
      memory->destroy(stored_displace);
      memory->destroy(stored_eflags);
      memory->destroy(stored_orient);
      memory->destroy(stored_dorient);
      stored_bodyown = memory->create(stored_bodyown, stored_nmax, "hmc:stored_bodyown");
      stored_bodytag = memory->create(stored_bodytag, stored_nmax, "hmc:stored_bodytag");
      stored_atom2body = memory->create(stored_atom2body, stored_nmax, "hmc:stored_atom2body");
      stored_xcmimage = memory->create(stored_xcmimage, stored_nmax, "hmc:stored_xcmimage");
      stored_displace = memory->create(stored_displace, stored_nmax, 3, "hmc:stored_displace");
      if (fix_rigid->extended) {
        stored_eflags = memory->create(stored_eflags, nmax, "hmc:stored_eflags");
        if (fix_rigid->orientflag)
          stored_orient =
              memory->create(stored_orient, nmax, fix_rigid->orientflag, "hmc:stored_orient");
        if (fix_rigid->dorientflag)
          stored_dorient = memory->create(stored_dorient, nmax, 3, "hmc:stored_dorient");
      }
    }
  }

  // store tag array
  memcpy(stored_tag, atom->tag, ntotal * sizeof(tagint));

  // store body peratom data
  if (rigid_flag) { 
    memcpy(stored_bodyown, fix_rigid->bodyown, ntotal * sizeof(int));
    memcpy(stored_bodytag, fix_rigid->bodytag, ntotal * sizeof(tagint));
    memcpy(stored_atom2body, fix_rigid->atom2body, ntotal * sizeof(int));
    memcpy(stored_xcmimage, fix_rigid->xcmimage, ntotal * sizeof(imageint));
    for (int i = 0; i < ntotal; i++)
      memcpy(stored_displace[i], fix_rigid->displace[i], 3 * sizeof(double));
    if (fix_rigid->extended) {
      memcpy(stored_eflags, fix_rigid->eflags, ntotal * sizeof(int));
      if (fix_rigid->orientflag)
        for (int i = 0; i < ntotal; i++)
          memcpy(stored_orient[i], fix_rigid->orient[i], fix_rigid->orientflag * sizeof(double));
      if (fix_rigid->dorientflag)
        for (int i = 0; i < ntotal; i++)
          memcpy(stored_dorient[i], fix_rigid->dorient[i], 3 * sizeof(double));
    }
  }

  // clear peratom data and store a new struct
  for (Atom::PerAtom &stored_peratom_member : stored_peratom) {
    free(stored_peratom_member.address);
    free(stored_peratom_member.address_maxcols);
  }
  stored_peratom.clear();
  for (Atom::PerAtom &current_peratom_member : current_peratom) {
    Atom::PerAtom stored_peratom_member = current_peratom_member;
    if (current_peratom_member.address != nullptr) {
      switch (current_peratom_member.datatype) {
        case (Atom::INT):
          store_peratom_member<int>(stored_peratom_member, current_peratom_member, ntotal);
          break;
        case (Atom::DOUBLE):
          store_peratom_member<double>(stored_peratom_member, current_peratom_member, ntotal);
          break;
        case (Atom::BIGINT):
          store_peratom_member<bigint>(stored_peratom_member, current_peratom_member, ntotal);
          break;
      }
    }
    stored_peratom.push_back(stored_peratom_member);
  }

  // store totals
  stored_ntotal = ntotal;
  stored_nlocal = nlocal;
  stored_nmax = nmax;
  stored_nghost = atom->nghost;
  stored_nbonds = atom->nbonds;
  stored_nangles = atom->nangles;
  stored_ndihedrals = atom->ndihedrals;
  stored_nimpropers = atom->nimpropers;

  // store bodies
  if (rigid_flag) {
    stored_nlocal_body = fix_rigid->nlocal_body;
    stored_nghost_body = fix_rigid->nghost_body;
    stored_ntotal_body = stored_nlocal_body + stored_nghost_body;
    delete stored_body;
    stored_body = new FixRigidSmall::Body[stored_ntotal_body];
    for (int i = 0; i < stored_ntotal_body; i++) stored_body[i] = fix_rigid->body[i];
  }

  // Save global energy terms:
  for (m = 0; m < neg; m++)
    eglobal[m] = *eglobalptr[m];

  // Save global virial terms:
  if (press_flag)
    for (m = 0; m < nv; m++)
      memcpy( vglobal[m], *vglobalptr[m], six );

  // Perform reverse communication to incorporate ghost atoms info:
  if (comm_reverse && (peatom_flag || pressatom_flag)) {
    comm_flag = ATOMS;
    comm->reverse_comm(this, ncommrev);
  }
}

/* ----------------------------------------------------------------------
   Restore the system state saved at the beginning of the MC step
------------------------------------------------------------------------- */

void FixHMC::restore_saved_state()
{
  int i, m;
  int nlocal = atom->nlocal;
  int ntotal = nlocal + atom->nghost;
  double **x = atom->x;
  double *scalar, **vector, *energy, **stress;

  int map_cleared = false;
  
  // clear the atom map since we will be messing all that up
  if (atom->map_style != Atom::MAP_NONE) {
    atom->map_clear();
    map_cleared = true;
  }

  // restore tag and peratom body data
  memcpy(atom->tag, stored_tag, stored_ntotal * sizeof(tagint));
  if (rigid_flag) {
    memcpy(fix_rigid->bodyown, stored_bodyown, ntotal * sizeof(int));
    memcpy(fix_rigid->bodytag, stored_bodytag, ntotal * sizeof(tagint));
    memcpy(fix_rigid->atom2body, stored_atom2body, ntotal * sizeof(int));
    memcpy(fix_rigid->xcmimage, stored_xcmimage, ntotal * sizeof(imageint));
    for (int i = 0; i < ntotal; i++)
      memcpy(fix_rigid->displace[i], stored_displace[i], 3 * sizeof(double));
    if (fix_rigid->extended) {
      memcpy(fix_rigid->eflags, stored_eflags, ntotal * sizeof(int));
      if (fix_rigid->orientflag)
        for (int i = 0; i < ntotal; i++)
          memcpy(fix_rigid->orient[i], stored_orient[i], fix_rigid->orientflag * sizeof(double));
      if (fix_rigid->dorientflag)
        for (int i = 0; i < ntotal; i++)
          memcpy(fix_rigid->dorient[i], stored_dorient[i], 3 * sizeof(double));
    }
  }

  if (stored_ntotal > atom->nlocal + atom->nghost){
    atom->avec->grow(stored_ntotal);
  }

  // restore counts
  atom->nlocal = stored_nlocal;
  atom->nghost = stored_nghost;
  atom->nbonds = stored_nbonds;
  atom->nangles = stored_nangles;
  atom->ndihedrals = stored_ndihedrals;
  atom->nimpropers = stored_nimpropers;

  // restore peratom data
  for (Atom::PerAtom &stored_peratom_member : stored_peratom) {
    for (Atom::PerAtom &current_peratom_member : current_peratom) {
      if (stored_peratom_member.name.compare(current_peratom_member.name)) {
        continue;
      } else {
        switch (current_peratom_member.datatype) {
          case (Atom::INT):
            restore_peratom_member<int>(stored_peratom_member, current_peratom_member, ntotal);
            break;
          case (Atom::DOUBLE):
            restore_peratom_member<double>(stored_peratom_member, current_peratom_member,
                                         ntotal);
            break;
          case (Atom::BIGINT):
            restore_peratom_member<bigint>(stored_peratom_member, current_peratom_member,
                                         ntotal);
            break;
        }
        break;
      }
    }
  }

  // restore bodies
  if (rigid_flag) {
    fix_rigid->nlocal_body = stored_nlocal_body;
    fix_rigid->nghost_body = stored_nghost_body;
    for (int i = 0; i < stored_ntotal_body; i++) fix_rigid->body[i] = stored_body[i];
  }

  // reinit atom_map
  if (map_cleared) {
    atom->map_init();
    atom->map_set();
  }

  // Restore global energy terms:
  for (i = 0; i < neg; i++)
    *eglobalptr[i] = eglobal[i];

  // Restore global virial terms:
  if (press_flag)
    for (i = 0; i < nv; i++)
      memcpy( *vglobalptr[i], vglobal[i], six );
}

/* ----------------------------------------------------------------------
   Randomly choose velocities from a Maxwell-Boltzmann distribution
------------------------------------------------------------------------- */

void FixHMC::random_velocities()
{
  double **v = atom->v;
  int *type = atom->type;
  int *mask = atom->mask;

  double stdev;
  int nlocal, dimension;
  
  double *rmass = atom->rmass;
  double *mass = atom->mass;
  nlocal = atom->nlocal;
  dimension = domain->dimension;
  if (igroup == atom->firstgroup) nlocal = atom->nfirst;
  for (int i = 0; i < nlocal; i++)
    if (mask[i] & groupbit) {
      if (rmass) stdev = sqrt(KT/rmass[i]);
      else stdev = sqrt(KT/mass[type[i]]);
      for (int j = 0; j < dimension; j++)
        v[i][j] = stdev*random->gaussian();
    }
  if (mom_flag) {
    double vcm[3];
    group->vcm(igroup, group->mass(igroup), vcm);
    for (int i = 0; i < nlocal; i++)
      if (mask[i] & groupbit) {
        for (int j = 0; j < dimension; j++)
        v[i][j] -= vcm[j];
      }
  }
}

/* ----------------------------------------------------------------------
  
------------------------------------------------------------------------- */

void FixHMC::rigid_body_random_velocities()
{
  FixRigidSmall::Body *body = fix_rigid->body;
  int nlocal = fix_rigid->nlocal_body;
  int ntotal = nlocal + fix_rigid->nghost_body;
  int *mask = atom->mask;

  double stdev, bmass, wbody[3], mbody[3];
  double total_mass = 0;
  FixRigidSmall::Body *b;
  double vcm[] = {0.0, 0.0, 0.0};

  for (int ibody = 0; ibody < nlocal; ibody++) {
    b = &body[ibody];
    if (mask[b->ilocal] & groupbit) {
      bmass = b->mass;
      stdev = sqrt(KT/bmass);
      total_mass += bmass;
      for (int j = 0; j < 3; j++) {
        b->vcm[j] = stdev*random->gaussian();
        vcm[j] += b->vcm[j] * bmass;
        if (b->inertia[j] > 0.0)
          wbody[j] = sqrt(KT/b->inertia[j])*random->gaussian();
        else
          wbody[j] = 0.0;
      }
    }
    MathExtra::matvec(b->ex_space,b->ey_space,b->ez_space,wbody,b->omega);
  }

  if (mom_flag) {
    for (int j = 0; j < 3; j++) vcm[j] /= total_mass;
    for (int ibody = 0; ibody < nlocal; ibody++) {
      if (mask[b->ilocal] & groupbit) {
        b = &body[ibody];
        for (int j = 0; j < 3; j++) b->vcm[j] -= vcm[j];
      }
    }
  }

  // Forward communicate vcm and omega to ghost bodies:
  comm_flag = VCM_OMEGA;
  comm->forward_comm(this,6);

  // Compute angular momenta of rigid bodies:
  for (int ibody = 0; ibody < ntotal; ibody++) {
    b = &body[ibody];
    MathExtra::omega_to_angmom(b->omega,b->ex_space,b->ey_space,b->ez_space,
                               b->inertia,b->angmom);
    MathExtra::transpose_matvec(b->ex_space,b->ey_space,b->ez_space,
                                b->angmom,mbody);
    MathExtra::quatvec(b->quat,mbody,b->conjqm);
    b->conjqm[0] *= 2.0;
    b->conjqm[1] *= 2.0;
    b->conjqm[2] *= 2.0;
    b->conjqm[3] *= 2.0;
  }

  // Compute velocities of individual atoms:
  fix_rigid->set_v();
}

/* ----------------------------------------------------------------------
   Pack rigid body info for forward communication
------------------------------------------------------------------------- */

int FixHMC::pack_forward_comm(int n, int *list, double *buf, int /*pbc_flag*/, int * /*pbc*/)
{
  int *bodyown = fix_rigid->bodyown;
  FixRigidSmall::Body *body = fix_rigid->body;

  int i, m, ibody;
  FixRigidSmall::Body *b;

  m = 0;
  for (i = 0; i < n; i++) {
    ibody = bodyown[list[i]];
    if (ibody >= 0) {
      b = &body[ibody];
      if (comm_flag == VCM_OMEGA) {
        memcpy( &buf[m], b->vcm, three );
        memcpy( &buf[m+3], b->omega, three );
        m += 6;
      }
      else if (comm_flag == XCM) {
        memcpy( &buf[m], b->xcm, three );
        m += 3;
      }
      else if (comm_flag == ROTATION) {
        memcpy( &buf[m], b->ex_space, three );
        memcpy( &buf[m+3], b->ey_space, three );
        memcpy( &buf[m+6], b->ez_space, three );
        memcpy( &buf[m+9], b->quat, four );
        m += 12;
      }
    }
  }
  return m;
}

/* ----------------------------------------------------------------------
   Unpack rigid body info from forward communication
------------------------------------------------------------------------- */

void FixHMC::unpack_forward_comm(int n, int first, double *buf)
{
  int *bodyown = fix_rigid->bodyown;
  FixRigidSmall::Body *body = fix_rigid->body;

  int i, m, last;
  FixRigidSmall::Body *b;

  m = 0;
  last = first + n;
  for (i = first; i < last; i++)
    if (bodyown[i] >= 0) {
      b = &body[bodyown[i]];
      if (comm_flag == VCM_OMEGA) {
        memcpy( b->vcm, &buf[m], three );
        memcpy( b->omega, &buf[m+3], three );
        m += 6;
      }
    }
}

/* ----------------------------------------------------------------------
   Pack per-atom energies and/or virials or
   rigid body info for reverse communication
------------------------------------------------------------------------- */

int FixHMC::pack_reverse_comm(int n, int first, double *buf)
{
  int last = first + n;

  int i, k, m;

  m = 0;
  if (comm_flag == ATOMS) {
    for (i = first; i < last; i++)
      for (k = 0; k < ne; k++)
        if (rev_comm[k]) {
          if (peatom_flag)
            buf[m++] = eatom[k][i];
          if (pressatom_flag) {
            memcpy( &buf[m], vatom[k][i], six );
            m += 6;
          }
        }
  }
    return m;
  }

/* ----------------------------------------------------------------------
   Unpack per-atom energies and/or virials or
   rigid body info for reverse communication
------------------------------------------------------------------------- */

void FixHMC::unpack_reverse_comm(int n, int *list, double *buf)
{
  int i, j, k, m;

  m = 0;
  if (comm_flag == ATOMS) {
    for (j = 0; j < n; j++) {
      i = list[j];
      for (k = 0; k < ne; k++)
        if (rev_comm[k]) {
          if (peatom_flag)
            eatom[k][i] += buf[m++];
          if (pressatom_flag) 
            vatom[k][i][0] += buf[m++];
            vatom[k][i][1] += buf[m++];
            vatom[k][i][2] += buf[m++];
            vatom[k][i][3] += buf[m++];
            vatom[k][i][4] += buf[m++];
            vatom[k][i][5] += buf[m++];
        }
    }
  }
}

/* ----------------------------------------------------------------------
   allocate atom-based arrays
------------------------------------------------------------------------- */

void FixHMC::grow_arrays(int nmax)
{
  memory->grow(eatom,ne,nmax,"fix_hmc:eatom");
  memory->grow(vatom,nv,nmax,6,"fix_hmc:vatom");
}

/* ----------------------------------------------------------------------
   copy values within local atom-based arrays
------------------------------------------------------------------------- */

void FixHMC::copy_arrays(int i, int j, int delflag)
{
  int m;

  if (peatom_flag)
    for (m = 0; m < ne; m++)
      eatom[m][j] = eatom[m][i];

  if (pressatom_flag)
    for (m = 0; m < nv; m++)
      memcpy( vatom[m][j], vatom[m][i], six );
}

/* ----------------------------------------------------------------------
   pack values in local atom-based array for exchange with another proc
------------------------------------------------------------------------- */

int FixHMC::pack_exchange(int i, double *buf)
{
  int k, m = 0;

  if (peatom_flag)
    for (k = 0; k < ne; k++)
      buf[m++] = eatom[k][i];

  if (pressatom_flag)
    for (k = 0; k < nv; k++) {
      memcpy( &buf[m], vatom[k][i], six );
      m += 6;
    }

  return m;
}

/* ----------------------------------------------------------------------
   unpack values in local atom-based array from exchange with another proc
------------------------------------------------------------------------- */

int FixHMC::unpack_exchange(int i, double *buf)
{
  int k, m = 0;

  if (peatom_flag)
    for (k = 0; k < ne; k++) 
      eatom[k][i] = buf[m++];

  if (pressatom_flag)
    for (k = 0; k < nv; k++) {
      memcpy( vatom[k][i], &buf[m], six );
      m += 6;
    }

  return m;
}

/* ----------------------------------------------------------------------
   memory usage of local atom-based array
------------------------------------------------------------------------- */

double FixHMC::memory_usage()
{
  double bytes = nvalues * atom->nmax * sizeof(double);
  return bytes;
}

