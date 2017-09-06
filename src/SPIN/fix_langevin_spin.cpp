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
   Contributing authors: Julien Tranchida (SNL)
                         Aidan Thompson (SNL) 
------------------------------------------------------------------------- */

#include <mpi.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

#include "atom.h"
#include "atom_vec_ellipsoid.h"
#include "comm.h"
#include "compute.h"
#include "domain.h"
#include "error.h"
#include "fix_langevin_spin.h"
#include "force.h"
#include "group.h"
#include "input.h"
#include "math_const.h"
#include "math_extra.h"
#include "memory.h"
#include "modify.h"
#include "random_mars.h"
#include "random_park.h"
#include "region.h"
#include "respa.h"
#include "update.h"
#include "variable.h"

using namespace LAMMPS_NS;
using namespace FixConst;
using namespace MathConst;

/* ---------------------------------------------------------------------- */

FixLangevinSpin::FixLangevinSpin(LAMMPS *lmp, int narg, char **arg) :
  Fix(lmp, narg, arg), id_temp(NULL), random(NULL)
{
  if (narg != 7) error->all(FLERR,"Illegal langevin/spin command");

  dynamic_group_allow = 1;
  scalar_flag = 1;
  global_freq = 1;
  extscalar = 1;
  nevery = 1;

  temp = force->numeric(FLERR,arg[3]);
  alpha_t = force->numeric(FLERR,arg[4]);
  alpha_l = force->numeric(FLERR,arg[5]);
  seed = force->inumeric(FLERR,arg[6]);

  if (alpha_t < 0.0) {
    error->all(FLERR,"Illegal langevin/spin command");
  } else if (alpha_t == 0.0) {
    tdamp_flag = 0;
  } else {
    tdamp_flag = 1;
  }

  if (alpha_l < 0.0) {
    error->all(FLERR,"Illegal langevin/spin command");
  } else if (alpha_l == 0.0) {
    ldamp_flag = 0;
  } else {
    ldamp_flag = 1;
  }
  
  if (temp < 0.0) {
    error->all(FLERR,"Illegal langevin/spin command");
  } else if (temp == 0.0) {
    temp_flag = 0;
  } else {
    temp_flag = 1;
  }

  // initialize Marsaglia RNG with processor-unique seed
  //random = new RanMars(lmp,seed + comm->me);
  random = new RanPark(lmp,seed + comm->me);

}

/* ---------------------------------------------------------------------- */

FixLangevinSpin::~FixLangevinSpin()
{
  memory->destroy(spi);
  memory->destroy(fmi);
  delete random;
}

/* ---------------------------------------------------------------------- */

int FixLangevinSpin::setmask()
{
  int mask = 0;
  mask |= POST_FORCE;
  mask |= POST_FORCE_RESPA;
  mask |= END_OF_STEP;
  mask |= THERMO_ENERGY;
  return mask;
}

/* ---------------------------------------------------------------------- */

void FixLangevinSpin::init()
{
  // warn if any fix comes after this one  
  int after = 0;
  int flag_force = 0;
  int flag_lang = 0;
  for (int i = 0; i < modify->nfix; i++) { 
     if (strcmp("force/spin",modify->fix[i]->style)==0) flag_force = MAX(flag_force,i);
     if (strcmp("langevin/spin",modify->fix[i]->style)==0) flag_lang = i;
  }
  if (flag_force >= flag_lang) error->all(FLERR,"Fix langevin/spin should come after all other spin fixes");  

  memory->create(spi,3,"pair:spi");
  memory->create(fmi,3,"pair:fmi");

  dts = update->dt; 
  Gil_factor = 1.0/(1.0+(alpha_t)*(alpha_t));
  
  double hbar = force->hplanck/MY_2PI; //eV/(rad.THz)
  double kb = force->boltz;
  D = (MY_2PI*Gil_factor*kb*temp)/hbar/dts;
  sigma = sqrt(D);
}

/* ---------------------------------------------------------------------- */

void FixLangevinSpin::setup(int vflag)
{
  if (strstr(update->integrate_style,"verlet"))
    post_force(vflag);
  else {
    ((Respa *) update->integrate)->copy_flevel_f(nlevels_respa-1);
    post_force_respa(vflag,nlevels_respa-1,0);
    ((Respa *) update->integrate)->copy_f_flevel(nlevels_respa-1);
  }
}

/* ---------------------------------------------------------------------- */

void FixLangevinSpin::post_force(int vflag)
{
  double **sp = atom->sp;
  double **fm = atom->fm;
  int *mask = atom->mask;
  const int nlocal = atom->nlocal;  
              
  double sx, sy, sz;
  double fmx, fmy, fmz;
  double cpx, cpy, cpz;
  double rx, ry, rz;  
                          
  // add the damping to the effective field of each spin
  for (int i = 0; i < nlocal; i++) {
    if (mask[i] & groupbit) {
      spi[0] = sp[i][0];
      spi[1] = sp[i][1];
      spi[2] = sp[i][2];
 		
      fmi[0] = fm[i][0];
      fmi[1] = fm[i][1];
      fmi[2] = fm[i][2];

      if (tdamp_flag) {
        add_tdamping(spi,fmi);
      }

      if (temp_flag) {
        add_temperature(fmi);
      }
		
      fm[i][0] = fmi[0];
      fm[i][1] = fmi[1];
      fm[i][2] = fmi[2];
    }
  }

}

/* ---------------------------------------------------------------------- */
void FixLangevinSpin::add_tdamping(double *spi, double *fmi)
{
  double cpx = fmi[1]*spi[2] - fmi[2]*spi[1];
  double cpy = fmi[2]*spi[0] - fmi[0]*spi[2];
  double cpz = fmi[0]*spi[1] - fmi[1]*spi[0];
	
  // taking away the transverse damping 
  fmi[0] -= alpha_t*cpx;
  fmi[1] -= alpha_t*cpy;
  fmi[2] -= alpha_t*cpz;
}

/* ---------------------------------------------------------------------- */
void FixLangevinSpin::add_temperature(double *fmi) 
{
//#define GAUSSIAN_R
#if defined GAUSSIAN_R
  // drawing gausian random dist
  double rx = sigma*random->gaussian();
  double ry = sigma*random->gaussian();
  double rz = sigma*random->gaussian();
#else 
  double rx = sigma*(random->uniform() - 0.5);
  double ry = sigma*(random->uniform() - 0.5);
  double rz = sigma*(random->uniform() - 0.5);
#endif

  // adding the random field 
  fmi[0] += rx; 
  fmi[1] += ry;
  fmi[2] += rz;
               
  // adding Gilbert's prefactor 
  fmi[0] *= Gil_factor; 
  fmi[1] *= Gil_factor; 
  fmi[2] *= Gil_factor; 

}


/* ---------------------------------------------------------------------- */

void FixLangevinSpin::post_force_respa(int vflag, int ilevel, int iloop)
{
  if (ilevel == nlevels_respa-1) post_force(vflag);
}

